#pragma once

#include <gtkmm.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <iostream>

class PlaybackWindow : public Gtk::Window
{
public:
    PlaybackWindow();
	void set_fallback_image(const std::string& path);

    void start_video(const std::string& filename);
	void start_slideshow(const std::vector<std::string>& files, int slideshow_interval_seconds);
	void set_video_container_content(Gtk::Widget& widget);
	void on_video_container_resized(Gtk::Allocation& allocation);

	void pause_video();
	void resume_video();
	void pause_audio();
	void resume_audio();
	void stop_playback();
	void audio_volume_down();
	void audio_volume_up();

	void show_slide_file(const std::string& filepath);
    void slideshow_next();
    void slideshow_prev();
    void slideshow_pause();
    void slideshow_resume();
	void slideshow_stop();
	void update_scaled_slide_image();
	void on_image_size_allocate(Gtk::Allocation& allocation);
	void update_fallback_image_scaled();
	GtkWidget* sink_widget = nullptr;
    Gtk::DrawingArea video_area;
protected:
    // video
    Gtk::Box video_container {Gtk::ORIENTATION_VERTICAL};
    GstElement* playbin = nullptr;

    // slideshow
    Gtk::ScrolledWindow image_scrolled;
    Gtk::Image slideshow_image;
    std::vector<std::string> slideshow_files;
    int slideshow_index = 0;
    bool slideshow_playing = false;
    sigc::connection slideshow_timer;
	Glib::RefPtr<Gdk::Pixbuf> fallback_pixbuf_original;
	Glib::RefPtr<Gdk::Pixbuf> current_pixbuf_original;
	std::string fallback_image_path = std::string(STAGESHOW_DATA_DIR) + "/images/fallback.png";

    // helpers
    void show_slide(int index);
    bool on_slideshow_tick();

    // fullscreen
    bool on_key_press(GdkEventKey* key_event);
};
