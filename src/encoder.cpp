#include "encoder.h"

NVEncoder::NVEncoder(AVFormatContext* ctx, const uint32_t& width, const uint32_t& height, const NVEncoderOpts& opts)
	: ctx_fmt(ctx)
{
	encoder = pick_available_encoder();
	if (!encoder) {
		throw std::runtime_error("Failed to find any supported nvidia codec!");
	}

	encoder_ctx = avcodec_alloc_context3(encoder);
	if (!encoder_ctx) {
		throw std::runtime_error("Failed to allocate encoder context!");
	}

	encoder_ctx->width = width;
	encoder_ctx->height = height;
	encoder_ctx->time_base = { 1, opts.fps }; // 30 fps
	encoder_ctx->pix_fmt = AV_PIX_FMT_NV12; // NV12 format for NVENC

	av_opt_set(encoder_ctx->priv_data, "preset", opts.preset, 0);
	av_opt_set(encoder_ctx->priv_data, "profile", opts.profile, 0);
	av_opt_set(encoder_ctx->priv_data, "gpu", opts.gpu, 0); // Specify the GPU to use

	if (avcodec_open2(encoder_ctx, encoder, nullptr) < 0) {
		throw std::runtime_error("Failed to open codec!");
	}
}

const AVCodec* NVEncoder::pick_available_encoder()
{
	for (const auto& candidate : NVENCCodecPriorities) {
		const AVCodec* _encoder = avcodec_find_encoder_by_name("hevc_nvenc");
		if (!_encoder) {
			spdlog::warn("NVENC Codec {} not supported!", candidate);
			continue;
		}

		spdlog::info("Picking NVEncoder: {}", candidate);
		return _encoder;
	}

	return nullptr;
}
