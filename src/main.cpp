#include <gtkmm.h>
#include <gst/gst.h>
#include <gdk/gdkx.h>
#include <gst/video/videooverlay.h>
#include <iostream>
#include <vector>
#include "countdownwindow.h"
#include "playlistwindow.h"
#include "playbackwindow.h"
#include "cuepropertiesdialog.h"
#include "cueitem.h"

// Forward declarations
class PlaybackWindow;
class PlaylistWindow;
class CountdownWindow;

int main(int argc, char* argv[])
{
    auto app = Gtk::Application::create(argc, argv, "org.media.cueplayer");
    gst_init(&argc, &argv);
    auto playback_win = std::make_shared<PlaybackWindow>();
    auto playlist_win = std::make_shared<PlaylistWindow>(playback_win);
    auto countdown_win = std::make_shared<CountdownWindow>();

    playlist_win->show_all();
    playback_win->show_all();
//    countdown_win->show_all();

    return app->run(*playlist_win);  // use playlist window as main
}
