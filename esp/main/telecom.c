#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <inttypes.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>

#include "freertos/projdefs.h"
#include "i2s.h"
#include "lcd.h"
#include "snd.h"

#define TAG "main"
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// TODO: make a compile-time stack based profiler
#define PROFILE(MSG, CALL) do { \
    uint64_t start = esp_timer_get_time(); \
    CALL; \
    ESP_LOGI("profiler", "%s [ln. %d] took %.03fms", MSG, __LINE__, \
        (esp_timer_get_time() - start) / 1000.0f); \
} while (false)

#define MIC_SCK GPIO_NUM_22
#define MIC1_SD GPIO_NUM_21
#define MIC1_WS GPIO_NUM_19
#define MIC2_SD GPIO_NUM_18
#define MIC2_WS GPIO_NUM_5

//TODO: builtin map visualization
#define LCD_SCK GPIO_NUM_1
#define LCD_MOSI GPIO_NUM_1
#define LCD_DC GPIO_NUM_1
#define LCD_RESET GPIO_NUM_1
#define LCD_CS GPIO_NUM_1

#define SAMPLE_RATE (48000 * 1)
#define SAMPLES (2 * 64 * 3)

void app_main() {
    // lcd_t lcd = lcd_init(320, 240, (lcd_opts_t) {
    //     .vertical = true,
    // }, (lcd_pins_t) {
    //     .cs = LCD_CS,
    //     .reset = LCD_RESET,
    //     .dc = LCD_DC,
    //     .mosi = LCD_MOSI,
    //     .sck = LCD_SCK,
    // });
    //
    // lcd_fill(&lcd, LCD_BLACK);

    i2s_t mic1 = i2s_init(0, SAMPLE_RATE, (i2s_opts_t) {
        .bits = 24,
        .bytes = sizeof(int32_t),
        .sides = I2S_BOTH,
        .stereo = true,
        .shift = true,
        .slave = false,
    }, (i2s_pins_t) {
        .ws = MIC1_WS,
        .sd = MIC1_SD,
        .sck = MIC_SCK,
    });

    // second channel should be slave to sync the inputs.
    i2s_t mic2 = i2s_init(1, SAMPLE_RATE, (i2s_opts_t) {
        .bits = 24,
        .bytes = sizeof(int32_t),
        .sides = I2S_LEFT,
        .stereo = false,
        .shift = true,
        .slave = true,
    }, (i2s_pins_t) {
        .ws = MIC2_WS,
        .sd = MIC2_SD,
        .sck = MIC_SCK,
    });

    while (true) {
        ESP_LOGE(TAG, "free %.02fkB", heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024.0f);

        ESP_LOGW(TAG, "reading %d samples", SAMPLES);
        i2s_buffer* buffer = i2s_read(&mic1, SAMPLES);
        if (!buffer) {
            vTaskDelay(1);
            continue;
        }

        // The time wasted on these is insignificant
        //   compared to the matched filter, so there's
        //   no need to remove them (good for debugging).
        float lmin = snd_min(buffer->left, buffer->samples);
        float lmax = snd_max(buffer->left, buffer->samples);
        float lrms = snd_rms(buffer->left, buffer->samples);
        float lfreq = snd_zero_crossings(buffer->left, buffer->samples, SAMPLE_RATE);

        float rmin = 0.0f;
        float rmax = 0.0f;
        float rrms = 0.0f;
        float rfreq = 0.0f;

        match_t match = { 0 };
        float mlmin = 0.0f, mrmin = 0.0f;
        float mlmax = 0.0f, mrmax = 0.0f;
        float mlrms = 0.0f, mrrms = 0.0f;
        float mlfreq = 0.0f, mrfreq = 0.0f;

        if (buffer->right) {
            rmin = snd_min(buffer->right, buffer->samples);
            rmax = snd_max(buffer->right, buffer->samples);
            rrms = snd_rms(buffer->right, buffer->samples);
            rfreq = snd_zero_crossings(buffer->right, buffer->samples, SAMPLE_RATE);

            match = snd_matched_filter(buffer->left, buffer->right, buffer->samples, SAMPLE_RATE);

            mlmin = snd_min(&buffer->left[MAX(match.lag, 0)], buffer->samples - MAX(match.lag, 0));
            mlmax = snd_max(&buffer->left[MAX(match.lag, 0)], buffer->samples - MAX(match.lag, 0));
            mlrms = snd_rms(&buffer->left[MAX(match.lag, 0)], buffer->samples - MAX(match.lag, 0));
            mlfreq = snd_zero_crossings(&buffer->left[MAX(match.lag, 0)], buffer->samples - MAX(match.lag, 0), SAMPLE_RATE);

            mrmin = snd_min(&buffer->right[MIN(match.lag, 0)], buffer->samples - MIN(match.lag, 0));
            mrmax = snd_max(&buffer->right[MIN(match.lag, 0)], buffer->samples - MIN(match.lag, 0));
            mrrms = snd_rms(&buffer->right[MIN(match.lag, 0)], buffer->samples - MIN(match.lag, 0));
            mrfreq = snd_zero_crossings(&buffer->right[MIN(match.lag, 0)], buffer->samples - MIN(match.lag, 0), SAMPLE_RATE);
        }

        ESP_LOGI(TAG, "left:  min/max=%.06f/%.06f rms=%.0f freq=%.0fHz", lmin, lmax, lrms * 0x800000, lfreq);
        if (buffer->right) {
            ESP_LOGI(TAG, "right: min/max=%.06f/%.06f rms=%.0f freq=%.0fHz", rmin, rmax, rrms * 0x800000, rfreq);

            if (match.corr != 0.0f) {
                ESP_LOGW(TAG, "match: %.0f%% (offsetted by %d samples / %.02fµs)", match.corr * 100, match.lag, 1.0f / SAMPLE_RATE * match.lag * 1000 * 1000);

                ESP_LOGI(TAG, "match left:  min/max=%.06f/%.06f rms=%.0f freq=%.0fHz", mlmin, mlmax, mlrms * 0x800000, mlfreq);
                ESP_LOGI(TAG, "match right: min/max=%.06f/%.06f rms=%.0f freq=%.0fHz", mrmin, mrmax, mrrms * 0x800000, mrfreq);
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(800));
        }

        // TODO: run the triangulation algorithm hosted on arcjth/tloc2

        i2s_free(buffer);

        // TODO: make a header only for protocol communication
        // uint16_t data[] = { SAMPLES, mic1.opts.bytes };
        // uint8_t event[] = { 0xED, 0x80 + (sizeof(data) / sizeof(*data)) };
        //
        // PROFILE("log to serial", {
        //     fwrite(event, sizeof(*event), sizeof(event) / sizeof(*event), stdout);
        //     fwrite(data, sizeof(*data), sizeof(data) / sizeof(*data), stdout);
        //     fwrite(samples[i], mic1.opts.bytes, SAMPLES, stdout);
        //
        //     // TODO: wait for response to not
        //     //       output unnecesssary garbage
        //
        //     fflush(stdout);
        //     fsync(fileno(stdout));
        //
        //     vTaskDelay(1);
        // });

        // printf("\x1b[3A");
        vTaskDelay(1);
    }
}
