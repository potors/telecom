#pragma once
#include <driver/gpio.h>
#include <driver/i2s_types.h>

typedef struct i2s_pins_t {
    gpio_num_t ws;
    gpio_num_t sd;
    gpio_num_t sck;
} i2s_pins_t;

typedef struct i2s_opts_t {
    uint16_t bits   : 6; // data bits
    uint16_t bytes  : 3; // slot bytes
     uint8_t sides  : 2; // left, right or both

        bool stereo : 1; // stereo off and  left side -> continuous left
                         // stereo off and right side -> continuous right
                         // stereo off and both sides -> mixed left
                         // stereo on  and  left side ->   left and 0-right
                         // stereo on  and right side -> 0-left and   right
                         // stereo on  and both sides ->   left and   right

        bool shift  : 1; // bit shift
        bool slave  : 1; // master or slave
    uint16_t __unused : 2;
} i2s_opts_t;

typedef struct i2s_t {
    i2s_chan_handle_t rx;
    int sample_rate;

    i2s_opts_t opts;
    i2s_pins_t pins;
} i2s_t;

#define I2S_LEFT  0b01
#define I2S_RIGHT 0b10
#define I2S_BOTH  0b11

i2s_t i2s_init(int port, int sample_rate, i2s_opts_t opts, i2s_pins_t pins);

typedef struct {
    float* left;
    float* right;

    int samples;
} i2s_buffer;

i2s_buffer* i2s_read(i2s_t* i2s, int samples);
void i2s_free(i2s_buffer* buffer);
