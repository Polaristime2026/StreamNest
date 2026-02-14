#include "ffmpeg_wrap.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <vector>

#include "ring_buffer.h"

namespace wasm_player {

namespace {

constexpr size_t kInputRingCapacity = 32 * 1024 * 1024;
constexpr size_t kInputReserveBytes = 128 * 1024 * 1024;
constexpr size_t kMaxVideoQueue = 8;
constexpr size_t kMaxAudioQueue = 32;

struct VideoFrame {
  std::vector<uint8_t> y;
  std::vector<uint8_t> u;
  std::vector<uint8_t> v;
  int width = 0;
  int height = 0;
  int y_stride = 0;
  int uv_stride = 0;
  double pts = 0.0;
};

struct AudioFrame {
  std::vector<float> pcm;
  int frames = 0;
  double pts = 0.0;
};

struct MemoryReader {
  const uint8_t* data = nullptr;
  size_t size = 0;
  size_t pos = 0;
};

int read_packet(void* opaque, uint8_t* buf, int buf_size) {
  auto* mem = static_cast<MemoryReader*>(opaque);
  if (!mem || !buf || buf_size <= 0) {
    return AVERROR(EINVAL);
  }

  if (mem->pos >= mem->size) {
    return AVERROR_EOF;
  }

  const size_t left = mem->size - mem->pos;
  const size_t n = std::min(left, static_cast<size_t>(buf_size));
  std::memcpy(buf, mem->data + mem->pos, n);
  mem->pos += n;
  return static_cast<int>(n);
}

int64_t seek_packet(void* opaque, int64_t offset, int whence) {
  auto* mem = static_cast<MemoryReader*>(opaque);
  if (!mem) {
    return -1;
  }

  if (whence == AVSEEK_SIZE) {
    return static_cast<int64_t>(mem->size);
  }

  const int origin = whence & ~AVSEEK_FORCE;
  int64_t base = 0;
  switch (origin) {
    case SEEK_SET:
      base = 0;
      break;
    case SEEK_CUR:
      base = static_cast<int64_t>(mem->pos);
      break;
    case SEEK_END:
      base = static_cast<int64_t>(mem->size);
      break;
    default:
      return -1;
  }

  const int64_t next = base + offset;
  if (next < 0 || static_cast<size_t>(next) > mem->size) {
    return -1;
  }

  mem->pos = static_cast<size_t>(next);
  return next;
}

inline double to_sec(int64_t ts, AVRational tb) {
  if (ts == AV_NOPTS_VALUE) {
    return 0.0;
  }
  return ts * av_q2d(tb);
}

}  // namespace

struct Core {
  explicit Core() : input_ring(kInputRingCapacity) { input_blob.reserve(kInputReserveBytes); }

  int out_sample_rate = 48000;
  int out_channels = 2;

  ByteRingBuffer input_ring;
  std::vector<uint8_t> input_blob;
  bool input_eof = false;

  AVFormatContext* fmt = nullptr;
  AVIOContext* avio = nullptr;
  uint8_t* avio_buffer = nullptr;
  MemoryReader memory_reader;

  AVCodecContext* vdec = nullptr;
  AVCodecContext* adec = nullptr;
  int video_stream_index = -1;
  int audio_stream_index = -1;

  AVPacket* packet = nullptr;
  AVFrame* frame = nullptr;
  AVFrame* yuv_frame = nullptr;

  SwsContext* sws = nullptr;
  SwrContext* swr = nullptr;
  int swr_in_rate = 0;
  int swr_in_channels = 0;
  int swr_in_sample_fmt = AV_SAMPLE_FMT_NONE;
  int64_t swr_in_layout = 0;

  std::deque<VideoFrame> video_queue;
  std::deque<AudioFrame> audio_queue;

  VideoFrame current_video;
  AudioFrame current_audio;
  bool has_video = false;
  bool has_audio = false;

  bool opened = false;
  bool decoders_drained = false;
  bool file_eof = false;

