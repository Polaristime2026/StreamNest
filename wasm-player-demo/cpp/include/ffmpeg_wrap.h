#pragma once

#include <cstdint>

namespace wasm_player {

struct Core;

Core* core_create();
void core_destroy(Core* core);

int core_init(Core* core, int sample_rate, int channels);
int core_feed(Core* core, const uint8_t* data, int len);
int core_decode_step(Core* core);

int core_poll_video_frame(
    Core* core,
    uint8_t** y,
    uint8_t** u,
    uint8_t** v,
    int* w,
    int* h,
    int* y_stride,
    int* uv_stride,
    double* pts);

int core_poll_audio(Core* core, float** pcm, int* frames, double* pts);

int core_seek_ms(Core* core, int64_t ms);
void core_flush(Core* core);
void core_close(Core* core);

double core_get_duration_sec(const Core* core);

}  // namespace wasm_player
