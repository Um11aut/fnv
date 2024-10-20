#include "screen_grabber.h"

#include <stdexcept>

ScreenGrabber::ScreenGrabber(AVFormatContext* ctx, const std::optional<const ScreenGrabberOpts>& opts)
	: options(nullptr), ctx_fmt(nullptr), input_fmt(nullptr)
{
	this->ctx_fmt = ctx;

	// Depends on the platform used
	std::string input_name = {};

#ifdef _WIN32
	input_name = "gdigrab";
#endif
	// TODO: add linux and mac support

	input_fmt = av_find_input_format(input_name.c_str());
	if (!input_fmt) {
		throw std::runtime_error(fmt::format("Failed to find input format: {}", input_name));
	}

	// Depends on the platform used
	std::string window_name;

#ifdef _WIN32
	window_name = "desktop";
#endif

	if (opts.has_value()) {
		window_name = opts.value().window_name;
	}

	if (uint32_t ret = avformat_open_input(&ctx_fmt, window_name.c_str(), input_fmt, &options); ret != 0) {
		char err_buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, err_buf, sizeof(err_buf));
		throw std::runtime_error(fmt::format("Failed to open input: {} (Error: {})", window_name, err_buf));
	}

	if (uint32_t ret = avformat_find_stream_info(ctx, &options); ret < 0) {
		char err_buf[AV_ERROR_MAX_STRING_SIZE];
		av_strerror(ret, err_buf, sizeof(err_buf));
		throw std::runtime_error(fmt::format("Failed to find stream info. Error: {}", err_buf).c_str());
	}

	video_stream_idx = -1;

	// Determine which video stream index is available
	for (uint32_t i = 0; i < ctx->nb_streams; i++) {
		if (ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_stream_idx = i;
			break;
		}
	}

	if (video_stream_idx == -1) {
		throw std::runtime_error("Failed to find video stream!");
	}

	if (opts.has_value()) {
		options = ScreenGrabber::get_dict_from_opts(opts.value());
	}
}

void ScreenGrabber::loop(const std::function<void(AVPacket&)>& handler)
{
	AVPacket packet;
	while (av_read_frame(ctx_fmt, &packet) >= 0) {
		if (packet.stream_index == video_stream_idx) {
			handler(packet);
		}

		av_packet_unref(&packet);
	}
}

AVDictionary* ScreenGrabber::get_dict_from_opts(const ScreenGrabberOpts& opts)
{
	AVDictionary* options = nullptr;
	av_dict_set(&options, "framerate", fmt::format("{}", opts.framerate).c_str(), 0);
	if (opts.window_size.has_value()) {
		auto& val = opts.window_size.value();

		// {1920, 1080} -> "1920x1080"
		av_dict_set(&options, "video_size", fmt::format("{}x{}", val.first, val.second).c_str(), 0);
	}
	return options;
}
