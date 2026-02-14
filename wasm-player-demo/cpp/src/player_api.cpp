#include "player_api.h"

#include <emscripten/emscripten.h>

#include "ffmpeg_wrap.h"

namespace {
wasm_player::Core* g_core = nullptr;
}

extern "C" {

EMSCRIPTEN_KEEPALIVE int player_init(int sample_rate, int channels) {
  if (!g_core) {
    g_core = wasm_player::core_create();
  }
  return wasm_player::core_init(g_core, sample_rate, channels);
}

EMSCRIPTEN_KEEPALIVE int player_feed(uint8_t* data, int len) {
  if (!g_core) {
    return -1;
  }
  return wasm_player::core_feed(g_core, data, len);
}

EMSCRIPTEN_KEEPALIVE int player_decode_step() {
  if (!g_core) {
    return -1;
  }
  return wasm_player::core_decode_step(g_core);
}

EMSCRIPTEN_KEEPALIVE int player_poll_video_frame(
    uint8_t** y,
    uint8_t** u,
    uint8_t** v,
    int* w,
    int* h,
    int* y_stride,
    int* uv_stride,
    double* pts) {
  if (!g_core) {
    return -1;
  }
  return wasm_player::core_poll_video_frame(g_core, y, u, v, w, h, y_stride, uv_stride, pts);
}

EMSCRIPTEN_KEEPALIVE int player_poll_audio(float** pcm, int* frames, double* pts) {
  if (!g_core) {
    return -1;
  }
  return wasm_player::core_poll_audio(g_core, pcm, frames, pts);
}

EMSCRIPTEN_KEEPALIVE int player_seek_ms(int64_t ms) {
  if (!g_core) {
    return -1;
  }
  return wasm_player::core_seek_ms(g_core, ms);
}

EMSCRIPTEN_KEEPALIVE void player_flush() {
  if (!g_core) {
    return;
  }
  wasm_player::core_flush(g_core);
}

EMSCRIPTEN_KEEPALIVE void player_close() {
  if (!g_core) {
    return;
  }
  wasm_player::core_close(g_core);
  wasm_player::core_destroy(g_core);
  g_core = nullptr;
}

EMSCRIPTEN_KEEPALIVE double player_get_duration_sec() {
  if (!g_core) {
    return 0.0;
  }
  return wasm_player::core_get_duration_sec(g_core);
}

}  // extern "C"
