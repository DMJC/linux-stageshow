#include "playlistwindow.h"
#if defined(GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#endif
#include "utils.h"
#include <iostream>
#include <gtkmm.h>
#include <gtkmm/application.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>

PlaylistWindow::PlaylistWindow(std::shared_ptr<PlaybackWindow> pw)
: playback_window(pw)
{
    set_title("Playlist Manager");
    set_default_size(1000, 700);

    // top-level grid
    auto main_grid = Gtk::make_managed<Gtk::Grid>();
    add(*main_grid);

    // menubar
    auto file_menuitem = Gtk::make_managed<Gtk::MenuItem>("File");
    auto file_menu = Gtk::make_managed<Gtk::Menu>();
    file_menuitem->set_submenu(*file_menu);
    menu_bar.append(*file_menuitem);

    auto preferences_item = Gtk::make_managed<Gtk::MenuItem>("Preferences");
    preferences_item->signal_activate().connect(sigc::mem_fun(*this, &PlaylistWindow::on_preferences_clicked));
    file_menu->append(*preferences_item);

    auto quit_item = Gtk::make_managed<Gtk::MenuItem>("Quit");
    quit_item->signal_activate().connect([](){
        Gtk::Application::get_default()->quit();
    });
    file_menu->append(*quit_item);

    menu_bar.show_all();

    main_grid->attach(menu_bar, 0, 0, 2, 1);

    // left side
    left_top_grid.set_row_spacing(3);
    left_top_grid.set_column_spacing(5);
    left_top_grid.attach(go_button, 0, 0, 1, 2);
    left_top_grid.attach(label_queued, 1, 0, 1, 1);
    left_top_grid.attach(label_playing, 1, 1, 1, 1);

    cue_store = Gtk::ListStore::create(cue_columns);
    cue_treeview.set_model(cue_store);
    cue_treeview.append_column("•", cue_columns.live);
    cue_treeview.append_column("#", cue_columns.number);
    cue_treeview.append_column("Cue Name", cue_columns.name);
    cue_treeview.append_column("Pre Wait", cue_columns.prewait);

    auto progress_column = Gtk::make_managed<Gtk::TreeViewColumn>("Action");
    auto cell_progress = Gtk::make_managed<Gtk::CellRendererProgress>();
    progress_column->pack_start(*cell_progress, true);
    progress_column->add_attribute(*cell_progress, "value", cue_columns.action_progress);
    progress_column->add_attribute(*cell_progress, "text", cue_columns.action_text);
	cue_treeview.set_reorderable(true);
    cue_treeview.append_column(*progress_column);
    cue_treeview.append_column("Post Wait", cue_columns.postwait);

    auto left_box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL);
    left_box->pack_start(left_top_grid, Gtk::PACK_SHRINK);
    left_box->pack_start(cue_treeview, Gtk::PACK_EXPAND_WIDGET);

    // right side
    global_control_grid.set_row_spacing(5);
    global_control_grid.set_column_spacing(5);
    global_control_grid.attach(button_global_pause,    0, 0, 1, 1);
    global_control_grid.attach(button_global_stop,     1, 0, 1, 1);
    global_control_grid.attach(button_global_remove,   2, 0, 1, 1);
    global_control_grid.attach(button_global_play,     0, 1, 1, 1);
    global_control_grid.attach(button_global_fadedown, 1, 1, 1, 1);
    global_control_grid.attach(button_global_fadeup,   2, 1, 1, 1);

    auto right_box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL);
    right_box->pack_start(global_control_grid, Gtk::PACK_SHRINK);
    right_box->pack_start(sep1, Gtk::PACK_SHRINK);
    right_box->pack_start(per_cue_controls_box, Gtk::PACK_EXPAND_WIDGET);

    // pack the paned
    paned.set_wide_handle(true);
    paned.set_hexpand(true);
    paned.set_vexpand(true);
    paned.add1(*left_box);
    paned.add2(*right_box);

    main_grid->attach(paned, 0, 1, 1, 1);

    // show status
    label_queued.set_text("Queued: (none)");
    label_playing.set_text("Playing: (none)");
    go_button.set_label("GO");
    go_button.set_size_request(-1, 80);
    go_button.override_font(Pango::FontDescription("Sans 64"));

    // context menu
    right_click_menu.append(item_add_audio);
    right_click_menu.append(item_add_video);
    right_click_menu.append(item_add_slideshow);
    right_click_menu.append(item_add_control);
    right_click_menu.show_all();

    item_add_audio.signal_activate().connect(sigc::mem_fun(*this, &PlaylistWindow::add_audio_cue));
    item_add_video.signal_activate().connect(sigc::mem_fun(*this, &PlaylistWindow::add_video_cue));
    item_add_slideshow.signal_activate().connect(sigc::mem_fun(*this, &PlaylistWindow::add_slideshow_cue));
    item_add_control.signal_activate().connect(sigc::mem_fun(*this, &PlaylistWindow::add_control_cue));

	cue_store->signal_rows_reordered().connect(sigc::mem_fun(*this, &PlaylistWindow::on_rows_reordered));

    cue_treeview.add_events(Gdk::KEY_PRESS_MASK | Gdk::BUTTON_PRESS_MASK);
    cue_treeview.signal_key_press_event().connect(sigc::mem_fun(*this, &PlaylistWindow::on_treeview_key_press), false);
    cue_treeview.signal_button_press_event().connect(sigc::mem_fun(*this, &PlaylistWindow::on_right_click), false);
    cue_treeview.signal_row_activated().connect(sigc::mem_fun(*this, &PlaylistWindow::on_row_activated));

    go_button.signal_clicked().connect(sigc::mem_fun(*this, &PlaylistWindow::on_go_clicked));
    Glib::signal_timeout().connect(sigc::mem_fun(*this, &PlaylistWindow::on_timeout), 500);

    show_all_children();
}


