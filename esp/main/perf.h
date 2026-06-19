#pragma once
#include <esp_timer.h>

static int64_t _tictac;

static inline void tic() {
    _tictac = esp_timer_get_time();
}

static inline uint64_t tac_us() { return esp_timer_get_time() - _tictac; }
static inline uint64_t tac_ms() { return tac_us() * 1000; }
static inline uint64_t tac_sec() { return tac_ms() * 1000; }

static inline float tac() {
    return (float) tac_us() / (1000 * 1000);
}
