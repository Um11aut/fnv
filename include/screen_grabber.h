#pragma once
#include "fnv.h"

#include <string_view>
#include <optional>

struct ScreenGrabberOpts {
	uint32_t framerate = 30;
	std::optional<std::pair<uint32_t, uint32_t>> window_size = std::nullopt;
	const char* window_name = "desktop";
};

// Crossplatform: Grabs a screen and provide function to handle frames in a loop 
class ScreenGrabber {
public:
	// win: dshow or gdigrab
	// x11 linux: x11grab
	// mac: avfoundation
	ScreenGrabber(AVFormatContext* ctx, const std::optional<const ScreenGrabberOpts>& opts);

	inline const uint32_t get_stream_idx() const { return video_stream_idx; }

	// While loop in which the frames are read
	void loop(const std::function<void(AVPacket&)>& handler);
private:
	static AVDictionary* get_dict_from_opts(const ScreenGrabberOpts& opts);

	uint32_t video_stream_idx;
	const AVInputFormat* input_fmt;
	AVDictionary* options;

	// shared FormatContext ptr
	AVFormatContext* ctx_fmt;
};