PlaylistWindow::~PlaylistWindow()
{
    for (auto& cue : cue_items)
    {
        if (cue->gst_pipeline)
        {
            gst_element_set_state(cue->gst_pipeline, GST_STATE_NULL);
            gst_object_unref(cue->gst_pipeline);
        }
    }
}

void PlaylistWindow::add_audio_cue()
{
    CuePropertiesDialog dlg(*this, CuePropertiesDialog::CueType::Audio);
    CuePropertiesDialog::Result res;
    if (dlg.run_and_get_result(res)) {
        auto cue = std::make_shared<CueItem>(
            CueItem::Type::Audio,
            res.name.empty() ? Glib::path_get_basename(res.file_or_command) : res.name,
            res.file_or_command,
            res.prewait_seconds,
            res.postwait_seconds,
            10,
            res.auto_next,
            res.immediate,
            res.loop_forever,
            res.last_frame
        );
        cue_items.push_back(cue);

        // --- Create control box for cue ---
        auto control_box = Gtk::make_managed<Gtk::Grid>();

        auto label = Gtk::make_managed<Gtk::Label>("Audio Cue: " + cue->name);
        auto but_pause_img = Gtk::make_managed<Gtk::Image>("images/pause.png");
        auto but_stop_img = Gtk::make_managed<Gtk::Image>("images/stop.png");
        auto but_vol_down_img = Gtk::make_managed<Gtk::Image>("images/fade_down.png");
        auto but_vol_up_img = Gtk::make_managed<Gtk::Image>("images/fade_up.png");
        auto but_remove_img = Gtk::make_managed<Gtk::Image>("images/close.png");

        auto button_playpause = Gtk::make_managed<Gtk::ToggleButton>();
        auto button_stop = Gtk::make_managed<Gtk::Button>();
        auto button_vol_down = Gtk::make_managed<Gtk::Button>();
        auto button_vol_up = Gtk::make_managed<Gtk::Button>();
        auto button_remove = Gtk::make_managed<Gtk::Button>();

        button_playpause->set_image(*but_pause_img);
        button_stop->set_image(*but_stop_img);
        button_vol_down->set_image(*but_vol_down_img);
        button_vol_up->set_image(*but_vol_up_img);
        button_remove->set_image(*but_remove_img);

        control_box->attach(*label,             0, 0, 3, 1);
        control_box->attach(*button_playpause,  0, 1, 1, 1);
        control_box->attach(*button_stop,       1, 1, 1, 1);
        control_box->attach(*button_remove,     2, 1, 1, 1);
        control_box->attach(*button_vol_down,   1, 2, 1, 1);
        control_box->attach(*button_vol_up,     2, 2, 1, 1);

        // --- Signal handlers ---
        button_playpause->signal_toggled().connect([this, cue, button_playpause]() {
            if (!cue->gst_pipeline)
                return;

            if (button_playpause->get_active()) {
                gst_element_set_state(cue->gst_pipeline, GST_STATE_PAUSED);
                button_playpause->set_image_from_icon_name("media-playback-start");
            } else {
                gst_element_set_state(cue->gst_pipeline, GST_STATE_PLAYING);
                button_playpause->set_image_from_icon_name("media-playback-pause");
            }
        });

        button_stop->signal_clicked().connect([cue]() {
            if (cue->gst_pipeline)
                gst_element_set_state(cue->gst_pipeline, GST_STATE_NULL);
        });

        button_vol_down->signal_clicked().connect([cue]() {
            Glib::signal_timeout().connect([cue]() -> bool {
                gdouble vol = 1.0;
                g_object_get(cue->gst_pipeline, "volume", &vol, nullptr);
                vol -= 0.05;
                if (vol <= 0.0) {
                    vol = 0.0;
                    g_object_set(cue->gst_pipeline, "volume", vol, nullptr);
                    return false;
                }
                g_object_set(cue->gst_pipeline, "volume", vol, nullptr);
                return true;
            }, 100);
        });

        button_vol_up->signal_clicked().connect([cue]() {
            Glib::signal_timeout().connect([cue]() -> bool {
                gdouble vol = 0.0;
                g_object_get(cue->gst_pipeline, "volume", &vol, nullptr);
                vol += 0.05;
                if (vol >= 1.0) {
                    vol = 1.0;
                    g_object_set(cue->gst_pipeline, "volume", vol, nullptr);
                    return false;
                }
                g_object_set(cue->gst_pipeline, "volume", vol, nullptr);
                return true;
            }, 100);
        });

        button_remove->signal_clicked().connect([this, cue]() {
            remove_cue(cue);
        });

        control_box->show_all();
        per_cue_controls_box.pack_start(*control_box, Gtk::PACK_SHRINK);
        cue_control_boxes[cue] = control_box;

        // Add to model
        auto row = *(cue_store->append());
        row[cue_columns.name] = cue->name;
        row[cue_columns.prewait] = Glib::ustring::format(res.prewait_seconds / 60, ":", res.prewait_seconds % 60);
        row[cue_columns.action_text] = get_media_duration_hms(cue->path_or_command);
        row[cue_columns.action_progress] = 0;
        row[cue_columns.postwait] = Glib::ustring::format(res.postwait_seconds / 60, ":", res.postwait_seconds % 60);
    }
}

