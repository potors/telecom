#pragma once
#include <driver/gpio.h>
#include <driver/i2s_types.h>

typedef struct i2s_pins_t {
    gpio_num_t ws;
    gpio_num_t sd;
    gpio_num_t sck;
} i2s_pins_t;

typedef struct i2s_opts_t {
    uint16_t bits   : 6;
    uint16_t bytes  : 3;
     uint8_t sides  : 2;
        bool stereo : 1;
        bool shift  : 1;
} i2s_opts_t;

typedef struct i2s_t {
    i2s_chan_handle_t rx;
    uint32_t sample_rate;

    int i2s_port;

    i2s_opts_t opts;
    i2s_pins_t pins;
} i2s_t;

#define I2S_LEFT  0b01
#define I2S_RIGHT 0b10
#define I2S_BOTH  0b11

i2s_t i2s_init(uint32_t sample_rate, i2s_opts_t opts, i2s_pins_t pins);

void i2s_buffer(i2s_t* i2s, void* buffer, int len);
void* i2s_read(i2s_t* i2s, int samples);
