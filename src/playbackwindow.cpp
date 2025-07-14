#include "playbackwindow.h"

#if defined(GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#endif

PlaybackWindow::PlaybackWindow()
{
    set_title("Media Playback");
    set_default_size(640, 480);
    add_events(Gdk::KEY_PRESS_MASK);

    // Prepare both widgets but do not pack them both
    video_area.set_hexpand();
    video_area.set_vexpand();
    video_area.set_size_request(640, 480);

    slideshow_image.set_hexpand();
    slideshow_image.set_vexpand();
    slideshow_image.set(Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, 640, 480));

    slideshow_image.signal_size_allocate().connect(
        sigc::mem_fun(*this, &PlaybackWindow::on_image_size_allocate), false);

    video_container.set_hexpand(true);
    video_container.set_vexpand(true);
    video_container.set_size_request(640, 480);
    video_container.signal_size_allocate().connect(
        sigc::mem_fun(*this, &PlaybackWindow::on_video_container_resized));

    // Initially show fallback or empty image
    set_video_container_content(slideshow_image);

    add(video_container);
    video_container.show_all();

    signal_key_press_event().connect(sigc::mem_fun(*this, &PlaybackWindow::on_key_press));
}

void PlaybackWindow::set_fallback_image(const std::string& path)
{
    try {
        auto pixbuf = Gdk::Pixbuf::create_from_file(path);
		fallback_pixbuf_original = pixbuf;
		update_fallback_image_scaled();
        set_video_container_content(slideshow_image);
    } catch (...) {
        std::cerr << "Failed to load fallback image: " << path << std::endl;
    }
}

void PlaybackWindow::update_fallback_image_scaled()
{
    if (!fallback_pixbuf_original)
        return;

    int width = slideshow_image.get_allocated_width();
    int height = slideshow_image.get_allocated_height();

    if (width <= 0 || height <= 0)
        return;

    auto scaled = fallback_pixbuf_original->scale_simple(
        width, height, Gdk::INTERP_BILINEAR);
    
    slideshow_image.set(scaled);
}

void PlaybackWindow::on_image_size_allocate(Gtk::Allocation& allocation)
{
    update_fallback_image_scaled();
}

void PlaybackWindow::set_video_container_content(Gtk::Widget& widget)
{
    // Remove all children
    std::vector<Gtk::Widget*> to_remove;
    video_container.foreach([&](Gtk::Widget& child) {
        to_remove.push_back(&child);
    });
    for (auto* w : to_remove)
        video_container.remove(*w);

    video_container.pack_start(widget);
    widget.show();
}

void PlaybackWindow::on_video_container_resized(Gtk::Allocation& allocation)
{
    if (!current_pixbuf_original)
        return;

    auto scaled = current_pixbuf_original->scale_simple(
        allocation.get_width(),
        allocation.get_height(),
        Gdk::INTERP_BILINEAR
    );
    slideshow_image.set(scaled);
}

void PlaybackWindow::pause_audio() {
    if (playbin) gst_element_set_state(playbin, GST_STATE_PAUSED);
}

void PlaybackWindow::resume_audio() {
    if (playbin) gst_element_set_state(playbin, GST_STATE_PLAYING);
}

void PlaybackWindow::stop_audio() {
    if (playbin) {
        gst_element_set_state(playbin, GST_STATE_NULL);
        gst_object_unref(playbin);
        playbin = nullptr;
    }
}
void PlaybackWindow::audio_volume_down() {
    if (playbin) {
        gdouble volume;
        g_object_get(playbin, "volume", &volume, nullptr);
        volume -= 0.1;
        if (volume < 0.0) volume = 0.0;
        g_object_set(playbin, "volume", volume, nullptr);
    }
}

void PlaybackWindow::audio_volume_up() {
    if (playbin) {
        gdouble volume;
        g_object_get(playbin, "volume", &volume, nullptr);
        volume += 0.1;
        if (volume > 1.0) volume = 1.0;
        g_object_set(playbin, "volume", volume, nullptr);
    }
}

