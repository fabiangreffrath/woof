
#include "doomtype.h"
#include "doomdef.h"
#include "i_oalsound.h"
#include "i_printf.h"
#include "i_system.h"
#include "v_video.h"

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "libswscale/swscale.h"

#include "SDL_pixels.h"

#define VIDEO_CODEC_ID       AV_CODEC_ID_H264
#define AUDIO_CODEC_ID       AV_CODEC_ID_MP2
#define OUTPUT_FILENAME      "output.mp4"
#define VIDEO_BITRATE        4000000
#define AUDIO_BITRATE        128000
#define VIDEO_FRAMERATE      60
#define AUDIO_SAMPLE_RATE    44100
#define AUDIO_CHANNEL_LAYOUT AV_CH_LAYOUT_STEREO

static AVFormatContext *fmt_ctx;
static AVCodecContext *video_enc_ctx, *audio_enc_ctx;
static struct SwsContext *sws_ctx;
static AVStream *video_stream, *audio_stream;
static AVFrame *yuv_frame, *pal_frame, *audio_frame;
static AVPacket *packet;

static boolean ffmpeg_initialized;

static int InitVideoStream(void)
{
    const AVCodec *codec = avcodec_find_encoder(VIDEO_CODEC_ID);
    if (!codec)
    {
        I_Printf(VB_DEBUG, "Video codec not found");
        return AVERROR(ENOSYS);
    }

    video_stream = avformat_new_stream(fmt_ctx, NULL);
    if (!video_stream)
    {
        return AVERROR(ENOMEM);
    }

    video_enc_ctx = avcodec_alloc_context3(codec);
    if (!video_enc_ctx)
    {
        return AVERROR(ENOMEM);
    }

    // Video codec configuration
    video_enc_ctx->width = video.width;
    video_enc_ctx->height = video.height;
    video_enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    video_enc_ctx->time_base = (AVRational){1, TICRATE};
    video_enc_ctx->bit_rate = VIDEO_BITRATE;

    AVRational sar = {.num = 5, .den = 6};
    video_enc_ctx->sample_aspect_ratio = sar;

    av_opt_set(video_enc_ctx->priv_data, "preset", "fast", 0);

    if (avcodec_open2(video_enc_ctx, codec, NULL) < 0)
    {
        I_Printf(VB_DEBUG, "Failed to open video codec");
        return AVERROR_EXIT;
    }

    avcodec_parameters_from_context(video_stream->codecpar, video_enc_ctx);

    // Setup palette frame (input format)
    pal_frame->format = AV_PIX_FMT_PAL8;
    pal_frame->width = video.width;
    pal_frame->height = video.height;
    av_frame_get_buffer(pal_frame, 0);

    // Setup YUV frame (output format)
    yuv_frame->format = AV_PIX_FMT_YUV420P;
    yuv_frame->width = video.width;
    yuv_frame->height = video.height;
    yuv_frame->pts = 0;
    av_frame_get_buffer(yuv_frame, 0);

    // Initialize scaling context
    sws_ctx = sws_getContext(video.width, video.height, AV_PIX_FMT_PAL8,
                             video.width, video.height, AV_PIX_FMT_YUV420P,
                             SWS_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx)
    {
        I_Printf(VB_DEBUG, "Failed to initialize scaling context");
        return AVERROR_EXIT;
    }

    return 0;
}

static int InitAudioStream(void)
{
    const AVCodec *codec = avcodec_find_encoder(AUDIO_CODEC_ID);
    if (!codec)
    {
        I_Printf(VB_DEBUG, "Audio codec not found");
        return AVERROR(ENOSYS);
    }

    audio_stream = avformat_new_stream(fmt_ctx, NULL);
    if (!audio_stream)
    {
        return AVERROR(ENOMEM);
    }

    audio_enc_ctx = avcodec_alloc_context3(codec);
    if (!audio_enc_ctx)
    {
        return AVERROR(ENOMEM);
    }

    // Audio codec configuration
    audio_enc_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    audio_enc_ctx->bit_rate = AUDIO_BITRATE;
    audio_enc_ctx->sample_rate = AUDIO_SAMPLE_RATE;
    audio_enc_ctx->time_base = (AVRational){1, AUDIO_SAMPLE_RATE};

    // Channel layout configuration
    av_channel_layout_from_mask(&audio_enc_ctx->ch_layout,
                                AUDIO_CHANNEL_LAYOUT);

    if (avcodec_open2(audio_enc_ctx, codec, NULL) < 0)
    {
        I_Printf(VB_DEBUG, "Failed to open audio codec");
        return AVERROR_EXIT;
    }

    avcodec_parameters_from_context(audio_stream->codecpar, audio_enc_ctx);

    // Setup audio frame
    audio_frame->format = audio_enc_ctx->sample_fmt;
    audio_frame->sample_rate = AUDIO_SAMPLE_RATE;
    audio_frame->nb_samples = audio_enc_ctx->frame_size;
    audio_frame->pts = 0;
    av_channel_layout_copy(&audio_frame->ch_layout, &audio_enc_ctx->ch_layout);
    if (av_frame_get_buffer(audio_frame, 0) < 0)
    {
        I_Error("Failed to setup audio frame.");
    }

    return 0;
}