void PlaylistWindow::add_video_cue()
{
    CuePropertiesDialog dlg(*this, CuePropertiesDialog::CueType::Video);
    CuePropertiesDialog::Result res;
    if (!dlg.run_and_get_result(res))
        return;

    auto cue = std::make_shared<CueItem>(
        CueItem::Type::Video,
        Glib::path_get_basename(res.file_or_command),
        res.file_or_command,
        res.prewait_seconds,
        res.postwait_seconds,
        10,
        res.auto_next,
        res.immediate,
        res.loop_forever,
        res.last_frame
    );
    cue_items.push_back(cue);

    // Create control box
    auto control_box = Gtk::make_managed<Gtk::Grid>();

    auto label = Gtk::make_managed<Gtk::Label>("Video Cue: " + cue->name);
    auto but_pause_img = Gtk::make_managed<Gtk::Image>("images/pause.png");
    auto but_stop_img = Gtk::make_managed<Gtk::Image>("images/stop.png");
    auto but_vol_down_img = Gtk::make_managed<Gtk::Image>("images/fade_down.png");
    auto but_vol_up_img = Gtk::make_managed<Gtk::Image>("images/fade_up.png");
    auto but_remove_img = Gtk::make_managed<Gtk::Image>("images/close.png");

    auto button_playpause = Gtk::make_managed<Gtk::ToggleButton>();
    auto button_stop = Gtk::make_managed<Gtk::Button>();
    auto button_vol_down = Gtk::make_managed<Gtk::Button>();
    auto button_vol_up = Gtk::make_managed<Gtk::Button>();
    auto button_remove = Gtk::make_managed<Gtk::Button>();

    button_playpause->set_image(*but_pause_img);
    button_stop->set_image(*but_stop_img);
    button_vol_down->set_image(*but_vol_down_img);
    button_vol_up->set_image(*but_vol_up_img);
    button_remove->set_image(*but_remove_img);

    control_box->attach(*label,             0, 0, 3, 1);
    control_box->attach(*button_playpause,  0, 1, 1, 1);
    control_box->attach(*button_stop,       1, 1, 1, 1);
    control_box->attach(*button_remove,     2, 1, 1, 1);
    control_box->attach(*button_vol_down,   1, 2, 1, 1);
    control_box->attach(*button_vol_up,     2, 2, 1, 1);

    // Signal handlers
    button_playpause->signal_toggled().connect([this, cue, button_playpause]() {
        if (!cue->gst_pipeline)
            return;

        if (button_playpause->get_active()) {
            gst_element_set_state(cue->gst_pipeline, GST_STATE_PAUSED);
            button_playpause->set_image_from_icon_name("media-playback-start");
        } else {
            gst_element_set_state(cue->gst_pipeline, GST_STATE_PLAYING);
            button_playpause->set_image_from_icon_name("media-playback-pause");
        }
    });

    button_stop->signal_clicked().connect([this]() {
        playback_window->stop_audio();  // This seems like a misnamed method
    });

    button_vol_down->signal_clicked().connect([cue]() {
        Glib::signal_timeout().connect([cue]() -> bool {
            gdouble vol = 1.0;
            g_object_get(cue->gst_pipeline, "volume", &vol, nullptr);
            vol = std::max(0.0, vol - 0.05);
            g_object_set(cue->gst_pipeline, "volume", vol, nullptr);
            return vol > 0.0;
        }, 100);
    });

    button_vol_up->signal_clicked().connect([cue]() {
        Glib::signal_timeout().connect([cue]() -> bool {
            gdouble vol = 0.0;
            g_object_get(cue->gst_pipeline, "volume", &vol, nullptr);
            vol = std::min(1.0, vol + 0.05);
            g_object_set(cue->gst_pipeline, "volume", vol, nullptr);
            return vol < 1.0;
        }, 100);
    });

    button_remove->signal_clicked().connect([this, cue]() {
        remove_cue(cue);
    });

    control_box->show_all();
    per_cue_controls_box.pack_start(*control_box, Gtk::PACK_SHRINK);
    cue_control_boxes[cue] = control_box;

    // Add to model
    auto row = *(cue_store->append());
    row[cue_columns.name] = cue->name;
    row[cue_columns.prewait] = Glib::ustring::format(res.prewait_seconds / 60, ":", res.prewait_seconds % 60);
    row[cue_columns.action_text] = get_media_duration_hms(cue->path_or_command);
    row[cue_columns.action_progress] = 0;
    row[cue_columns.postwait] = Glib::ustring::format(res.postwait_seconds / 60, ":", res.postwait_seconds % 60);
}