void PlaybackWindow::start_slideshow(const std::vector<std::string>& files, int slideshow_interval_seconds)
{
    if (files.empty())
        return;

    slideshow_files = files;
    slideshow_index = 0; // start from the beginning
    slideshow_playing = true;

    show_slide(slideshow_index);

    // start auto-advance every 5 seconds (or interval)
    if (slideshow_timer.connected())
        slideshow_timer.disconnect();
	std::cout << "Interval: " << slideshow_interval_seconds << std::endl;
	if (!slideshow_interval_seconds == 0){
		slideshow_timer = Glib::signal_timeout().connect(
		    sigc::mem_fun(*this, &PlaybackWindow::on_slideshow_tick),
		    slideshow_interval_seconds * 1000
		);
	}
}

void PlaybackWindow::show_slide(int index)
{
    if (index < 0 || index >= static_cast<int>(slideshow_files.size()))
        return;

    std::string filepath = slideshow_files[index];
    try {
        current_pixbuf_original = Gdk::Pixbuf::create_from_file(filepath);
        update_scaled_slide_image();
        set_video_container_content(slideshow_image);
    } catch (const Glib::FileError& e) {
        std::cerr << "File error: " << e.what() << std::endl;
    } catch (const Gdk::PixbufError& e) {
        std::cerr << "Pixbuf error: " << e.what() << std::endl;
    }
}

void PlaybackWindow::update_scaled_slide_image()
{
    if (!current_pixbuf_original)
        return;

    int width = slideshow_image.get_allocated_width();
    int height = slideshow_image.get_allocated_height();

	if (width <= 0 || height <= 0) {
	    std::cout << "Invalid image area size: " << width << "x" << height << std::endl;
    return;
	}
    auto scaled = current_pixbuf_original->scale_simple(
        width, height, Gdk::INTERP_BILINEAR);

    slideshow_image.set(scaled);
}

void PlaybackWindow::show_slide_file(const std::string& filepath)
{
    try {
        current_pixbuf_original = Gdk::Pixbuf::create_from_file(filepath);
        update_scaled_slide_image();
    } catch (const Glib::Error& e) {
        std::cerr << "Error loading slide " << filepath << ": " << e.what() << std::endl;
    }
}

bool PlaybackWindow::on_slideshow_tick()
{
    if (!slideshow_playing)
        return true;

    slideshow_index++;
    if (slideshow_index >= static_cast<int>(slideshow_files.size()))
        slideshow_index = 0;

    show_slide(slideshow_index);

    return true;
}

void PlaybackWindow::slideshow_next()
{
    slideshow_index++;
    if (slideshow_index >= static_cast<int>(slideshow_files.size()))
        slideshow_index = 0;
		std::cout << "Trying slide: " << slideshow_index << std::endl;
    show_slide(slideshow_index);
}

void PlaybackWindow::slideshow_prev()
{
    slideshow_index--;
    if (slideshow_index < 0)
        slideshow_index = slideshow_files.size() - 1;
		std::cout << "Trying slide: " << slideshow_index << std::endl;
    show_slide(slideshow_index);
}

void PlaybackWindow::slideshow_pause()
{
    slideshow_playing = false;
}

void PlaybackWindow::slideshow_resume()
{
    slideshow_playing = true;
}

void PlaybackWindow::slideshow_stop()
{
    if (slideshow_timer.connected())
        slideshow_timer.disconnect();
    slideshow_playing = false;

    image_scrolled.hide();
    video_container.show();
}

bool PlaybackWindow::on_key_press(GdkEventKey* key_event)
{
    if (key_event->keyval == GDK_KEY_f || key_event->keyval == GDK_KEY_F)
    {
        if (get_window()->get_state() & Gdk::WINDOW_STATE_FULLSCREEN)
            unfullscreen();
        else
            fullscreen();
        return true;
    }
    return false;
}
