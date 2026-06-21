#include "snd.h"
#include <math.h>

float matched_filter(float* a, float* b, int len) {
    float match = -1e12;

    for (int i = 0; i < len; i++) {
        match = fmax(match, a[i] * b[i]);
    }

    return match;
}

float matched_filter_dBFS(int32_t* a, int32_t* b, int len) {
    float match = -1e12;

    for (int i = 0; i < len; i++) {
        float value = (a[i] / dBFS) * (b[i] / dBFS);

        match = fmax(match, value);
    }

    return match;
}

float matched_filter_interleaved(int32_t* buffer, int len) {
    float match = -1e12;

    for (int i = 0; i < len; i += 2) {
        float value = (buffer[i] / dBFS) * (buffer[i + 1] / dBFS);

        match = fmax(match, value);
    }

    return match;
}
