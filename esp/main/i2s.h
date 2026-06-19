#pragma once
#include "driver/i2s_types.h"
#include <esp_err.h>

// See: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2s.html
// Also: https://esp32.com/viewtopic.php?t=15185
#include <driver/i2s_std.h>
#include <driver/gpio.h>
#include <driver/uart.h>

// These settings are for the INMP441 microphone.
static i2s_chan_handle_t i2s(uint32_t sample_rate, gpio_num_t ws, gpio_num_t sd, gpio_num_t sck) {
    i2s_chan_handle_t rx;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, NULL, &rx);

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_24BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = 32,
            .ws_pol = false,
            .bit_shift = true,

            #if defined CONFIG_IDF_TARGET_ESP32
                .msb_right = false,
            #endif
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = sck,
            .ws = ws,
            .dout = I2S_GPIO_UNUSED,
            .din = sd,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx));

    return rx;
}
