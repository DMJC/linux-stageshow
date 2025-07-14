#pragma once
class CountdownWindow : public Gtk::Window {
public:
    CountdownWindow() {
        set_title("Countdown Clock");
        set_default_size(200, 100);

        clock_label.set_text("00:00:00");
        clock_label.set_hexpand();
        clock_label.set_vexpand();
        clock_label.set_justify(Gtk::JUSTIFY_CENTER);
        clock_label.set_halign(Gtk::ALIGN_CENTER);
        clock_label.set_valign(Gtk::ALIGN_CENTER);
        clock_label.set_name("countdown");

        add(clock_label);
        show_all();
    }

    void set_time(const std::string& time_str) {
        clock_label.set_text(time_str);
    }

protected:
    Gtk::Label clock_label;
};
