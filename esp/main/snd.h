#pragma once

float snd_min(float* buffer, int samples);
float snd_max(float* buffer, int samples);

float snd_freq(float* buffer, int samples);
float snd_rms(float* buffer, int samples);

typedef struct {
    int lag;
    float corr;
} match_t;

match_t snd_match(float* a, float* b, int samples);

typedef struct {
    float x, y;
} vec_t;

typedef struct {
    float r, theta;
} pol_t;

vec_t vec2add(vec_t, vec_t);
vec_t vec2mul(vec_t, float);
pol_t vec2pol(vec_t);

pol_t pol2mul(pol_t, pol_t);
pol_t pol2div(pol_t, pol_t);
vec_t pol2vec(pol_t);

#define MICA_POS ((vec_t) { 0.0f, 1.0f })
#define MICB_POS ((vec_t) { 1.0f, 0.0f })
#define MICC_POS ((vec_t) { 1.0f, 1.0f })

// Locate Sound Origin
//
// One microphone is fixed at "[x=0, y=0]" and it's used
// for others to deduct their positions and called _BASE_.
//
// TODO: Self-locate microphones with some signal.
//
// Required data:
//
//   `a` = position of 1st mic relative to _BASE_.
//   `b` = position of 2nd mic relative to _BASE_.
//   `c` = position of 3rd mic relative to _BASE_.
//
//   `A` = difference R from 1st mic relative to _BASE_.
//   `B` = difference R from 2nd mic relative to _BASE_.
//   `C` = difference R from 3rd mic relative to _BASE_.
vec_t snd_locate(vec_t a, vec_t b, vec_t c, float A, float B, float C);
