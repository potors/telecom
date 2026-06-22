#include "snd.h"
#include <math.h>

float matched_filter(float* a, float* b, int len) {
    float match = -1e12;

    for (int i = 0; i < len; i++) {
        float x = a[i];
        float y = b[i];

        match = fmaxf(match, x * y);
    }

    return match;
}

float matched_filter_dBFS(int32_t* a, int32_t* b, int len) {
    float match = -1e12;

    for (int i = 0; i < len; i++) {
        float x = ((a[i] << 8) >> 8) / dBFS;
        float y = ((b[i] << 8) >> 8) / dBFS;

        match = fmaxf(match, x * y);
    }

    return match;
}

float matched_filter_interleaved(int32_t* buffer, int len) {
    float match = -1e12;

    for (int i = 0; i < len; i += 2) {
        float x = ((buffer[i + 0] << 8) >> 8) / dBFS;
        float y = ((buffer[i + 1] << 8) >> 8) / dBFS;

        match = fmaxf(match, x * y);
    }

    return match;
}
