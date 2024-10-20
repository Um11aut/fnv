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

        const AVCodec* encoder = avcodec_find_encoder_by_name("hevc_nvenc");
        if (!encoder) {
            spdlog::error("Failed to find encoder!");
            return -1;
        }

        AVCodecContext* encoder_ctx = avcodec_alloc_context3(encoder);
        if (!encoder_ctx) {
            spdlog::error("Failed to allocate encoder context!");
            return -1;
        }

        AVCodecID input_codec = ctx->streams[grabber.get_stream_idx()]->codecpar->codec_id;
        spdlog::info("Default Codec for encoding: {}", avcodec_get_name(input_codec));
        const AVCodec* decoder = avcodec_find_decoder(input_codec);
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

        encoder_ctx->bit_rate = 4000000;
        encoder_ctx->width = ctx->streams[grabber.get_stream_idx()]->codecpar->width;
        encoder_ctx->height = ctx->streams[grabber.get_stream_idx()]->codecpar->height;
        encoder_ctx->time_base = { 1, 30 }; // 30 fps
        encoder_ctx->pix_fmt = AV_PIX_FMT_NV12; // NV12 format for NVENC
        encoder_ctx->max_b_frames = 2;
        encoder_ctx->gop_size = 10;

        av_opt_set(encoder_ctx->priv_data, "preset", "slow", 0);
        av_opt_set(encoder_ctx->priv_data, "profile", "main", 0);
        av_opt_set(encoder_ctx->priv_data, "gpu", "0", 0); // Specify the GPU to use

        if (avcodec_open2(encoder_ctx, encoder, nullptr) < 0) {
            spdlog::error("Failed to open codec!");
            return -1;
        }

        AVFrame* frame = av_frame_alloc();
        AVFrame* sws_frame = av_frame_alloc(); // Frame for conversion
        if (!frame || !sws_frame) {
            spdlog::error("Failed to allocate frame!");
            return -1;
        }

        sws_frame->format = AV_PIX_FMT_NV12;
        sws_frame->width = encoder_ctx->width;
        sws_frame->height = encoder_ctx->height;

        struct SwsContext* sws_ctx = sws_getContext(
            encoder_ctx->width, encoder_ctx->height, AV_PIX_FMT_RGBA,  // Source format
            encoder_ctx->width, encoder_ctx->height, AV_PIX_FMT_NV12,  // Target format (NV12 for NVENC)
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

        // Initialize the output format context
        AVFormatContext* output_ctx = nullptr;
        avformat_alloc_output_context2(&output_ctx, nullptr, "mp4", "output.mp4");
        if (!output_ctx) {
            spdlog::error("Failed to create output context!");
            return -1;
        }

        AVStream* out_stream = avformat_new_stream(output_ctx, encoder);
        if (!out_stream) {
            spdlog::error("Failed to create output stream!");
            avformat_free_context(output_ctx);
            return -1;
        }

        avcodec_parameters_from_context(out_stream->codecpar, encoder_ctx);
        out_stream->time_base = encoder_ctx->time_base;

        if (!(output_ctx->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&output_ctx->pb, "output.mp4", AVIO_FLAG_WRITE) < 0) {
                spdlog::error("Could not open output file!");
                avformat_free_context(output_ctx);
                return -1;
            }
        }

        if (avformat_write_header(output_ctx, nullptr) < 0) {
            spdlog::error("Error occurred when opening output file!");
            avio_closep(&output_ctx->pb);
            avformat_free_context(output_ctx);
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

                // Send the converted frame to the encoder
                response = avcodec_send_frame(encoder_ctx, sws_frame);
                if (response < 0) {
                    spdlog::error("Error sending frame to encoder: {}", response);
                    break;
                }

                AVPacket* output_packet = av_packet_alloc();
                while (avcodec_receive_packet(encoder_ctx, output_packet) >= 0) {
                    spdlog::info("Encoded packet size: {}", output_packet->size);

                    output_packet->pts = av_rescale_q(output_packet->pts, encoder_ctx->time_base, out_stream->time_base);
                    output_packet->dts = av_rescale_q(output_packet->dts, encoder_ctx->time_base, out_stream->time_base);

                    if (auto res = av_write_frame(output_ctx, output_packet); res < 0) {
                        char err_buf[AV_ERROR_MAX_STRING_SIZE];
                        av_strerror(res, err_buf, sizeof(err_buf));
                        spdlog::error("Error writing to output file: {}!", err_buf);
                    }

                    // Clean up packet
                    av_packet_unref(output_packet);
                }
            }
        });

        // Write the trailer and close the file
        av_write_trailer(output_ctx);
        avio_closep(&output_ctx->pb);
        avformat_free_context(output_ctx);

        av_frame_free(&frame);
        av_frame_free(&sws_frame);
        sws_freeContext(sws_ctx);
        avcodec_free_context(&encoder_ctx);
        avcodec_free_context(&decoder_ctx);
    }
    catch (const std::exception& e) {
        spdlog::error(e.what());
        return -1;
    }

    avformat_free_context(ctx);
    return 0;
}