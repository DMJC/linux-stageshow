#pragma once

#include <gtkmm.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <memory>
#include "cueitem.h"
#include "playbackwindow.h"
#include "cuepropertiesdialog.h"

class PlaylistWindow : public Gtk::Window
{
public:
    explicit PlaylistWindow(std::shared_ptr<PlaybackWindow> playback_win);
    ~PlaylistWindow();

protected:
    // columns
    class CueColumns : public Gtk::TreeModel::ColumnRecord {
    public:
        CueColumns() {
			add(live);
            add(number);
            add(name);
            add(prewait);
            add(action_text);
            add(action_progress);
            add(postwait);
            add(cue_ptr);
        }
        Gtk::TreeModelColumn<Glib::ustring> live;
        Gtk::TreeModelColumn<int> number;
        Gtk::TreeModelColumn<Glib::ustring> name;
        Gtk::TreeModelColumn<Glib::ustring> prewait;
        Gtk::TreeModelColumn<Glib::ustring> action_text;
        Gtk::TreeModelColumn<int> action_progress;
        Gtk::TreeModelColumn<Glib::ustring> postwait;
		Gtk::TreeModelColumn<std::shared_ptr<CueItem>> cue_ptr;
    };
	PlaybackWindow pw;

    CueColumns cue_columns;
    Glib::RefPtr<Gtk::ListStore> cue_store;

    std::vector<std::shared_ptr<CueItem>> cue_items;
    std::shared_ptr<CueItem> active_cue;
    std::shared_ptr<PlaybackWindow> playback_window;

    // layout
    Gtk::Box vbox {Gtk::ORIENTATION_VERTICAL};
    Gtk::MenuBar menu_bar;
    Gtk::Paned paned {Gtk::ORIENTATION_HORIZONTAL};

    // left
    Gtk::Grid left_top_grid;
    Gtk::Button go_button {"GO"};
    Gtk::Label label_queued {"Queued: (none)"};
    Gtk::Label label_playing {"Playing: (none)"};
    Gtk::TreeView cue_treeview;

    // right
    Gtk::Grid global_control_grid;
    Gtk::Separator sep1;
    Gtk::Stack cue_controls_stack;
	std::map<std::shared_ptr<CueItem>, Gtk::Widget*> cue_control_boxes;
	Gtk::Box per_cue_controls_box {Gtk::ORIENTATION_VERTICAL};

    // global controls
    Gtk::Button button_global_play {"Play"};
    Gtk::Button button_global_pause {"Pause"};
    Gtk::Button button_global_remove {"Remove"};
    Gtk::Button button_global_stop {"Stop"};
    Gtk::Button button_global_fadeup {"Fade Up"};
    Gtk::Button button_global_fadedown {"Fade Down"};

    // right click menu
    Gtk::Menu right_click_menu;
    Gtk::MenuItem item_add_audio {"Add Audio Cue"};
    Gtk::MenuItem item_add_video {"Add Video Cue"};
    Gtk::MenuItem item_add_slideshow {"Add Slideshow Cue"};
    Gtk::MenuItem item_add_control {"Add Control Cue"};

    GstElement* gtk_sink = nullptr; // class member

	std::string fallback_image_path;
    // handlers
	void on_preferences_clicked();

    void on_go_clicked();
    void on_row_activated(const Gtk::TreeModel::Path&, Gtk::TreeViewColumn*);
	void on_rows_reordered(const Gtk::TreeModel::Path& path, const Gtk::TreeModel::iterator& iter, int* new_order);
    bool on_right_click(GdkEventButton*);
    bool on_treeview_key_press(GdkEventKey*);
    bool on_timeout();

    void on_global_play();
    void on_global_pause();
    void on_global_stop();
    void on_global_fadeup();
    void on_global_fadedown();

    void add_audio_cue();
    void add_video_cue();
    void add_slideshow_cue();
    void add_control_cue();
    void remove_cue(std::shared_ptr<CueItem> cue);
    
    static GstBusSyncReply bus_sync_handler(GstBus* bus, GstMessage* message, gpointer user_data);
	void on_cue_finished();
	int get_cue_index(const std::shared_ptr<CueItem>& cue) const;
    void set_active_cue(std::shared_ptr<CueItem>);
	std::string get_media_duration_hms(const std::string& filepath);
	std::string get_slideshow_duration_hms(const std::string& filepath, int seconds);
	void on_gst_message(GstMessage* msg);
	void show_fallback_image();
	void start_prewait_countdown(std::shared_ptr<CueItem> cue, Gtk::TreeModel::Row row, std::function<void()> on_complete);
	void start_postwait_countdown(std::shared_ptr<CueItem> cue, Gtk::TreeModel::Row row, std::function<void()> on_complete);
	void start_slideshow_cue(std::shared_ptr<CueItem> cue);
	void start_video_cue(std::shared_ptr<CueItem> cue);
    void start_audio_cue(std::shared_ptr<CueItem> cue);
    void start_command_cue(std::shared_ptr<CueItem>);
};