  double duration_sec = 0.0;
};

int open_decoder(AVFormatContext* fmt, AVMediaType type, int* stream_index, AVCodecContext** out_ctx) {
  const int idx = av_find_best_stream(fmt, type, -1, -1, nullptr, 0);
  if (idx < 0) {
    return idx;
  }

  AVStream* stream = fmt->streams[idx];
  const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
  if (!codec) {
    return AVERROR_DECODER_NOT_FOUND;
  }

  AVCodecContext* ctx = avcodec_alloc_context3(codec);
  if (!ctx) {
    return AVERROR(ENOMEM);
  }

  int ret = avcodec_parameters_to_context(ctx, stream->codecpar);
  if (ret < 0) {
    avcodec_free_context(&ctx);
    return ret;
  }

  ret = avcodec_open2(ctx, codec, nullptr);
  if (ret < 0) {
    avcodec_free_context(&ctx);
    return ret;
  }

  *stream_index = idx;
  *out_ctx = ctx;
  return 0;
}

void clear_queues(Core* core) {
  core->video_queue.clear();
  core->audio_queue.clear();
  core->has_video = false;
  core->has_audio = false;
}

void free_codec_resources(Core* core) {
  if (core->sws) {
    sws_freeContext(core->sws);
    core->sws = nullptr;
  }
  if (core->swr) {
    swr_free(&core->swr);
  }

  if (core->yuv_frame) {
    av_frame_free(&core->yuv_frame);
  }
  if (core->frame) {
    av_frame_free(&core->frame);
  }
  if (core->packet) {
    av_packet_free(&core->packet);
  }

  if (core->vdec) {
    avcodec_free_context(&core->vdec);
  }
  if (core->adec) {
    avcodec_free_context(&core->adec);
  }

  core->video_stream_index = -1;
  core->audio_stream_index = -1;
}

void close_format(Core* core) {
  if (core->fmt) {
    avformat_close_input(&core->fmt);
    core->fmt = nullptr;
  }

  if (core->avio) {
    avio_context_free(&core->avio);
    core->avio = nullptr;
    core->avio_buffer = nullptr;
  }
}

void close_all(Core* core) {
  clear_queues(core);
  free_codec_resources(core);
  close_format(core);

  core->opened = false;
  core->decoders_drained = false;
  core->file_eof = false;
  core->duration_sec = 0.0;
  core->memory_reader = {};
}

int drain_input_ring(Core* core) {
  std::array<uint8_t, 64 * 1024> chunk{};
  while (!core->input_ring.empty()) {
    const size_t n = core->input_ring.read(chunk.data(), chunk.size());
    if (n == 0) {
      break;
    }
    const size_t old_size = core->input_blob.size();
    core->input_blob.resize(old_size + n);
    std::memcpy(core->input_blob.data() + old_size, chunk.data(), n);
  }
  return 0;
}

int ensure_yuv_frame(Core* core, int width, int height) {
  if (core->yuv_frame && core->yuv_frame->width == width && core->yuv_frame->height == height) {
    return 0;
  }

  if (core->yuv_frame) {
    av_frame_free(&core->yuv_frame);
  }

  core->yuv_frame = av_frame_alloc();
  if (!core->yuv_frame) {
    return AVERROR(ENOMEM);
  }

  core->yuv_frame->format = AV_PIX_FMT_YUV420P;
  core->yuv_frame->width = width;
  core->yuv_frame->height = height;

  int ret = av_frame_get_buffer(core->yuv_frame, 32);
  if (ret < 0) {
    return ret;
  }

  return 0;
}

int enqueue_video_frame(Core* core, AVFrame* decoded) {
  if (core->video_queue.size() >= kMaxVideoQueue) {
    return 0;
  }

  AVFrame* src = decoded;
  if (decoded->format != AV_PIX_FMT_YUV420P) {
    core->sws = sws_getCachedContext(core->sws,
                                     decoded->width,
                                     decoded->height,
                                     static_cast<AVPixelFormat>(decoded->format),
                                     decoded->width,
                                     decoded->height,
                                     AV_PIX_FMT_YUV420P,
                                     SWS_BILINEAR,
                                     nullptr,
                                     nullptr,
                                     nullptr);
    if (!core->sws) {
      return AVERROR(EINVAL);
    }

    int ret = ensure_yuv_frame(core, decoded->width, decoded->height);
    if (ret < 0) {
      return ret;
    }

    ret = av_frame_make_writable(core->yuv_frame);
    if (ret < 0) {
      return ret;
    }

    sws_scale(core->sws,
              decoded->data,
              decoded->linesize,
              0,
              decoded->height,
              core->yuv_frame->data,
              core->yuv_frame->linesize);
    src = core->yuv_frame;
  }

  VideoFrame out;
  out.width = src->width;
  out.height = src->height;
  out.y_stride = out.width;
  out.uv_stride = (out.width + 1) / 2;
  out.pts = to_sec(decoded->best_effort_timestamp,
                   core->fmt->streams[core->video_stream_index]->time_base);

  const int chroma_h = (out.height + 1) / 2;

  out.y.resize(static_cast<size_t>(out.y_stride) * out.height);
  out.u.resize(static_cast<size_t>(out.uv_stride) * chroma_h);
  out.v.resize(static_cast<size_t>(out.uv_stride) * chroma_h);

  for (int y = 0; y < out.height; ++y) {
    std::memcpy(out.y.data() + static_cast<size_t>(y) * out.y_stride,
                src->data[0] + static_cast<size_t>(y) * src->linesize[0],
                out.width);
  }

  const int chroma_w = (out.width + 1) / 2;
  for (int y = 0; y < chroma_h; ++y) {
    std::memcpy(out.u.data() + static_cast<size_t>(y) * out.uv_stride,
                src->data[1] + static_cast<size_t>(y) * src->linesize[1],
                chroma_w);
    std::memcpy(out.v.data() + static_cast<size_t>(y) * out.uv_stride,
                src->data[2] + static_cast<size_t>(y) * src->linesize[2],
                chroma_w);
  }

  core->video_queue.push_back(std::move(out));
  return 1;
}

int ensure_swr(Core* core, const AVFrame* decoded) {
  int64_t in_layout = decoded->channel_layout;
  if (in_layout == 0) {
    in_layout = av_get_default_channel_layout(decoded->channels);
  }

  const int in_fmt = decoded->format;
  const int in_rate = decoded->sample_rate;
  const int in_channels = decoded->channels;

  const bool reinit = !core->swr || core->swr_in_rate != in_rate ||
                      core->swr_in_channels != in_channels ||
                      core->swr_in_sample_fmt != in_fmt ||
                      core->swr_in_layout != in_layout;

  if (!reinit) {
    return 0;
  }

  if (core->swr) {
    swr_free(&core->swr);
  }

  const int64_t out_layout = av_get_default_channel_layout(core->out_channels);
  core->swr = swr_alloc_set_opts(nullptr,
                                 out_layout,
                                 AV_SAMPLE_FMT_FLT,
                                 core->out_sample_rate,
                                 in_layout,
                                 static_cast<AVSampleFormat>(in_fmt),
                                 in_rate,
                                 0,
                                 nullptr);
  if (!core->swr) {
    return AVERROR(ENOMEM);
  }

  int ret = swr_init(core->swr);
  if (ret < 0) {
    return ret;
  }

  core->swr_in_rate = in_rate;
  core->swr_in_channels = in_channels;
  core->swr_in_sample_fmt = in_fmt;
  core->swr_in_layout = in_layout;
  return 0;
}

int enqueue_audio_frame(Core* core, AVFrame* decoded) {
  if (core->audio_queue.size() >= kMaxAudioQueue) {
    return 0;
  }

  int ret = ensure_swr(core, decoded);
  if (ret < 0) {
    return ret;
  }

  int out_samples = av_rescale_rnd(
      swr_get_delay(core->swr, decoded->sample_rate) + decoded->nb_samples,
      core->out_sample_rate,
      decoded->sample_rate,
      AV_ROUND_UP);

  if (out_samples <= 0) {
    return 0;
  }

  AudioFrame out;
  out.pcm.resize(static_cast<size_t>(out_samples) * core->out_channels);

  uint8_t* dst[1] = {reinterpret_cast<uint8_t*>(out.pcm.data())};
  const int converted = swr_convert(core->swr,
                                    dst,
                                    out_samples,
                                    const_cast<const uint8_t**>(decoded->extended_data),
                                    decoded->nb_samples);
  if (converted < 0) {
    return converted;
  }

  out.frames = converted;
  out.pcm.resize(static_cast<size_t>(converted) * core->out_channels);
  out.pts = to_sec(decoded->best_effort_timestamp,
                   core->fmt->streams[core->audio_stream_index]->time_base);

  if (out.frames > 0) {
    core->audio_queue.push_back(std::move(out));
    return 1;
  }

  return 0;
}

int decode_video_packet(Core* core, const AVPacket* packet) {
  if (!core->vdec) {
    return 0;
  }

  int ret = avcodec_send_packet(core->vdec, packet);
  if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
    return ret;
  }

