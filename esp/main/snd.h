#pragma once
#include <stdint.h>

#define dBFS (0x1p24 - 1)

float matched_filter(float* a, float* b, int len);
float matched_filter_dBFS(int32_t* a, int32_t* b, int len);
float matched_filter_interleaved(int32_t* buffer, int len);