void PlaylistWindow::add_slideshow_cue()
{
    CuePropertiesDialog dlg(*this, CuePropertiesDialog::CueType::Slideshow);
    CuePropertiesDialog::Result res;
    if (!dlg.run_and_get_result(res))
        return;

    auto cue = std::make_shared<CueItem>(
        CueItem::Type::Slideshow,
        res.name.empty() ? "Slideshow" : res.name,
        res.slideshow_files.empty() ? "" : res.slideshow_files.front(),
        res.prewait_seconds,
        res.postwait_seconds,
        10,
        res.auto_next,
        res.immediate,
        res.loop_forever,
        res.last_frame
    );

    cue->slideshow_images = res.slideshow_files;
    cue_items.push_back(cue);

    // Create UI
    auto control_box = Gtk::make_managed<Gtk::Grid>();
    auto label = Gtk::make_managed<Gtk::Label>("Slideshow: " + cue->name);
    auto label_slide = Gtk::make_managed<Gtk::Label>();

    auto current_index = std::make_shared<int>(0);
    if (!cue->slideshow_images.empty())
        label_slide->set_text(cue->slideshow_images[*current_index]);
    else
        label_slide->set_text("(No slides)");

    // Images
    auto but_next_img = Gtk::make_managed<Gtk::Image>("images/fwd.png");
    auto but_play_img = Gtk::make_managed<Gtk::Image>("images/play.png");
    auto but_pause_img = Gtk::make_managed<Gtk::Image>("images/pause.png");
    auto but_prev_img = Gtk::make_managed<Gtk::Image>("images/prev.png");
    auto but_remove_img = Gtk::make_managed<Gtk::Image>("images/close.png");

    // Buttons
    auto button_next = Gtk::make_managed<Gtk::Button>();
    auto button_prev = Gtk::make_managed<Gtk::Button>();
    auto button_playpause = Gtk::make_managed<Gtk::ToggleButton>();
    auto button_remove = Gtk::make_managed<Gtk::Button>();

    button_next->set_image(*but_next_img);
    button_prev->set_image(*but_prev_img);
    button_playpause->set_image(*but_pause_img);
    button_remove->set_image(*but_remove_img);

    control_box->attach(*label,           0, 0, 4, 1);
    control_box->attach(*label_slide,     0, 1, 4, 1);
    control_box->attach(*button_prev,     0, 2, 1, 1);
    control_box->attach(*button_playpause,1, 2, 1, 1);
    control_box->attach(*button_next,     2, 2, 1, 1);
    control_box->attach(*button_remove,   3, 2, 1, 1);

    // Signals
    button_next->signal_clicked().connect([this, cue, current_index, label_slide]() {
        (*current_index)++;
        if (*current_index >= static_cast<int>(cue->slideshow_images.size()))
            *current_index = 0;

        auto new_file = cue->slideshow_images[*current_index];
        label_slide->set_text(new_file);
        playback_window->show_slide_file(new_file);
    });

    button_prev->signal_clicked().connect([this, cue, current_index, label_slide]() {
        (*current_index)--;
        if (*current_index < 0)
            *current_index = cue->slideshow_images.size() - 1;

        auto new_file = cue->slideshow_images[*current_index];
        label_slide->set_text(new_file);
        playback_window->show_slide_file(new_file);
    });

    button_playpause->signal_toggled().connect([this, button_playpause]() {
        if (button_playpause->get_active()) {
            playback_window->slideshow_pause();
            button_playpause->set_image_from_icon_name("media-playback-start");
        } else {
            playback_window->slideshow_resume();
            button_playpause->set_image_from_icon_name("media-playback-pause");
        }
    });

    button_remove->signal_clicked().connect([this, cue]() {
        remove_cue(cue);
    });

    control_box->show_all();
    per_cue_controls_box.pack_start(*control_box, Gtk::PACK_SHRINK);
    cue_control_boxes[cue] = control_box;

    // Add to model
    auto row = *(cue_store->append());
    row[cue_columns.name] = cue->name;
    row[cue_columns.prewait] = Glib::ustring::format(cue->prewait / 60, ":", cue->prewait % 60);
    row[cue_columns.action_text] = "Slideshow";
    row[cue_columns.action_progress] = 0;
    row[cue_columns.postwait] = Glib::ustring::format(cue->postwait / 60, ":", cue->postwait % 60);
}

void PlaylistWindow::add_control_cue() {
    Gtk::Dialog dialog("Add Control Cue", *this);
    Gtk::Entry command_entry;
    command_entry.set_placeholder_text("Enter Unix command");

    dialog.get_content_area()->pack_start(command_entry);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_OK", Gtk::RESPONSE_OK);

    dialog.show_all();

    auto response = dialog.run();

    if (response == Gtk::RESPONSE_OK) {
        std::string command = command_entry.get_text();

        auto cue = std::make_shared<CueItem>(
            CueItem::Type::Control,
            command.substr(0,20),
            command,
            0, 0, 0, false, false, false, false
        );

        cue_items.push_back(cue);

        auto row = *(cue_store->append());
        row[cue_columns.name] = cue->name;
        row[cue_columns.prewait] = "00:00";
        row[cue_columns.action_text] = "—";
        row[cue_columns.action_progress] = 0;
        row[cue_columns.postwait] = "00:00";
    }
}

void PlaylistWindow::on_rows_reordered(const Gtk::TreeModel::Path& path, const Gtk::TreeModel::iterator& iter, int* new_order)
{
    std::cout << "Rows reordered!" << std::endl;
    // Optionally resync the internal std::vector<std::shared_ptr<CueItem>> cues
    // with the new Gtk::ListStore order
}

void PlaylistWindow::on_go_clicked()
{
    auto selection = cue_treeview.get_selection();
    auto selected_iter = selection->get_selected();
    if (!selected_iter)
        return;

    int row_index = std::distance(cue_store->children().begin(), selected_iter);

    if (row_index >= 0 && row_index < static_cast<int>(cue_items.size()))
    {
        active_cue = cue_items[row_index];

        // highlight next
        auto next_iter = selected_iter;
        ++next_iter;
        if (next_iter != cue_store->children().end())
        {
            selection->select(next_iter);
            cue_treeview.scroll_to_row(cue_store->get_path(next_iter));
        }

        if (active_cue->type == CueItem::Type::Audio)
        {
            start_audio_cue(active_cue);
        }
        else if (active_cue->type == CueItem::Type::Video)
        {
            start_video_cue(active_cue);
        }
        else if (active_cue->type == CueItem::Type::Slideshow)
        {
            playback_window->start_slideshow(active_cue->slideshow_images, active_cue->slideshow_interval_seconds);
        	start_slideshow_cue(active_cue);
        }
        else if (active_cue->type == CueItem::Type::Control)
        {
            system(active_cue->path_or_command.c_str());
        }
    }
}