  int produced = 0;
  while (true) {
    ret = avcodec_receive_frame(core->vdec, core->frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break;
    }
    if (ret < 0) {
      return ret;
    }

    const int pushed = enqueue_video_frame(core, core->frame);
    av_frame_unref(core->frame);
    if (pushed < 0) {
      return pushed;
    }
    produced += pushed;

    if (core->video_queue.size() >= kMaxVideoQueue) {
      break;
    }
  }

  return produced;
}

int decode_audio_packet(Core* core, const AVPacket* packet) {
  if (!core->adec) {
    return 0;
  }

  int ret = avcodec_send_packet(core->adec, packet);
  if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
    return ret;
  }

  int produced = 0;
  while (true) {
    ret = avcodec_receive_frame(core->adec, core->frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break;
    }
    if (ret < 0) {
      return ret;
    }

    const int pushed = enqueue_audio_frame(core, core->frame);
    av_frame_unref(core->frame);
    if (pushed < 0) {
      return pushed;
    }
    produced += pushed;

    if (core->audio_queue.size() >= kMaxAudioQueue) {
      break;
    }
  }

  return produced;
}

int drain_decoders(Core* core) {
  if (core->decoders_drained) {
    return 0;
  }

  int produced = 0;

  if (core->vdec) {
    int ret = decode_video_packet(core, nullptr);
    if (ret < 0) {
      return ret;
    }
    produced += ret;
  }

  if (core->adec) {
    int ret = decode_audio_packet(core, nullptr);
    if (ret < 0) {
      return ret;
    }
    produced += ret;
  }

  core->decoders_drained = true;
  return produced;
}

