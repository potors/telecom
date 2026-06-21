#include <stdio.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <esp_random.h>
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>

#include "i2s.h"
#include "lcd.h"
#include "snd.h"

#define PROFILE(CALL) do { \
    uint64_t start = esp_timer_get_time(); \
    CALL; \
    ESP_LOGD("profiler", "'%s' took %.03fms", #CALL, \
        (esp_timer_get_time() - start) / 1000.0f); \
} while (false);

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

    #define MIC1_SD GPIO_NUM_0
    #define MIC1_WS GPIO_NUM_1
    #define MIC1_SCK GPIO_NUM_2

    #define LCD_SCK GPIO_NUM_9
    #define LCD_MOSI GPIO_NUM_6
    #define LCD_DC GPIO_NUM_10
    #define LCD_RESET GPIO_NUM_20
    #define LCD_CS GPIO_NUM_21
#endif

#define SAMPLE_RATE (48000 * 2)
#define SAMPLES (1024 * 1)

float iir(float x, float last) {
    #define a 0.3f

    // y[n] = (alpha * x[n]) + ((1 - alpha) * y[n-1])
    return (a * x) + ((1.0f - a) * last);

    #undef a
}

void app_main() {
    lcd_t lcd = lcd_init(320, 240, (lcd_opts_t) {
        .on = false,
        .vertical = true,
    }, (lcd_pins_t) {
        .cs = LCD_CS,
        .reset = LCD_RESET,
        .dc = LCD_DC,
        .mosi = LCD_MOSI,
        .sck = LCD_SCK,
    });

    lcd_fill(&lcd, LCD_WHITE);

    i2s_t mic = i2s_init(SAMPLE_RATE, (i2s_opts_t) {
        .bits = 24,
        .bytes = sizeof(uint32_t),
        .sides = I2S_BOTH,
        .stereo = true,
        .shift = true,
    }, (i2s_pins_t) {
        .ws = MIC1_WS,
        .sd = MIC1_SD,
        .sck = MIC1_SCK,
    });

    static int32_t buffer[SAMPLES * 2];

    while (true) {
        PROFILE(i2s_buffer(&mic, buffer, sizeof(buffer) / sizeof(*buffer)));

        static float left[SAMPLES];
        static float right[SAMPLES];

        for (int i = 0; i < SAMPLES * 2; i += 2) {
            left[i] = buffer[i] / dBFS;
            right[i] = buffer[i + 1] / dBFS;
        }

        static float mean = 0.0f;
        float min = 1e12;
        float max = -1e12;

        float match = matched_filter(left, right, SAMPLES);

        mean = iir(match, mean);

        for (int i = 0; i < SAMPLES; i++) {
            min = fmin(min, left[i]);
            max = fmax(max, left[i]);
        }

        printf("match: %.03f, mean: %.03f, min: %.03f, max: %.03f\n\x1b[F", match, mean, min, max);

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
