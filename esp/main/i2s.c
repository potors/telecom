// See: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2s.html
// Also: https://esp32.com/viewtopic.php?t=15185

#include "i2s.h"
#include <stdlib.h>
#include <esp_err.h>
#include <esp_log.h>
#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>

#define TAG "i2s"

// These settings are for the INMP441 microphone.
i2s_t i2s_init(uint32_t sample_rate, i2s_opts_t opts, i2s_pins_t pins) {
    ESP_LOGI(TAG, "initializing i2s interface at %uHz of sample rate", sample_rate);
    i2s_chan_handle_t rx;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, NULL, &rx);

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = {
            .data_bit_width = opts.bits,
            .slot_bit_width = opts.bytes * 8,
            .slot_mode = opts.stereo + 1,
            .slot_mask = opts.sides,
            .ws_width = opts.bytes * 8,
            .bit_shift = opts.shift,
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = pins.sck,
            .ws = pins.ws,
            .dout = I2S_GPIO_UNUSED,
            .din = pins.sd,
        },
    };

    ESP_LOGI(TAG, "configuring i2s with these options:");
    ESP_LOGI(TAG, "  data bits = %d", opts.bits);
    ESP_LOGI(TAG, "  slot bits = %d", opts.bytes * 8);
    ESP_LOGI(TAG, "  sides     = %s", opts.sides == I2S_BOTH ? "both"
                                    : opts.sides == I2S_LEFT ? "left" : "right");
    ESP_LOGI(TAG, "  stereo    = %s", opts.stereo ? "yes" : "no");
    ESP_LOGI(TAG, "  bit shift = %s", opts.shift ? "yes" : "no");

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx));

    return (i2s_t) {
        .rx = rx,
        .sample_rate = sample_rate,

        .opts = opts,

        .i2s_port = chan_cfg.id,

        .pins = pins,
    };
}

void i2s_buffer(i2s_t* i2s, void* buffer, int samples) {
    int len = samples * i2s->opts.bytes;

    ESP_ERROR_CHECK(i2s_channel_read(i2s->rx, buffer, len, NULL, portMAX_DELAY));
}

void* i2s_read(i2s_t* i2s, int samples) {
    int len = samples * i2s->opts.bytes;

    void* buffer = malloc(len);
    i2s_buffer(i2s, buffer, samples);

    return buffer;
}