int open_from_memory(Core* core) {
  if (core->opened) {
    return 0;
  }
  if (core->input_blob.empty()) {
    return AVERROR(EINVAL);
  }

  core->memory_reader.data = core->input_blob.data();
  core->memory_reader.size = core->input_blob.size();
  core->memory_reader.pos = 0;

  core->avio_buffer = static_cast<uint8_t*>(av_malloc(64 * 1024));
  if (!core->avio_buffer) {
    return AVERROR(ENOMEM);
  }

  core->avio = avio_alloc_context(core->avio_buffer,
                                  64 * 1024,
                                  0,
                                  &core->memory_reader,
                                  read_packet,
                                  nullptr,
                                  seek_packet);
  if (!core->avio) {
    av_free(core->avio_buffer);
    core->avio_buffer = nullptr;
    return AVERROR(ENOMEM);
  }

  core->fmt = avformat_alloc_context();
  if (!core->fmt) {
    return AVERROR(ENOMEM);
  }

  core->fmt->pb = core->avio;
  core->fmt->flags |= AVFMT_FLAG_CUSTOM_IO;

  int ret = avformat_open_input(&core->fmt, nullptr, nullptr, nullptr);
  if (ret < 0) {
    return ret;
  }

  ret = avformat_find_stream_info(core->fmt, nullptr);
  if (ret < 0) {
    return ret;
  }

  if (core->fmt->duration > 0) {
    core->duration_sec = static_cast<double>(core->fmt->duration) / AV_TIME_BASE;
  }

  ret = open_decoder(core->fmt, AVMEDIA_TYPE_VIDEO, &core->video_stream_index, &core->vdec);
  if (ret < 0) {
    return ret;
  }

  const int audio_ret = open_decoder(core->fmt, AVMEDIA_TYPE_AUDIO, &core->audio_stream_index, &core->adec);
  if (audio_ret < 0) {
    core->audio_stream_index = -1;
    core->adec = nullptr;
  }

  core->packet = av_packet_alloc();
  core->frame = av_frame_alloc();
  if (!core->packet || !core->frame) {
    return AVERROR(ENOMEM);
  }

  core->opened = true;
  core->decoders_drained = false;
  core->file_eof = false;
  return 0;
}

}  // namespace wasm_player

