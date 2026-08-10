#pragma once
#include <driver/gpio.h>
#include <driver/i2s_std.h>

typedef struct i2s_pins_t {
    gpio_num_t ws;
    gpio_num_t sd;
    gpio_num_t sck;
} i2s_pins_t;

#define I2S_LEFT 0
#define I2S_RIGHT 1

typedef struct i2s_opts_t {
    uint16_t data   : 6; // data bits
    uint16_t slot   : 6; // slot bits
        bool stereo : 1; // uses two channels
        bool shift  : 1; // bit shift
        bool slave  : 1; // master or slave
} i2s_opts_t;

typedef struct i2s_t {
    i2s_chan_handle_t rx;
    int sample_rate;

    i2s_opts_t opts;
    i2s_pins_t pins;
} i2s_t;

i2s_t i2s_init(int port, int sample_rate, i2s_opts_t opts, i2s_pins_t pins);

void* i2s_read(i2s_t* i2s, int samples);

typedef struct {
    float* l;
    float* r;
} i2s_data_t;

i2s_data_t i2s_parse(i2s_t* i2s, void* buffer, int samples);
