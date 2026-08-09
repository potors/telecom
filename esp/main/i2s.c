// See: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2s.html
// Also: https://esp32.com/viewtopic.php?t=15185

// TODO: fix this file to allow both versions of i2s_std
//       to work together:
//          - esp32 sends padded bytes
//          - any other sends packed bytes
//          - theres also a slot_cfg field conflicts

// TODO: check the behavior from both when applying
//       an over-16kHz wave and sub-100Hz ones

#include "i2s.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>
#include <esp_err.h>
#include <esp_log.h>
#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>

#define TAG "i2s"
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

i2s_t i2s_init(int port, int sample_rate, i2s_opts_t opts, i2s_pins_t pins) {
    ESP_LOGI(TAG, "initializing i2s interface at %uHz of sample rate", sample_rate);
    i2s_chan_handle_t rx;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(port, opts.slave);
    i2s_new_channel(&chan_cfg, NULL, &rx);

    if (opts.stereo && opts.sides != I2S_BOTH) {
        ESP_LOGW(TAG, "stereo mode but only using %s side",
            opts.sides == I2S_LEFT ? "left" : "right");
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        // .clk_cfg = {
        //     .sample_rate_hz = sample_rate,
        //     .clk_src = I2S_CLK_SRC_DEFAULT,
        //     .mclk_multiple = clk,
        //     .bclk_div = 8,
        // },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(opts.bits, opts.stereo + 1),
        // .slot_cfg = {
        //     .data_bit_width = opts.bits,
        //     .slot_bit_width = opts.bytes * 8,
        //     .slot_mode = opts.stereo + 1,
        //     .slot_mask = opts.stereo ? I2S_BOTH : opts.sides,
        //     .ws_width = opts.bytes * 8,
        //     .bit_shift = opts.shift,
        //
        //     #ifdef CONFIG_IDF_TARGET_ESP32
        //         .msb_right = false,
        //     #else
        //         .left_align = true,
        //         .big_endian = false,
        //         .bit_order_lsb = false,
        //     #endif
        // },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = pins.sck,
            .ws = pins.ws,
            .dout = I2S_GPIO_UNUSED,
            .din = pins.sd,
        },
    };

    int clk = (opts.bits % 24) ? 256 : 192;
    std_cfg.clk_cfg.mclk_multiple = clk;

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
    assert(bytes >= 1 && bytes <= 4);

    // FIXME: fuck it for now
    #ifdef CONFIG_IDF_TARGET_ESP32
        int32_t espraw = buffer[3] << 24
                        | buffer[2] << 16
                        | buffer[1] << 8;

        return espraw >> 8;
    #endif

    int32_t sample = 0;

    for (int i = 0; i < bytes; i++) {
        sample |= (uint32_t) buffer[i] << (i * 8);
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

    #ifdef CONFIG_IDF_TARGET_ESP32
    len = i2s->opts.bytes;
    #endif

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

    // #if CONFIG_LOG_DEFAULT_LEVEL_INFO >= ESP_LOG_DEBUG
    //     printf("first samples:\n");
    //     for (int i = 0; i < MIN(samples, 10 * sizeof(int32_t)); i += sizeof(int32_t)) {
    //         uint8_t a = buffer[i + 0];
    //         uint8_t b = buffer[i + 1];
    //         uint8_t c = buffer[i + 2];
    //         uint8_t d = buffer[i + 3];
    //
    //         printf("sample %d: %02x %02x %02x %02x\n", i, a, b, c, d);
    //     }
    // #endif

    float* left = malloc(samples * sizeof(*left) * channels);
    float* right = channels == 2 ? &left[samples] : NULL;
    if (!right) ESP_LOGD(TAG, "mono mode: ignoring right side");

    ESP_LOGD(TAG, "left: %p, right: %p", left, right);

    for (int i = 0; i < samples; i++) {
        int32_t l = reconstruct_sample(&buffer[i * len * channels], len);
        left[i] = l / (float) (1 << (i2s->opts.bits - 1));
        if (i < 10) ESP_LOGD(TAG, "left [at %d] got: %f (%"PRIi32")", i * len * channels, left[i], l);

        if (!right) continue;

        int32_t r = reconstruct_sample(&buffer[i * len * channels + len], len);
        right[i] = r / (float) (1 << (i2s->opts.bits - 1));
        if (i < 10) ESP_LOGD(TAG, "right [at %d] got: %f (%"PRIi32")", i * len * channels + len, right[i], r);
    }

    free(buffer);

    i2s_buffer* data = malloc(sizeof(*data));

    data->left = left;
    data->right = right;
    data->samples = samples;

    return data;
}

void i2s_free(i2s_buffer* buffer) {
    // right => left memory allocation
    free(buffer->left);
    free(buffer);
}