static int WriteFrame(AVFormatContext *fmt_ctx, AVCodecContext *enc_ctx,
                      AVStream *stream, AVFrame *frame, AVPacket *pkt)
{
    int ret = avcodec_send_frame(enc_ctx, frame);
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return 0;
        }
        if (ret < 0)
        {
            return ret;
        }

        av_packet_rescale_ts(pkt, enc_ctx->time_base, stream->time_base);
        pkt->stream_index = stream->index;
        ret = av_interleaved_write_frame(fmt_ctx, pkt);
        av_packet_unref(pkt);
        if (ret < 0)
        {
            return ret;
        }
    }
    return ret;
}

void I_MPG_Flush(void)
{
    if (!ffmpeg_initialized)
    {
        return;
    }

    // Flush encoders
    if (WriteFrame(fmt_ctx, video_enc_ctx, video_stream, NULL, packet) < 0)
    {
        I_Error("Error flush video encoder.");
    }
    if (WriteFrame(fmt_ctx, audio_enc_ctx, audio_stream, NULL, packet) < 0)
    {
        I_Error("Error flush audio encoder.");
    }

    // Write trailer
    av_write_trailer(fmt_ctx);
}

void I_MPG_Shutdown(void)
{
    I_MPG_Flush();
    av_packet_free(&packet);
    av_frame_free(&yuv_frame);
    av_frame_free(&pal_frame);
    av_frame_free(&audio_frame);
    avcodec_free_context(&video_enc_ctx);
    avcodec_free_context(&audio_enc_ctx);
    if (fmt_ctx && !(fmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        avio_closep(&fmt_ctx->pb);
    }
    avformat_free_context(fmt_ctx);
    ffmpeg_initialized = false;
}

void I_MPG_Init(void)
{
    I_AtExit(I_MPG_Shutdown, true);

    // Allocate format context
    avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, OUTPUT_FILENAME);
    if (!fmt_ctx)
    {
        I_Error("Failed to allocate format context.");
    }

    // Allocate frames
    yuv_frame = av_frame_alloc();
    pal_frame = av_frame_alloc();
    audio_frame = av_frame_alloc();
    packet = av_packet_alloc();
    if (!yuv_frame || !pal_frame || !audio_frame || !packet)
    {
        I_Error("Failed to allocate frames.");
    }

    // Initialize streams
    if (InitVideoStream())
    {
        I_Error("Failed to initialize video stream.");
    }
    if (InitAudioStream())
    {
        I_Error("Failed to initialize audio stream.");
    }

    // Open output file
    if (avio_open(&fmt_ctx->pb, OUTPUT_FILENAME, AVIO_FLAG_WRITE) < 0)
    {
        I_Error("Failed to open output file.");
    }

    // Write header
    if (avformat_write_header(fmt_ctx, NULL) < 0)
    {
        I_Error("Failed to write header.");
    }

    I_Printf(VB_INFO, "Initialize FFmpeg");

    ffmpeg_initialized = true;
}

void I_MPG_EncodeVideo(const pixel_t *frame)
{
    if (!ffmpeg_initialized)
    {
        return;
    }

    pal_frame->data[0] = (uint8_t *)frame;
    av_frame_make_writable(yuv_frame);
    // Convert PAL8 -> YUV420P
    sws_scale(sws_ctx, (const uint8_t *const *)pal_frame->data,
              pal_frame->linesize, 0, video.height, yuv_frame->data,
              yuv_frame->linesize);
    yuv_frame->pts++;

    if (WriteFrame(fmt_ctx, video_enc_ctx, video_stream, yuv_frame, packet))
    {
        I_Error("Error writing video frame.");
    }
}

void I_MPG_SetPalette(SDL_Color *palette)
{
    if (!ffmpeg_initialized)
    {
        return;
    }

    uint32_t *avpalette = (uint32_t *)pal_frame->data[1];

    for (int i = 0; i < 256; i++)
    {
        avpalette[i] = (0xff << 24) | (palette[i].r << 16) | (palette[i].g << 8)
                       | (palette[i].b);
    }
}

void I_MPG_EncodeAudio(void)
{
    if (!ffmpeg_initialized)
    {
        return;
    }

    // Generate and encode audio frames (one tick)
    const int audio_frame_count =
        (AUDIO_SAMPLE_RATE / TICRATE) / audio_enc_ctx->frame_size;
    for (int i = 0; i < audio_frame_count; i++)
    {
        av_frame_make_writable(audio_frame);
        I_OAL_RenderSamples(audio_frame->data[0], audio_frame->nb_samples);
        audio_frame->pts += audio_enc_ctx->frame_size;

        if (WriteFrame(fmt_ctx, audio_enc_ctx, audio_stream, audio_frame,
                       packet))
        {
            I_Error("Error writing audio frame.");
        }
    }
}
