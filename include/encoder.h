#pragma once

#include "util.h"
#include <array>

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

private:
	static const AVCodec* pick_available_encoder();

	// global context
	AVFormatContext* ctx_fmt;
	const AVCodec* encoder;
	AVCodecContext* encoder_ctx;
};