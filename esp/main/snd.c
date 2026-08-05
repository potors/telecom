#include "snd.h"
#include <stdio.h>
#include <math.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

#define TAG "snd"
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// TODO: padronize logging infra
#define TRACE(MSG, ...) ESP_LOGD(TAG, "(%s) " MSG, __func__, __VA_ARGS__)

float snd_dot_product(float* a, float* b, int len) {
    TRACE("calculating dot product between two vector of %d elements", len);
    float dot = 0.0f;
    float A = 0.0f;
    float B = 0.0f;

    for (int i = 0; i < len; i++) {
        dot += a[i] * b[i];

        A += a[i] * a[i];
        B += b[i] * b[i];
    }

    dot /= sqrtf(A * B);

    TRACE("vector [a] magnitude: %f", A);
    TRACE("vector [b] magnitude: %f", B);
    TRACE("got %f", dot);
    return dot;
}

// TODO: check float operations time length
match_t snd_matched_filter(float* a, float* b, int samples, int sample_rate) {
    TRACE("matching two signals (%d samples each) at %dHz", samples, sample_rate);
    float best_corr = 0.0f;
    int best_lag = 0;

    #define SKIPS 1
    #define DIVISOR 4

    for (int lag = -samples / DIVISOR; lag < samples / DIVISOR; lag += SKIPS) {
        float corr = snd_dot_product(
            &a[MAX(lag, 0)],
            &b[MIN(lag, 0)],
            samples - abs(lag) * 2);

        if (corr > best_corr) {
            best_corr = corr;
            best_lag = lag;
        }

        vTaskDelay(1);
    }

    TRACE("best lag: %d, best corr: %d", best_lag, best_corr);
    return (match_t) {
        .lag = best_lag,
        .corr = best_corr,
    };
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