void PlaylistWindow::set_active_cue(std::shared_ptr<CueItem> cue) {
    active_cue = cue;
}

void PlaylistWindow::start_slideshow_cue(std::shared_ptr<CueItem> cue)
{
    if (!cue || cue->slideshow_images.empty())
        return;

    auto it = cue_control_boxes.find(cue);
    if (it != cue_control_boxes.end()) {
        auto name = "cue_" + std::to_string(reinterpret_cast<uintptr_t>(cue.get()));
        cue_controls_stack.set_visible_child(name);
    }

    auto first_image = cue->slideshow_images.front();

    if (cue->prewait > 0) {
        Glib::signal_timeout().connect_once(
            [this, first_image]() {
                playback_window->show_slide_file(first_image);
            },
            cue->prewait * 1000
        );
    } else {
        playback_window->show_slide_file(first_image);
    }

    std::cout << "Started slideshow cue: " << cue->name << "\n";
}

void PlaylistWindow::start_video_cue(std::shared_ptr<CueItem> cue)
{
    if (!cue) return;

    // Show control box
    auto it = cue_control_boxes.find(cue);
    if (it != cue_control_boxes.end()) {
        auto name = "cue_" + std::to_string(reinterpret_cast<uintptr_t>(cue.get()));
        cue_controls_stack.set_visible_child(name);
    }

    // Reset pipeline if needed
    if (cue->gst_pipeline) {
        gst_element_set_state(cue->gst_pipeline, GST_STATE_NULL);
        gst_object_unref(cue->gst_pipeline);
    }

    // Setup pipeline
    cue->gst_pipeline = gst_element_factory_make("playbin", nullptr);
    auto gtk_sink = gst_element_factory_make("gtksink", "gtksink");
    auto audio_sink = gst_element_factory_make("autoaudiosink", "audiosink");

    g_object_set(cue->gst_pipeline,
                 "video-sink", gtk_sink,
                 "audio-sink", audio_sink,
                 nullptr);

    // Extract GtkWidget from gtksink and embed
    GtkWidget* sink_widget = nullptr;
    g_object_get(gtk_sink, "widget", &sink_widget, nullptr);
    if (sink_widget) {
        auto cpp_widget = Glib::wrap(GTK_WIDGET(sink_widget));
        playback_window->set_video_container_content(*cpp_widget);
    } else {
        std::cerr << "gtksink widget is null!" << std::endl;
    }

    g_object_set(cue->gst_pipeline, "uri", ("file://" + cue->path_or_command).c_str(), nullptr);
    g_object_set(cue->gst_pipeline, "volume", 1.0, nullptr);

    // Hook up bus for EOS
    GstBus* bus = gst_element_get_bus(cue->gst_pipeline);
    gst_bus_add_signal_watch(bus);

    g_signal_connect(bus, "message", G_CALLBACK(+[](
        GstBus* bus, GstMessage* msg, gpointer user_data) {
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
            auto* self = static_cast<PlaylistWindow*>(user_data);
            Glib::signal_idle().connect_once([self]() {
                std::cout << "Setting Fallback Pic: " << self->fallback_image_path << std::endl;
                self->playback_window->set_fallback_image(self->fallback_image_path); // or use a member fallback_slide_path
                self->on_cue_finished();
            });
        }
    }), this);
    gst_object_unref(bus);

    // Play with optional delay
    if (cue->prewait > 0) {
        Glib::signal_timeout().connect_once(
            [cue]() {
                gst_element_set_state(cue->gst_pipeline, GST_STATE_PLAYING);
            },
            cue->prewait * 1000
        );
    } else {
        gst_element_set_state(cue->gst_pipeline, GST_STATE_PLAYING);
    }

    std::cout << "Started video cue: " << cue->path_or_command << "\n";
}

void PlaylistWindow::start_command_cue(std::shared_ptr<CueItem> cue) {
    if (!cue) return;
    // Run shell command using std::system (or async if desired)
    std::string command = cue->path_or_command;
    if (!command.empty()) {
        std::cerr << "Running command cue: " << command << std::endl;
        std::system(command.c_str());
    }
    // Trigger cue finished after postwait
    Glib::signal_timeout().connect_once(
        sigc::mem_fun(*this, &PlaylistWindow::on_cue_finished),
        cue->postwait * 1000
    );
}

void PlaylistWindow::start_audio_cue(std::shared_ptr<CueItem> cue)
{
    if (!cue)
        return;

    // Show existing controls
    auto it = cue_control_boxes.find(cue);
    if (it != cue_control_boxes.end()) {
        auto name = "cue_" + std::to_string(reinterpret_cast<uintptr_t>(cue.get()));
        cue_controls_stack.set_visible_child(name);
    }

    // Clean up old pipeline
    if (cue->gst_pipeline) {
        gst_element_set_state(cue->gst_pipeline, GST_STATE_NULL);
        gst_object_unref(cue->gst_pipeline);
    }

    // Create pipeline
    cue->gst_pipeline = gst_element_factory_make("playbin", nullptr);
    g_object_set(cue->gst_pipeline, "uri", ("file://" + cue->path_or_command).c_str(), nullptr);
    g_object_set(cue->gst_pipeline, "volume", 1.0, nullptr);

    // Handle prewait
    if (cue->prewait > 0) {
        Glib::signal_timeout().connect_once(
            [cue]() {
                gst_element_set_state(cue->gst_pipeline, GST_STATE_PLAYING);
            },
            cue->prewait * 1000
        );
    } else {
        gst_element_set_state(cue->gst_pipeline, GST_STATE_PLAYING);
    }

    // Setup bus for EOS
    GstBus* bus = gst_element_get_bus(cue->gst_pipeline);
    gst_bus_add_signal_watch(bus);

    g_signal_connect(bus, "message", G_CALLBACK(+[](
        GstBus* bus, GstMessage* msg, gpointer user_data) {
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
            auto* self = static_cast<PlaylistWindow*>(user_data);
            Glib::signal_idle().connect_once([self]() {
                self->on_cue_finished();
            });
        }
    }), this);
    gst_object_unref(bus);

    std::cout << "Started audio cue: " << cue->path_or_command << "\n";
}

