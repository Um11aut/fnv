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
	encoder_ctx->bit_rate = 4000000;
	encoder_ctx->max_b_frames = 2;
	encoder_ctx->gop_size = 10;

	av_opt_set(encoder_ctx->priv_data, "preset", opts.preset, 0);
	av_opt_set(encoder_ctx->priv_data, "profile", opts.profile, 0);
	av_opt_set(encoder_ctx->priv_data, "gpu", opts.gpu, 0); // Specify the GPU to use

	if (avcodec_open2(encoder_ctx, encoder, nullptr) < 0) {
		throw std::runtime_error("Failed to open codec!");
	}
}

NVEncoder::~NVEncoder()
{
	avcodec_free_context(&encoder_ctx);
}

void NVEncoder::async_send_frame(AVFrame* frame)
{
	fut = std::async(std::launch::async, [&]{
		return avcodec_send_frame(encoder_ctx, frame);
	});
}

void NVEncoder::await_receive_frame(const std::function<void(AVPacket*)>& handle_func)
{
	int resp = fut.get();
	if (resp < 0) {
		throw std::runtime_error(fmt::format("Error sending frame to encoder: {}", resp));
	}

	AVPacket* out_packet = av_packet_alloc();
	while (avcodec_receive_packet(encoder_ctx, out_packet) >= 0) {
		handle_func(out_packet);

		// Clean up packet
		av_packet_unref(out_packet);
	}
}

void NVEncoder::send_receive_frame(AVFrame* frame, AVPacket& packet, const std::function<void()>& handle_func)
{
	if (int res = avcodec_send_frame(encoder_ctx, frame); res < 0) {
		throw std::runtime_error(fmt::format("Error sending frame to encoder: {}", res));
	}

	while (avcodec_receive_packet(encoder_ctx, &packet) >= 0) {
		handle_func();

		// Clean up packet
		av_packet_unref(&packet);
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
