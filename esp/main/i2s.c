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
#include <stdlib.h>
#include <esp_err.h>
#include <esp_log.h>

#define TAG "i2s"
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

i2s_t i2s_init(int port, int sample_rate, i2s_opts_t opts, i2s_pins_t pins) {
    i2s_chan_handle_t rx;

    i2s_chan_config_t cfg = I2S_CHANNEL_DEFAULT_CONFIG(port, opts.slave);

    cfg.dma_desc_num = 8;
    cfg.dma_frame_num = 128;
    #ifndef CONFIG_IDF_TARGET_ESP32
        if (opts.data % 3 == 0) chan_cfg.dma_frame_num = 96;
    #endif

    ESP_ERROR_CHECK(i2s_new_channel(&cfg, NULL, &rx));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(opts.data, opts.stereo + 1),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = pins.sck,
            .ws = pins.ws,
            .din = pins.sd,
            .dout = I2S_GPIO_UNUSED,
        },
    };

    #ifndef CONFIG_IDF_TARGET_ESP32
        std_cfg.slot_cfg.ws_width = opts.slot;
        std_cfg.slot_cfg.slot_bit_width = opts.slot;
    #endif

    if (opts.data % 3 == 0) std_cfg.clk_cfg.mclk_multiple = 192;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx));

    return (i2s_t) { rx, sample_rate, opts, pins };
}

void* i2s_read(i2s_t* i2s, int samples) {
    int channels = i2s->opts.stereo + 1;
    int size = samples * channels;

    int bytes = size *
        #ifdef CONFIG_IDF_TARGET_ESP32
            i2s->opts.slot / 8;
        #else
            i2s->opts.data / 8;
        #endif

    uint32_t* buf = malloc(bytes);

    ESP_LOGI(TAG, "reading %d samples [%d channels] (%d bytes)",
             samples, channels, bytes);

    ESP_ERROR_CHECK(i2s_channel_read(i2s->rx, buf, bytes, NULL, 5000));

    uint8_t* dbg = (void*) buf;
    for (int i = 0; i < 4; i++) {
        #ifdef CONFIG_IDF_TARGET_ESP32
            if (i2s->opts.stereo) {
                ESP_LOGD(TAG, "read: %02x %02x %02x %02x / %02x %02x %02x %02x :: (%08x / %08x)",
                        dbg[4*i + 0], dbg[4*i + 1], dbg[4*i + 2], dbg[4*i + 3],
                        dbg[4*i + 4], dbg[4*i + 5], dbg[4*i + 6], dbg[4*i + 7],
                        buf[2*i + 0], buf[2*i + 1]);
            } else {
                ESP_LOGD(TAG, "read: %02x %02x %02x %02x :: (%06x)",
                        dbg[4*i + 0], dbg[4*i + 1], dbg[4*i + 2], dbg[4*i + 3],
                        buf[i]);
            }
        #else
            if (i2s->opts.stereo) {
                ESP_LOGD(TAG, "read: %02x %02x %02x / %02x %02x %02x :: (%06x / %06x)",
                        dbg[4*i + 0], dbg[4*i + 1], dbg[4*i + 2],
                        dbg[4*i + 4], dbg[4*i + 5], dbg[4*i + 6],
                        buf[2*i + 0] & 0xFFFFF, buf[2*i + 1] & 0xFFFFF);
            } else {
                ESP_LOGD(TAG, "read: %02x %02x %02x :: (%06x)",
                        dbg[4*i + 0], dbg[4*i + 1], dbg[4*i + 2],
                        buf[i] & 0xFFFFF);
            }
        #endif
    }

    return buf;
}

float sample(uint8_t* buffer, int bytes) {
    #ifdef CONFIG_IDF_TARGET_ESP32
        buffer++;
    #endif

    int32_t sample = 0;

    for (int i = 0; i < bytes; i++) {
        sample |= (uint32_t) buffer[i] << (i * 8);
    }

    // sign extend
    sample <<= (sizeof(sample) - bytes) * 8;
    sample >>= (sizeof(sample) - bytes) * 8;

    return (float) sample / ((1 << (bytes * 8)) - 1);
}

i2s_data_t i2s_parse(i2s_t* i2s, void* buffer, int samples) {
    int data = i2s->opts.data / 8;

    #ifdef CONFIG_IDF_TARGET_ESP32
        int slot = i2s->opts.slot / 8;

        #define WIDTH slot
    #else
        #define WIDTH data
    #endif

    int channels = i2s->opts.stereo + 1;

    float* l = malloc(samples * sizeof(*l));
    float* r = !i2s->opts.stereo ? NULL
             : malloc(samples * sizeof(*r));

    for (int i = 0; i < samples; i++) {
        l[i] = sample(&((uint8_t*) buffer)[i * WIDTH * channels], data);

        if (!r) continue;

        r[i] = sample(&((uint8_t*) buffer)[i * WIDTH * channels + WIDTH], data);
    }

    return (i2s_data_t) { l, r };
}
