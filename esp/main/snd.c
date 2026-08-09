#include "snd.h"
#include <math.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <dsps_ccorr.h>

#define TAG "snd"
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// TODO: padronize logging infra
#define TRACE(MSG, ...) ESP_LOGD(TAG, "(%s) " MSG, __func__, __VA_ARGS__)

match_t snd_matched_filter(float* a, float* b, int samples) {
    int len = samples * 2 - 1;
    float* v = malloc(sizeof(*v) * len);
    dsps_ccorr_f32_ae32(b, samples, a, samples, v);

    float lag = 0;
    float corr = -1;
    for (int i = 0; i < len; i++) {
        float x = v[i];
        if (x > corr) {
            lag = i - (samples - 1);
            corr = x;
        }
    }

    free(v);

    float A = 0;
    float B = 0;
    for (int i = 0; i < samples; i++) {
        A += a[i] * a[i];
        B += b[i] * b[i];
    }

    corr /= sqrtf(A * B);

    TRACE("lag: %f, corr: %f", lag, corr);
    return (match_t) { lag, corr };
}

float snd_zero_crossings(float* buffer, int samples, int sample_rate) {
    TRACE("calculating frequency from %d samples at %dHz", samples, sample_rate);
    int crossings = 0;

    for (int i = 1; i < samples; i++) {
        bool prev = 0 > buffer[i - 1];
        bool curr = 0 > buffer[i];

        crossings += prev != curr;
    }

    float freq = (crossings / 2.0f) / samples * sample_rate;

    TRACE("got %fHz", freq);
    return freq;
}

float snd_rms(float* buffer, int samples) {
    TRACE("calculating rms value from %d samples", samples);
    float rms = 0.0f;

    for (int i = 0; i < samples; i++) {
        rms += powf(buffer[i], 2);
    }

    rms = sqrt(rms / samples);

    TRACE("got %f rms", rms);
    return rms;
}

float snd_min(float* buffer, int samples) {
    TRACE("searching min value from %d samples", samples);
    float min = 1.0f;

    while (samples--) {
        min = fminf(min, *buffer++);
    }

    TRACE("got min of %f", min);
    return min;
}

float snd_max(float* buffer, int samples) {
    TRACE("searching max value from %d samples", samples);
    float max = -1.0f;

    while (samples--) {
        max = fmaxf(max, *buffer++);
    }

    TRACE("got max of %f", max);
    return max;
}
