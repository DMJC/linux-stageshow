#include "cuepropertiesdialog.h"

CuePropertiesDialog::CuePropertiesDialog(Gtk::Window& parent, CueType type)
    : Gtk::Dialog("Cue Properties", parent, true),
      file_chooser("Select File", Gtk::FILE_CHOOSER_ACTION_OPEN)
{
    cue_type = type;
    content_area = get_content_area();

    auto name_box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_HORIZONTAL, 5);
    name_box->pack_start(*Gtk::make_managed<Gtk::Label>("Cue Name:"), Gtk::PACK_SHRINK);
    name_box->pack_start(name_entry);

    content_area->pack_start(*name_box, Gtk::PACK_SHRINK);

    // Grid for times and checkboxes
    auto grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_row_spacing(5);
    grid->set_column_spacing(5);

    // Pre-wait
    grid->attach(*Gtk::make_managed<Gtk::Label>("Pre-wait (hh:mm:ss):"), 0, 0, 1, 1);
    spin_prewait_h.set_range(0, 23);
    spin_prewait_m.set_range(0, 59);
    spin_prewait_s.set_range(0, 59);
    grid->attach(spin_prewait_h, 1, 0, 1, 1);
    grid->attach(spin_prewait_m, 2, 0, 1, 1);
    grid->attach(spin_prewait_s, 3, 0, 1, 1);

    // Post-wait
    grid->attach(*Gtk::make_managed<Gtk::Label>("Post-wait (hh:mm:ss):"), 0, 1, 1, 1);
    spin_postwait_h.set_range(0, 23);
    spin_postwait_m.set_range(0, 59);
    spin_postwait_s.set_range(0, 59);
    grid->attach(spin_postwait_h, 1, 1, 1, 1);
    grid->attach(spin_postwait_m, 2, 1, 1, 1);
    grid->attach(spin_postwait_s, 3, 1, 1, 1);

    // Checkboxes
    check_immediate.set_label("Start immediately");
    check_auto_next.set_label("Auto start next after completion");
    check_loop_forever.set_label("Repeat on Loop Forever");
    grid->attach(check_immediate, 0, 2, 2, 1);
    grid->attach(check_auto_next, 2, 2, 2, 1);
    grid->attach(check_loop_forever, 0, 3, 2, 1);
    if (cue_type == CueType::Video) {
        last_frame.set_label("Keep showing last Frame");
        grid->attach(last_frame, 0, 4, 2, 1);
    }
    content_area->pack_start(*grid, Gtk::PACK_SHRINK);

    // cue-type-specific
    if (cue_type == CueType::Audio || cue_type == CueType::Video) {
        content_area->pack_start(file_chooser, Gtk::PACK_SHRINK);
    } else if (cue_type == CueType::Control) {
        auto command_box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_HORIZONTAL, 5);
        command_entry.set_placeholder_text("Enter Unix command");
        command_box->pack_start(*grid, Gtk::PACK_SHRINK);
        command_box->pack_start(command_entry, Gtk::PACK_SHRINK);
        content_area->pack_start(*command_box, Gtk::PACK_SHRINK);

    } else if (cue_type == CueType::Slideshow) {
        // Slideshow list
        slideshow_store = Gtk::ListStore::create(slideshow_columns);
        slideshow_treeview.set_model(slideshow_store);
        slideshow_treeview.append_column("Image", slideshow_columns.filepath);
    
        slideshow_scrolled.add(slideshow_treeview);
        slideshow_scrolled.set_min_content_width(300);    
        slideshow_scrolled.set_min_content_height(200);
    
        // drag/drop
        slideshow_treeview.enable_model_drag_source();
        slideshow_treeview.enable_model_drag_dest();

        button_add_images.signal_clicked().connect([this]() {
        Gtk::FileChooserDialog chooser(*this, "Select Images", Gtk::FILE_CHOOSER_ACTION_OPEN);
        chooser.set_select_multiple(true);
        chooser.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
        chooser.add_button("_Open", Gtk::RESPONSE_OK);
            if (chooser.run() == Gtk::RESPONSE_OK) {
                for (auto& filename : chooser.get_filenames()) {
                    auto row = *(slideshow_store->append());
                    row[slideshow_columns.filepath] = filename;
                }
            }
        });
    
        button_remove_image.signal_clicked().connect([this]() {
        auto selection = slideshow_treeview.get_selection();
        if (auto iter = selection->get_selected()) {
            slideshow_store->erase(iter);
        }
    });

    slideshow_treeview.add_events(Gdk::KEY_PRESS_MASK);
    slideshow_treeview.signal_key_press_event().connect([this](GdkEventKey* key) {
        if (key->keyval == GDK_KEY_Delete) {
            auto selection = slideshow_treeview.get_selection();
            if (auto iter = selection->get_selected()) {
                slideshow_store->erase(iter);
            }
            return true;
        }
        return false;
        }, false);
    
        slideshow_controls.pack_start(button_add_images, Gtk::PACK_SHRINK);
        slideshow_controls.pack_start(button_remove_image, Gtk::PACK_SHRINK);
    
        // interval spinbutton
        spin_slideshow_interval.set_range(0, 3600);
        spin_slideshow_interval.set_increments(1, 5);
        spin_slideshow_interval.set_value(0);
        spin_slideshow_interval.set_tooltip_text("Slide interval (seconds)");
    
        auto interval_label = Gtk::make_managed<Gtk::Label>("Interval (sec):");
        auto interval_box = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_HORIZONTAL, 5);
        interval_box->pack_start(*interval_label, Gtk::PACK_SHRINK);
        interval_box->pack_start(spin_slideshow_interval, Gtk::PACK_SHRINK);
    
        // layout
        slideshow_box.pack_start(slideshow_scrolled);
        slideshow_box.pack_start(slideshow_controls, Gtk::PACK_SHRINK);
        slideshow_box.pack_start(*interval_box, Gtk::PACK_SHRINK);
    
        content_area->pack_start(slideshow_box);
    }
    add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    add_button("_OK", Gtk::RESPONSE_OK);

    show_all();
}

bool CuePropertiesDialog::run_and_get_result(CuePropertiesDialog::Result& result)
{
    int response = run();
    if (response == Gtk::RESPONSE_OK) {
        result.name = name_entry.get_text();
        result.prewait_seconds = spin_prewait_h.get_value_as_int() * 3600
                               + spin_prewait_m.get_value_as_int() * 60
                               + spin_prewait_s.get_value_as_int();
        result.postwait_seconds = spin_postwait_h.get_value_as_int() * 3600
                                + spin_postwait_m.get_value_as_int() * 60
                                + spin_postwait_s.get_value_as_int();
        result.immediate = check_immediate.get_active();
        result.auto_next = check_auto_next.get_active();
        result.slideshow_interval_seconds = spin_slideshow_interval.get_value_as_int();
        if (cue_type == CueType::Control) {
            result.file_or_command = command_entry.get_text();
        } else if (cue_type == CueType::Slideshow) {
            result.slideshow_files.clear();
            for (auto& row : slideshow_store->children()) {
                Glib::ustring value = row[slideshow_columns.filepath];
                result.slideshow_files.push_back(value.raw());
            }
            if (!result.slideshow_files.empty())
                result.file_or_command = result.slideshow_files.front();
        } else {
            result.file_or_command = file_chooser.get_filename();
        }
        return true;
    }
    return false;
}

