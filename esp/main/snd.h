#pragma once

float snd_dot_product(float* a, float* b, int len);

typedef struct {
    int lag;
    float corr;
} match_t;

match_t snd_matched_filter(float* a, float* b, int samples, int sample_rate);

float snd_zero_crossings(float* buffer, int samples, int sample_rate);
float snd_rms(float* buffer, int samples);

float snd_min(float* buffer, int samples);
float snd_max(float* buffer, int samples);
