#pragma once

#include "util.h"
#include <array>
#include <future>

// Priorities for NVENC
// Not all Nvidia GPUs support e.g. AV1 codec, so this struct will find any capable nvenc codec
static const std::array<const char*, 3> NVENCCodecPriorities = {
	"av1_nvenc",
	"hevc_nvenc",
	"h264_nvenc"
};

struct NVEncoderOpts {
	const char* preset = "fast";
	const char* profile = "main";
	const char* gpu = "0";
	const int fps = 30;
};

// Only works on nvidia rtx 30 or 40xx
class NVEncoder {
public:
	NVEncoder(AVFormatContext* ctx, const uint32_t& width, const uint32_t& height, const NVEncoderOpts& opts = {});
	~NVEncoder();

	void async_send_frame(AVFrame* frame);
	void await_receive_frame(const std::function<void(AVPacket*)>& handle_func);

	void send_receive_frame(AVFrame* frame, AVPacket& packet, const std::function<void()>& handle_func);

	inline const uint32_t get_width() const { return encoder_ctx->width; }
	inline const uint32_t get_height() const { return encoder_ctx->height; }
private:
	static const AVCodec* pick_available_encoder();

	// global context
	AVFormatContext* ctx_fmt;

	const AVCodec* encoder;
	AVCodecContext* encoder_ctx;

	std::future<int> fut;
};