namespace wasm_player {

Core* core_create() { return new Core(); }

void core_destroy(Core* core) {
  if (!core) {
    return;
  }
  close_all(core);
  delete core;
}

int core_init(Core* core, int sample_rate, int channels) {
  if (!core) {
    return -1;
  }

  close_all(core);

  core->input_ring.clear();
  core->input_blob.clear();
  core->input_eof = false;

  core->out_sample_rate = sample_rate > 0 ? sample_rate : 48000;
  core->out_channels = channels > 0 ? channels : 2;

  return 0;
}

int core_feed(Core* core, const uint8_t* data, int len) {
  if (!core) {
    return -1;
  }

  if (len <= 0) {
    core->input_eof = true;
    return 0;
  }

  const size_t written = core->input_ring.write(data, static_cast<size_t>(len));
  if (written != static_cast<size_t>(len)) {
    return -2;
  }
  return static_cast<int>(written);
}

int core_decode_step(Core* core) {
  if (!core) {
    return -1;
  }

  const int drain_ret = drain_input_ring(core);
  if (drain_ret < 0) {
    return drain_ret;
  }

  if (!core->opened) {
    if (!core->input_eof) {
      return 0;
    }
    const int open_ret = open_from_memory(core);
    if (open_ret < 0) {
      return open_ret;
    }
  }

  int produced = 0;
  int rounds = 0;

  while (rounds < 32) {
    if (core->video_queue.size() >= kMaxVideoQueue && core->audio_queue.size() >= kMaxAudioQueue) {
      break;
    }

    const int ret = av_read_frame(core->fmt, core->packet);
    if (ret == AVERROR_EOF) {
      core->file_eof = true;
      int drain = drain_decoders(core);
      if (drain < 0) {
        return drain;
      }
      produced += drain;
      break;
    }
    if (ret < 0) {
      return ret;
    }

    if (core->packet->stream_index == core->video_stream_index) {
      const int d = decode_video_packet(core, core->packet);
      if (d < 0) {
        av_packet_unref(core->packet);
        return d;
      }
      produced += d;
    } else if (core->packet->stream_index == core->audio_stream_index) {
      const int d = decode_audio_packet(core, core->packet);
      if (d < 0) {
        av_packet_unref(core->packet);
        return d;
      }
      produced += d;
    }

    av_packet_unref(core->packet);
    rounds += 1;
  }

  if (core->file_eof && core->video_queue.empty() && core->audio_queue.empty()) {
    return -1;
  }

  return produced;
}

int core_poll_video_frame(
    Core* core,
    uint8_t** y,
    uint8_t** u,
    uint8_t** v,
    int* w,
    int* h,
    int* y_stride,
    int* uv_stride,
    double* pts) {
  if (!core || !y || !u || !v || !w || !h || !y_stride || !uv_stride || !pts) {
    return -1;
  }

  if (core->video_queue.empty()) {
    if (core->file_eof) {
      return -1;
    }
    return 0;
  }

  core->current_video = std::move(core->video_queue.front());
  core->video_queue.pop_front();
  core->has_video = true;

  *y = core->current_video.y.data();
  *u = core->current_video.u.data();
  *v = core->current_video.v.data();

  *w = core->current_video.width;
  *h = core->current_video.height;
  *y_stride = core->current_video.y_stride;
  *uv_stride = core->current_video.uv_stride;
  *pts = core->current_video.pts;

  return 1;
}

int core_poll_audio(Core* core, float** pcm, int* frames, double* pts) {
  if (!core || !pcm || !frames || !pts) {
    return -1;
  }

  if (core->audio_queue.empty()) {
    if (core->file_eof) {
      return -1;
    }
    return 0;
  }

  core->current_audio = std::move(core->audio_queue.front());
  core->audio_queue.pop_front();
  core->has_audio = true;

  *pcm = core->current_audio.pcm.data();
  *frames = core->current_audio.frames;
  *pts = core->current_audio.pts;
  return 1;
}

int core_seek_ms(Core* core, int64_t ms) {
  if (!core || !core->opened || !core->fmt) {
    return -1;
  }

  const int64_t ts = av_rescale_q(ms, AVRational{1, 1000}, AV_TIME_BASE_Q);
  const int ret = av_seek_frame(core->fmt, -1, ts, AVSEEK_FLAG_BACKWARD);
  if (ret < 0) {
    return ret;
  }

  if (core->vdec) {
    avcodec_flush_buffers(core->vdec);
  }
  if (core->adec) {
    avcodec_flush_buffers(core->adec);
  }

  clear_queues(core);
  core->file_eof = false;
  core->decoders_drained = false;
  return 0;
}

void core_flush(Core* core) {
  if (!core) {
    return;
  }

  clear_queues(core);

  if (core->vdec) {
    avcodec_flush_buffers(core->vdec);
  }
  if (core->adec) {
    avcodec_flush_buffers(core->adec);
  }

  core->file_eof = false;
  core->decoders_drained = false;
}

void core_close(Core* core) {
  if (!core) {
    return;
  }

  close_all(core);
  core->input_ring.clear();
  core->input_blob.clear();
  core->input_eof = false;
}

double core_get_duration_sec(const Core* core) {
  if (!core) {
    return 0.0;
  }
  return core->duration_sec;
}

}  // namespace wasm_player
