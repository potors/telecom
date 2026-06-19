#pragma once
#include <stdlib.h>

#define dBFS_MAXVAL 0xFFFFFE
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

typedef struct {
    float a;
    float b;
} ccmax_t; // Cross-Correlation Maxes

static ccmax_t matched_filter_mixed_buffer(int32_t* mixed_buffer, int len) {
    int32_t max_a = 0;
    int32_t max_b = 0;

    for (int i = 0; i < len; i++) {
        int32_t a = mixed_buffer[i + 0];
        int32_t b = mixed_buffer[i + 1];

        max_a = max(a, max_a);
        max_b = max(b, max_b);
    }

    return (ccmax_t) {
        .a = max_a / (float) dBFS_MAXVAL,
        .b = max_b / (float) dBFS_MAXVAL,
    };
}

static float matched_filter_single_buffer(int32_t* buffer, int len) {
    int32_t max_v = 0;

    for (int i = 0; i < len; i++) {
        float v = buffer[i];

        max_v = max(v, max_v);
    }

    return max_v / (float) dBFS_MAXVAL;
}
