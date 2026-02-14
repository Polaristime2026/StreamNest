#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

int player_init(int sample_rate, int channels);
int player_feed(uint8_t* data, int len);
int player_decode_step();

int player_poll_video_frame(
    uint8_t** y,
    uint8_t** u,
    uint8_t** v,
    int* w,
    int* h,
    int* y_stride,
    int* uv_stride,
    double* pts);

int player_poll_audio(float** pcm, int* frames, double* pts);

int player_seek_ms(int64_t ms);
void player_flush();
void player_close();

double player_get_duration_sec();

#ifdef __cplusplus
}
#endif
