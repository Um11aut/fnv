#include "fnv.h"
#include <iostream>

int main(int argc, char** argv)
{
    av_log_set_level(AV_LOG_VERBOSE);
    avdevice_register_all();
    avformat_network_init();
    AVFormatContext* ctx = avformat_alloc_context();
    
    try {
        ScreenGrabber grabber{ ctx, ScreenGrabberOpts{
        .framerate = 20,
        .window_size = {{1920, 1080}},
        .window_name = "desktop",
        } };

        auto& codecpar = ctx->streams[grabber.get_stream_idx()]->codecpar;
        NVEncoder encoder{ ctx, static_cast<uint32_t>(codecpar->width), static_cast<uint32_t>(codecpar->height) };

        spdlog::info("Codec of incoming stream: {}", avcodec_get_name(codecpar->codec_id));
        const AVCodec* decoder = avcodec_find_decoder(codecpar->codec_id);
        if (!decoder) {
            spdlog::error("Failed to find decoder for stream!");
            avformat_free_context(ctx);
            return -1;
        }

        AVCodecContext* decoder_ctx = avcodec_alloc_context3(decoder);
        if (!decoder_ctx) {
            spdlog::error("Failed to allocate decoder context!");
            avformat_free_context(ctx);
            return -1;
        }

        if (avcodec_open2(decoder_ctx, decoder, nullptr) < 0) {
            spdlog::error("Failed to open decoder!");
            return -1;
        }

        AVFrame* frame = av_frame_alloc();
        AVFrame* sws_frame = av_frame_alloc(); // Frame for conversion
        if (!frame || !sws_frame) {
            spdlog::error("Failed to allocate frame!");
            return -1;
        }

        sws_frame->format = AV_PIX_FMT_NV12;
        sws_frame->width = encoder.get_width();
        sws_frame->height = encoder.get_height();

        struct SwsContext* sws_ctx = sws_getContext(
            encoder.get_width(), encoder.get_height(), AV_PIX_FMT_RGBA,  // Source format
            encoder.get_width(), encoder.get_height(), AV_PIX_FMT_NV12,  // Target format (NV12 for NVENC)
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );

        if (!sws_ctx) {
            spdlog::error("Failed to allocate sws context!");
            return -1;
        }

        if (av_frame_get_buffer(sws_frame, 0) < 0) {
            spdlog::error("Failed to allocate frame buffer!");
            return -1;
        }

        grabber.loop([&](AVPacket& packet) {
            // Send packet to decoder
            int response = avcodec_send_packet(decoder_ctx, &packet);
            if (response < 0) {
                spdlog::error("Error sending packet to decoder: {}", response);
            }

            while (avcodec_receive_frame(decoder_ctx, frame) >= 0) {
                // Convert RGB/RGBA to NV12
                sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height,
                    sws_frame->data, sws_frame->linesize);

                encoder.send_receive_frame(sws_frame, packet, [&] {
                    spdlog::info("Encoded packet size: {}", packet.size);
                });
            }
        });


        av_frame_free(&frame);
        av_frame_free(&sws_frame);
        sws_freeContext(sws_ctx);
        avcodec_free_context(&decoder_ctx);
    }
    catch (const std::exception& e) {
        spdlog::error(e.what());
        return -1;
    }

    avformat_free_context(ctx);
    return 0;
}