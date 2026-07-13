// See: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2s.html
// Also: https://esp32.com/viewtopic.php?t=15185

#include "i2s.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <esp_err.h>
#include <esp_log.h>
#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>

#define TAG "i2s"
#define LOCAL_LOG_LEVEL ESP_LOG_DEBUG

i2s_t i2s_init(int port, int sample_rate, i2s_opts_t opts, i2s_pins_t pins) {
    ESP_LOGI(TAG, "initializing i2s interface at %uHz of sample rate", sample_rate);
    i2s_chan_handle_t rx;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(port, opts.slave);
    i2s_new_channel(&chan_cfg, NULL, &rx);

    if (opts.stereo && opts.sides != I2S_BOTH) {
        ESP_LOGW(TAG, "stereo mode but only using %s side",
            opts.sides == I2S_LEFT ? "left" : "right");
    }

    int clk = (opts.bits % 32) ? 256 : 192;

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = sample_rate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = clk,
            .bclk_div = 8,
        },
        .slot_cfg = {
            .data_bit_width = opts.bits,
            .slot_bit_width = opts.bytes * 8,
            .slot_mode = opts.stereo + 1,
            .slot_mask = opts.stereo ? I2S_BOTH : opts.sides,
            .ws_width = opts.bytes * 8,
            .bit_shift = opts.shift,

            #ifdef CONFIG_IDF_TARGET_ESP32
                .msb_right = false,
            #else
                .left_align = true,
                .big_endian = false,
                .bit_order_lsb = false,
            #endif
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
    ESP_LOGI(TAG, "  master    = %s", !opts.slave ? "yes" : "no");

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx));

    return (i2s_t) {
        .rx = rx,
        .sample_rate = sample_rate,

        .opts = opts,
        .pins = pins,
    };
}

int32_t reconstruct_sample(uint8_t* buffer, int bytes) {
    int32_t sample = 0;

    // input endianess doesn't matter here
    // esp itself is lsb
    for (int i = 0; i < bytes; i++) {
        sample |= (typeof(sample)) buffer[i] << (i * 8);
    }

    // sign extend
    sample <<= (sizeof(sample) - bytes) * 8;
    sample >>= (sizeof(sample) - bytes) * 8;


    ESP_LOGD(TAG, "reconstructed sample: \x1b[%dm%08x << %02x %02x %02x %02x\x1b[m",
             32 - (buffer[bytes - 1] >> 7),
             sample,
             bytes > 3 ? buffer[3] : 0,
             bytes > 2 ? buffer[2] : 0,
             bytes > 1 ? buffer[1] : 0,
             buffer[0]);

    return sample;
}

i2s_buffer* i2s_read(i2s_t* i2s, int samples) {
    int len = i2s->opts.bits / 8;
    int channels = (i2s->opts.stereo && i2s->opts.sides == I2S_BOTH) + 1;
    ESP_LOGD(TAG, "reading %d samples of %d byte%s (%d channel%s)",
             samples,
             len, len > 1 ? "s" : "",
             channels, channels > 1 ? "s" : "");

    int size = samples * channels;
    uint8_t* buffer = malloc(size * len);

    esp_err_t err = i2s_channel_read(i2s->rx, buffer, size * len, NULL, 3000);
    if (err) {
        ESP_LOGE(TAG, "read error [%d]: %s", err, esp_err_to_name(err));

        free(buffer);
        return NULL;
    }

    #if CONFIG_LOG_DEFAULT_LEVEL_INFO >= ESP_LOG_DEBUG
        for (int i = 0; i < size * len;) {
            char str[64] = { 0 };

            char part[8] = { 0 };

            for (int c = 0; c < channels; c++) {
                sprintf(part, "\x1b[%dm", 32 - (buffer[i+2] >> 7));
                strcat(str, part);
                for (int n = 0; n < len; n++) {
                    sprintf(part, "%02x ", buffer[i + (len - n - 1)]);
                    strcat(str, part);
                }

                strcat(str, "  ");

                i += len;
            }

            strcat(str, "\x1b[m");
            ESP_LOGD(TAG, "%s", str);
        }
    #endif

    float* left = malloc(samples * sizeof(*left) * channels);
    float* right = channels == 2 ? &left[samples] : NULL;

    if (!right) ESP_LOGD(TAG, "mono mode: ignoring right side");

    ESP_LOGD(TAG, "left: %p, right: %p", left, right);

    for (int i = 0; i < samples; i++) {
        left[i] = reconstruct_sample(&buffer[i * len * channels], len) / (float) (1 << (i2s->opts.bits - 1));
        ESP_LOGD(TAG, "left [at %d] got: %f", i * len * channels, left[i]);

        if (!right) continue;

        right[i] = reconstruct_sample(&buffer[i * len * channels + len], len) / (float) (1 << (i2s->opts.bits - 1));
        ESP_LOGD(TAG, "right [at %d] got: %f", i * len * channels + len, right[i]);
    }

    free(buffer);

    #define HEAP(...) ({ typeof(__VA_ARGS__)* p = malloc(sizeof(*p)); *p = __VA_ARGS__; p; })
    return HEAP((i2s_buffer) {
        .left = left,
        .right = right,

        .samples = samples,
    });
}

void i2s_free(i2s_buffer* buffer) {
    free(buffer->left);
    free(buffer);
}
