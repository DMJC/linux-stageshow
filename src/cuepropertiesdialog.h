#pragma once

#include <gtkmm.h>
#include <vector>
#include <string>

class CuePropertiesDialog : public Gtk::Dialog
{
public:

    enum class CueType {
        Audio,
        Video,
        Slideshow,
        Control
    };

    struct Result {
 	    std::string name;
        std::string file_or_command;
        std::vector<std::string> slideshow_files;
        int prewait_seconds;
        int postwait_seconds;
        bool immediate;
        bool auto_next;
        bool loop_forever;
        bool last_frame;
        int slideshow_interval_seconds = 0;  // default
    };

    CuePropertiesDialog(Gtk::Window& parent, CueType type);

    CueType cue_type;

    Gtk::Box* content_area;
	Gtk::Entry name_entry;
    // Pre/Post wait:
    Gtk::SpinButton spin_prewait_h, spin_prewait_m, spin_prewait_s;
    Gtk::SpinButton spin_postwait_h, spin_postwait_m, spin_postwait_s;

    // Checkboxes:
    Gtk::CheckButton check_immediate;
    Gtk::CheckButton check_auto_next;
    Gtk::CheckButton check_loop_forever;

    // Audio/Video:
    Gtk::FileChooserButton file_chooser;
    //Video
    Gtk::CheckButton last_frame;
    // Control:
    Gtk::Entry command_entry;

    // Slideshow:
    Gtk::Box slideshow_box {Gtk::ORIENTATION_HORIZONTAL};
    Gtk::Box slideshow_controls {Gtk::ORIENTATION_VERTICAL};
    Gtk::ScrolledWindow slideshow_scrolled;
    Gtk::TreeView slideshow_treeview;
    Glib::RefPtr<Gtk::ListStore> slideshow_store;
    Gtk::Button button_add_images {"Add Images"};
    Gtk::Button button_remove_image {"Remove Selected"};
    Gtk::SpinButton spin_slideshow_interval;

    bool run_and_get_result(Result& result);

    class SlideshowColumns : public Gtk::TreeModel::ColumnRecord {
    public:
        SlideshowColumns() { add(filepath); }
        Gtk::TreeModelColumn<Glib::ustring> filepath;
    };
    SlideshowColumns slideshow_columns;

private:

};

