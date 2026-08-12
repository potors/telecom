#include "snd.h"
#include <math.h>
#include <float.h>
#include <esp_log.h>
#include <dsps_ccorr.h>

#define TAG "snd"

// TODO: padronize logging infra
#define TRACE(MSG, ...) ESP_LOGD(TAG, "(%s) " MSG, __func__, __VA_ARGS__)

float snd_min(float* buffer, int samples) {
    float min = 1.0f;

    while (samples--) {
        min = fminf(min, *buffer++);
    }

    return min;
}

float snd_max(float* buffer, int samples) {
    float max = -1.0f;

    while (samples--) {
        max = fmaxf(max, *buffer++);
    }

    return max;
}

float snd_freq(float* buffer, int samples) {
    int crossings = 0;

    for (int i = 1; i < samples; i++) {
        bool prev = 0 > buffer[i - 1];
        bool curr = 0 > buffer[i];

        crossings += prev != curr;
    }

    return (crossings / 2.0f) / samples;
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

match_t snd_match(float* a, float* b, int samples) {
    int len = samples * 2 - 1;
    float* v = malloc(len * sizeof(*v));
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

vec_t vec_add(vec_t a, vec_t b) {
    return (vec_t) {
        a.x + b.x,
        a.y + b.y,
    };
}

vec_t vec_mul(vec_t v, float k) {
    return (vec_t) {
        k * v.x,
        k * v.y,
    };
}

pol_t vec2pol(vec_t v) {
    return (pol_t) {
        sqrtf((v.x * v.x) + (v.y * v.y)),
        atan2f(v.y, v.x),
    };
}

pol_t pol_mul(pol_t a, pol_t b) {
    return (pol_t) {
        a.r * b.r,
        a.theta + b.theta,
    };
}

pol_t pol_div(pol_t a, pol_t b) {
    return (pol_t) {
        a.r / b.r,
        a.theta - b.theta,
    };
}

vec_t pol2vec(pol_t v) {
    return (vec_t) {
        v.r * cos(v.theta),
        v.r * sin(v.theta),
    };
}

#define n 3

// Gaussian Elimination
float* solve_linsys(float A[n][n], float b[n]) {
    float Ab[n][n + 1];

    // Copy values to augmented matrix
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            Ab[i][j] = A[i][j];
        }

        Ab[i][n] = b[i];
    }

    for (int p = 0; p < n; p++) {
        // Find highest pivot
        int maxRow = p;
        float maxVal = fabsf(Ab[p][p]);
        for (int i = p + 1; i < n; i++) {
            if (fabsf(Ab[i][p]) > maxVal) {
                maxVal = fabsf(Ab[i][p]);
                maxRow = i;
            }
        }

        // Swap rows
        for (int i = 0; i < n + 1; i++) {
            float tmp = Ab[p][i];
            Ab[p][i] = Ab[maxRow][i];
            Ab[maxRow][i] = tmp;
        }


        // Impossible system
        if (fabsf(Ab[p][p]) < FLT_EPSILON) return NULL;

        // Lower triangle
        for (int i = p + 1; i < n; i++) {
            float factor = Ab[i][p] / Ab[p][p];

            for (int j = p; j < n + 1; j++) {
                Ab[i][j] -= factor * Ab[p][j];
            }
        }
    }

    float* x = malloc(n * sizeof(*x));

    // Upper triangle
    for (int i = n - 1; i >= 0; i--) {
        float sum = 0.0f;

        for (int j = i + 1; j < n; j++) {
            sum += Ab[i][j] * x[j];
        }

        x[i] = (Ab[i][n] - sum) / Ab[i][i];
    }

    return x;
}

// TODO: this should take real distance in consideration
// TODO: delta should be given in seconds
float delta2distance(float delta) {
    return delta * 343;
}

#define DP(v) (((v).x * (v).x) + ((v).y * (v).y))
float* solve_tdoa(vec_t mics[n], float deltas[n]) {
    float A[n][n];
    float b[n];

    float xr = mics->x;
    float yr = mics->y;

    for (int i = 1; i < n; i++) {
        float xi = mics[i].x;
        float yi = mics[i].y;
        float qi = DP(mics[i]);

        float ri = deltas[i - 1];

        A[i][0] = 2.0 * (xi - xr);
        A[i][1] = 2.0 * (yi - yr);
        A[i][2] = 2.0 * ri;

        b[i - 1] = qi - (ri * ri);
    }

    for (int i = 0; i < 3; i++) {
        printf("[ %f %f %f | %f ]\n", A[i][0], A[i][1], A[i][2], b[i]);
    }

    return solve_linsys(A, b);
}

vec_t snd_locate(vec_t a, vec_t b, vec_t c, float A, float B, float C) {
    vec_t mics[n] = { a, b, c };
    float deltas[n] = { A, B, C };

    float* X = solve_tdoa(mics, deltas);
    if (!X) return (vec_t) { 0, 0 };

    float x = X[0];
    float y = X[1];

    return (vec_t) { x, y };
}