void PlaylistWindow::remove_cue(std::shared_ptr<CueItem> cue)
{
    if (!cue) return;

    // 1. stop playback if active
    if (cue->gst_pipeline) {
        gst_element_set_state(cue->gst_pipeline, GST_STATE_NULL);
        gst_object_unref(cue->gst_pipeline);
        cue->gst_pipeline = nullptr;
    }

    // 2. remove its controls from per_cue_controls_box
    auto it = cue_control_boxes.find(cue);
    if (it != cue_control_boxes.end()) {
        per_cue_controls_box.remove(*it->second);
        cue_control_boxes.erase(it);
    }

    // 3. remove from cue_items and model
    auto iter = std::find(cue_items.begin(), cue_items.end(), cue);
    if (iter != cue_items.end()) {
        int index = std::distance(cue_items.begin(), iter);
        cue_items.erase(iter);

        auto store_iter = cue_store->children().begin();
        std::advance(store_iter, index);
        if (store_iter != cue_store->children().end())
            cue_store->erase(store_iter);
    }

    if (active_cue == cue)
        active_cue.reset();
}

bool PlaylistWindow::on_treeview_key_press(GdkEventKey* event)
{
    if (event->keyval == GDK_KEY_Delete)
    {
        auto selection = cue_treeview.get_selection();
        if (auto iter = selection->get_selected())
        {
            int index = std::distance(cue_store->children().begin(), iter);
            if (index >= 0 && index < static_cast<int>(cue_items.size()))
            {
                auto cue = cue_items[index];
                remove_cue(cue);
            }
        }
        return true;
    }
    else if (event->keyval == GDK_KEY_space)
    {
        auto selection = cue_treeview.get_selection();
        if (auto iter = selection->get_selected())
        {
            int index = std::distance(cue_store->children().begin(), iter);
            if (index >= 0 && index < static_cast<int>(cue_items.size()))
            {
                active_cue = cue_items[index];

                if (active_cue->type == CueItem::Type::Audio)
                {
                    start_audio_cue(active_cue);
                }
                else if (active_cue->type == CueItem::Type::Video)
                {
                    start_video_cue(active_cue);
                }
                else if (active_cue->type == CueItem::Type::Slideshow)
                {
                    playback_window->start_slideshow(active_cue->slideshow_images, active_cue->slideshow_interval_seconds);
                    start_slideshow_cue(active_cue);
                }
                else if (active_cue->type == CueItem::Type::Control)
                {
                    system(active_cue->path_or_command.c_str());
                }

                // highlight next
                ++iter;
                if (iter != cue_store->children().end())
                {
                    selection->select(iter);
                    cue_treeview.scroll_to_row(cue_store->get_path(iter));
                }
            }
        }
        return true;
    }
    return false;
}

void PlaylistWindow::on_row_activated(const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn*)
{
    auto iter = cue_store->get_iter(path);
    if (!iter) return;

    int index = std::distance(cue_store->children().begin(), iter);
    if (index < 0 || index >= static_cast<int>(cue_items.size()))
        return;

    auto cue = cue_items[index];

    CuePropertiesDialog::CueType type;
    switch (cue->type)
    {
        case CueItem::Type::Audio: type = CuePropertiesDialog::CueType::Audio; break;
        case CueItem::Type::Video: type = CuePropertiesDialog::CueType::Video; break;
        case CueItem::Type::Slideshow: type = CuePropertiesDialog::CueType::Slideshow; break;
        case CueItem::Type::Control: type = CuePropertiesDialog::CueType::Control; break;
        default: return;
    }

    CuePropertiesDialog dlg(*this, type);
    CuePropertiesDialog::Result res;

    dlg.name_entry.set_text(cue->name);

    res.file_or_command = cue->path_or_command;
    res.slideshow_files.clear();
    if (cue->type == CueItem::Type::Slideshow)
        res.slideshow_files.push_back(cue->path_or_command);

    res.prewait_seconds = cue->prewait;
    res.postwait_seconds = cue->postwait;
    res.immediate = cue->immediate_next;
    res.auto_next = cue->auto_next;

    dlg.spin_prewait_h.set_value(cue->prewait / 3600);
    dlg.spin_prewait_m.set_value((cue->prewait % 3600) / 60);
    dlg.spin_prewait_s.set_value(cue->prewait % 60);

    dlg.spin_postwait_h.set_value(cue->postwait / 3600);
    dlg.spin_postwait_m.set_value((cue->postwait % 3600) / 60);
    dlg.spin_postwait_s.set_value(cue->postwait % 60);

    dlg.check_immediate.set_active(cue->immediate_next);
    dlg.check_auto_next.set_active(cue->auto_next);

    if (type == CuePropertiesDialog::CueType::Control)
    {
        dlg.command_entry.set_text(cue->path_or_command);
    }
    else if (type == CuePropertiesDialog::CueType::Audio || type == CuePropertiesDialog::CueType::Video)
    {
        dlg.file_chooser.set_filename(cue->path_or_command);
    }
    else if (type == CuePropertiesDialog::CueType::Slideshow)
    {
        dlg.slideshow_store->clear();
    
        for (const auto& img : cue->slideshow_images)
        {
            auto row = *(dlg.slideshow_store->append());
            row[dlg.slideshow_columns.filepath] = img;
        }
        dlg.spin_slideshow_interval.set_value(cue->slideshow_interval_seconds);
    }
    if (dlg.run_and_get_result(res))
    {
        cue->prewait = res.prewait_seconds;
        cue->postwait = res.postwait_seconds;
        cue->immediate_next = res.immediate;
        cue->auto_next = res.auto_next;
        cue->path_or_command = res.file_or_command;
        cue->name = res.name.empty() ? Glib::path_get_basename(res.file_or_command) : res.name;
        // store the first slide as path_or_command for preview in the listview
         cue->slideshow_images = res.slideshow_files;
        if (!res.slideshow_files.empty())
            cue->path_or_command = res.slideshow_files[0];

        cue->slideshow_interval_seconds = res.slideshow_interval_seconds;
        (*iter)[cue_columns.name] = cue->name;
        (*iter)[cue_columns.prewait] = Glib::ustring::format(cue->prewait / 60, ":", cue->prewait % 60);
        (*iter)[cue_columns.postwait] = Glib::ustring::format(cue->postwait / 60, ":", cue->postwait % 60);
    }
}

