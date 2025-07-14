#pragma once
#include <string>
#include <memory>
#include <glibmm/refptr.h>
#include <gst/gst.h>

// forward
class CueItem {
public:
    enum class Type {
        Audio,
        Video,
        Slideshow,
        Control
    };

    CueItem(Type type,
            const std::string& name,
            const std::string& path_or_command,
            int prewait,
            int postwait,
            int action_duration,
            bool auto_next,
            bool immediate_next,
			bool loop_forever,
            bool last_frame)

        : type(type),
          name(name),
          path_or_command(path_or_command),
          prewait(prewait),
          postwait(postwait),
          action_duration(action_duration),
          auto_next(auto_next),
          immediate_next(immediate_next),
		  loop_forever(loop_forever),
          last_frame(last_frame)
    {
        progress = 0.0;
        is_active = false;
    }

    Type type;
    std::string name;
    std::string path_or_command;
    int prewait;         // seconds
    int postwait;        // seconds
    int action_duration; // seconds
    bool auto_next;
    bool immediate_next;
    bool loop_forever;
    bool last_frame;
	std::vector<std::string> slideshow_images;
    double progress; // 0.0 to 1.0
    bool is_active;
	int slideshow_interval_seconds;
    // future: you could add a GstElement* here
    GstElement* gst_pipeline = nullptr;
};
