#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <inttypes.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>

#include "i2s.h"
#include "lcd.h"
#include "snd.h"

#define TAG "main"

// TODO: make an compile-time stack based profiler
#define PROFILE(MSG, CALL) do { \
    uint64_t start = esp_timer_get_time(); \
    CALL; \
    ESP_LOGI("profiler", "%s [ln. %d] took %.03fms", MSG, __LINE__, \
        (esp_timer_get_time() - start) / 1000.0f); \
} while (false)

#if defined CONFIG_IDF_TARGET_ESP32
    #define MIC1_WS GPIO_NUM_4
    #define MIC1_SD GPIO_NUM_2
    #define MIC1_SCK "todo"
#endif

#if defined CONFIG_IDF_TARGET_ESP32C3 /*
              .  5 -|----|||----|- 5V .
     LCD_MOSI .  6 -|    |||    |- 0V . in
              .  7 -|  -o- -o-  |- 3V . out
              .  8 -|    /^\    |- 4  .
      LCD_SCK .  9 -|   /   \   |- 3  .
       LCD_DC . 10 -|   \   /   |- 2  . MIC_SCK
    LCD_RESET . 20 -| =  \v/    |- 1  . MIC_WS
       LCD_CS . 21 -|----===----|- 0  . MIC_SD */

    // #define MIC1_SD GPIO_NUM_0
    // #define MIC1_WS GPIO_NUM_1
    // #define MIC1_SCK GPIO_NUM_2

    #define MIC1_SD GPIO_NUM_1
    #define MIC1_WS GPIO_NUM_0
    #define MIC1_SCK GPIO_NUM_3

    // #define LCD_SCK GPIO_NUM_9
    // #define LCD_MOSI GPIO_NUM_6
    // #define LCD_DC GPIO_NUM_10
    // #define LCD_RESET GPIO_NUM_20
    // #define LCD_CS GPIO_NUM_21
#endif

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

    i2s_t mic = i2s_init(0, SAMPLE_RATE, (i2s_opts_t) {
        .bits = 24,
        .bytes = sizeof(uint32_t),
        .sides = I2S_BOTH,
        .stereo = true,
        .shift = true,
        .slave = false,
    }, (i2s_pins_t) {
        .ws = MIC1_WS,
        .sd = MIC1_SD,
        .sck = MIC1_SCK,
    });

    while (true) {
        // ESP_LOGE(TAG, "free %.02fkB", heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024.0f);

        ESP_LOGW(TAG, "reading %d samples", SAMPLES);
        i2s_buffer* buffer = i2s_read(&mic, SAMPLES);
        if (!buffer) {
            vTaskDelay(1);
            continue;
        }

        // printf("left\n");
        // for (int i = 0; i < buffer->samples; i++) {
        //     printf("%f ", buffer->left[i]);
        // } printf("\n");

        // printf("right\n");
        // for (int i = 0; i < buffer->samples; i++) {
        //     printf("%f ", (buffer->right[i]));
        // } printf("\n");

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
        }

        i2s_free(buffer);

        // TODO: make a header only for protocol communication
        // uint16_t data[] = { SAMPLES, mic.opts.bytes };
        // uint8_t event[] = { 0xED, 0x80 + (sizeof(data) / sizeof(*data)) };
        //
        // PROFILE("log to serial", {
        //     fwrite(event, sizeof(*event), sizeof(event) / sizeof(*event), stdout);
        //     fwrite(data, sizeof(*data), sizeof(data) / sizeof(*data), stdout);
        //     fwrite(samples[i], mic.opts.bytes, SAMPLES, stdout);
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