bool PlaylistWindow::on_right_click(GdkEventButton* event)
{
    if (event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
        right_click_menu.popup_at_pointer(reinterpret_cast<GdkEvent*>(event));
        return true;
    }
    return false;
}

int PlaylistWindow::get_cue_index(const std::shared_ptr<CueItem>& cue) const
{
    for (size_t i = 0; i < cue_items.size(); ++i) {
        if (cue_items[i] == cue)
            return static_cast<int>(i);
    }
    return -1;
}

void PlaylistWindow::on_cue_finished()
{
    if (!active_cue || !active_cue->auto_next)
        return;

    int current_index = get_cue_index(active_cue);
    if (current_index < 0 || current_index + 1 >= static_cast<int>(cue_items.size()))
        return;

    auto next_cue = cue_items[current_index + 1];

	if (!next_cue)
		return;

    set_active_cue(next_cue); // highlight it in the list

    switch (next_cue->type) {
		case CueItem::Type::Video:
            start_video_cue(next_cue);
            break;
        case CueItem::Type::Audio:
            start_audio_cue(next_cue);
            break;
        case CueItem::Type::Slideshow:
          	start_slideshow_cue(next_cue);
           	break;
        case CueItem::Type::Control:
           	start_command_cue(next_cue);
        	break;
        default:
			show_fallback_image();
            std::cerr << "Unhandled cue type in on_cue_finished()" << std::endl;
        	break;
    }
}

bool PlaylistWindow::on_timeout()
{
    for (auto row : cue_store->children())
    {
        if (active_cue && row[cue_columns.name] == active_cue->name && active_cue->gst_pipeline)
        {
			row[cue_columns.live] = "•";
            gint64 pos = 0, dur = 0;
            if (gst_element_query_position(active_cue->gst_pipeline, GST_FORMAT_TIME, &pos) &&
                gst_element_query_duration(active_cue->gst_pipeline, GST_FORMAT_TIME, &dur))
            {
                int pos_sec = pos / GST_SECOND;
                int dur_sec = dur / GST_SECOND;
                if (dur_sec > 0)
                {
                    int percent = 100 * pos_sec / dur_sec;
                    row[cue_columns.action_progress] = percent;
                    int remaining = dur_sec - pos_sec;
                    row[cue_columns.action_text] = format_seconds_to_hhmmss(remaining);
                }
            }
        }
    }
    return true;
}

void PlaylistWindow::on_global_play()
{
    if (active_cue && active_cue->gst_pipeline)
        gst_element_set_state(active_cue->gst_pipeline, GST_STATE_PLAYING);
}
void PlaylistWindow::on_global_pause()
{
    if (active_cue && active_cue->gst_pipeline)
        gst_element_set_state(active_cue->gst_pipeline, GST_STATE_PAUSED);
}
void PlaylistWindow::on_global_stop()
{
    if (active_cue && active_cue->gst_pipeline)
        gst_element_set_state(active_cue->gst_pipeline, GST_STATE_NULL);
}

GstBusSyncReply PlaylistWindow::bus_sync_handler(GstBus* /*bus*/, GstMessage* message, gpointer user_data)
{
    auto self = static_cast<PlaybackWindow*>(user_data);
    if (gst_is_video_overlay_prepare_window_handle_message(message)) {
        GstVideoOverlay* overlay = GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(message));

        auto gdk_window = self->video_area.get_window();
		if (!gdk_window) {
    		std::cerr << "No Gdk::Window found for video_area" << std::endl;
    		return GST_BUS_DROP;
		}

#if defined(GDK_WINDOWING_X11)
        if (GDK_IS_X11_WINDOW(gdk_window->gobj())) {
            ::Window xid = GDK_WINDOW_XID(gdk_window->gobj());
            gst_video_overlay_set_window_handle(overlay, (guintptr)xid);
		    std::cerr << "X11 Window found for video_area" << std::endl;
        }
#elif defined(GDK_WINDOWING_WAYLAND)
        if (GDK_IS_WAYLAND_WINDOW(gdk_window->gobj())) {
            struct wl_surface* surface = gdk_wayland_window_get_wl_surface(gdk_window->gobj());
            gst_video_overlay_set_window_handle(overlay, reinterpret_cast<guintptr>(surface));
		    std::cerr << "Wayland Window found for video_area" << std::endl;
        }
#endif
    }

    return GST_BUS_PASS;
}

void PlaylistWindow::start_prewait_countdown(std::shared_ptr<CueItem> cue,
                                             Gtk::TreeModel::Row row,
                                             std::function<void()> on_complete)
{
    int* remaining = new int(cue->prewait);
    row[cue_columns.prewait] = std::to_string(*remaining) + "s";

    Glib::signal_timeout().connect_seconds([remaining, row, this, on_complete]() mutable -> bool {
        (*remaining)--;

        if (*remaining <= 0) {
            row[cue_columns.prewait] = "0s";
            delete remaining;
            on_complete();  // proceed to cue playback
            return false;   // stop timeout
        } else {
            row[cue_columns.prewait] = std::to_string(*remaining) + "s";
            return true;    // continue countdown
        }
    }, 1);
}

void PlaylistWindow::start_postwait_countdown(std::shared_ptr<CueItem> cue,
                                             Gtk::TreeModel::Row row,
                                             std::function<void()> on_complete)
{
    int* remaining = new int(cue->postwait);
    row[cue_columns.postwait] = std::to_string(*remaining) + "s";

    Glib::signal_timeout().connect_seconds([remaining, row, this, on_complete]() mutable -> bool {
        (*remaining)--;

        if (*remaining <= 0) {
            row[cue_columns.prewait] = "0s";
            delete remaining;
            on_complete();  // proceed to cue playback
            return false;   // stop timeout
        } else {
            row[cue_columns.prewait] = std::to_string(*remaining) + "s";
            return true;    // continue countdown
        }
    }, 1);
}

void PlaylistWindow::on_global_fadeup()
{
    if (active_cue && active_cue->gst_pipeline)
    {
        gdouble volume = 1.0;
        g_object_get(active_cue->gst_pipeline, "volume", &volume, nullptr);
        volume += 0.1;
        if (volume > 1.0) volume = 1.0;
        g_object_set(active_cue->gst_pipeline, "volume", volume, nullptr);
    }
}
void PlaylistWindow::on_global_fadedown()
{
    if (active_cue && active_cue->gst_pipeline)
    {
        gdouble volume = 1.0;
        g_object_get(active_cue->gst_pipeline, "volume", &volume, nullptr);
        volume -= 0.1;
        if (volume < 0.0) volume = 0.0;
        g_object_set(active_cue->gst_pipeline, "volume", volume, nullptr);
    }
}

void PlaylistWindow::on_gst_message(GstMessage* msg)
{
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS:
    case GST_MESSAGE_ERROR:
        // Show fallback image
        show_fallback_image();
        break;
    default:
        break;
    }
}

std::string PlaylistWindow::get_slideshow_duration_hms(const std::string& filepath, int seconds) {

}

std::string PlaylistWindow::get_media_duration_hms(const std::string& filepath) {
    GstElement* pipeline = gst_element_factory_make("playbin", nullptr);
    if (!pipeline)
        return "(Error)";

    std::string uri = "file://" + filepath;
    g_object_set(pipeline, "uri", uri.c_str(), nullptr);

    gst_element_set_state(pipeline, GST_STATE_PAUSED);

    GstStateChangeReturn ret = gst_element_get_state(pipeline, nullptr, nullptr, 5 * GST_SECOND);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        return "(Error)";
    }

    gint64 duration_ns = GST_CLOCK_TIME_NONE;
    if (!gst_element_query_duration(pipeline, GST_FORMAT_TIME, &duration_ns)) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        return "(Unknown)";
    }

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    int total_seconds = static_cast<int>(duration_ns / GST_SECOND);
    int h = total_seconds / 3600;
    int m = (total_seconds % 3600) / 60;
    int s = total_seconds % 60;

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << h << ":"
        << std::setw(2) << m << ":"
        << std::setw(2) << s;

    return oss.str();
}

void PlaylistWindow::show_fallback_image()
{
	std::cout << "Fallback image" << std::endl; 
    pw.video_area.hide();         // Or stop drawing video to this area
    playback_window->show_slide_file("fallback.png");
}

void PlaylistWindow::on_preferences_clicked()
{
    Gtk::Dialog dialog("Preferences", *this);
    dialog.set_modal(true);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_OK", Gtk::RESPONSE_OK);

    auto content = dialog.get_content_area();

    auto box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_HORIZONTAL, 10);
    auto label = Gtk::make_managed<Gtk::Label>("Fallback image:");
    auto filename_label = Gtk::make_managed<Gtk::Label>(fallback_image_path.empty() ? "(none)" : fallback_image_path);
    auto choose_button = Gtk::make_managed<Gtk::Button>("Choose...");

    box->pack_start(*label, Gtk::PACK_SHRINK);
    box->pack_start(*filename_label, Gtk::PACK_EXPAND_WIDGET);
    box->pack_start(*choose_button, Gtk::PACK_SHRINK);

    choose_button->signal_clicked().connect([this, filename_label]() {
        Gtk::FileChooserDialog chooser("Select fallback image", Gtk::FILE_CHOOSER_ACTION_OPEN);
        chooser.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
        chooser.add_button("_Open", Gtk::RESPONSE_OK);

        auto filter = Gtk::FileFilter::create();
        filter->add_pixbuf_formats();
        filter->set_name("Image files");
        chooser.add_filter(filter);

        int result = chooser.run();
        if (result == Gtk::RESPONSE_OK)
        {
            fallback_image_path = chooser.get_filename();
            filename_label->set_text(fallback_image_path);
            playback_window->set_fallback_image(fallback_image_path);
        }
    });

    content->pack_start(*box);
    dialog.show_all();
    dialog.run();
}
