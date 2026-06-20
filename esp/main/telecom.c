#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <esp_err.h>
#include <esp_random.h>

#include "i2s.h"
#include "perf.h"

#include "screen.h"

#include "matched_filter.h"

#define SAMPLE_RATE 48000

#define BUFFER_LEN 1024
#define BUFFER_SIZE sizeof(int32_t) * BUFFER_LEN

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


void app_main() {
    screen(LCD_SCK, LCD_MOSI, -1, 320, 240, LCD_DC, LCD_CS, 20 * 1000 * 1000, LCD_RESET);
    i2s_chan_handle_t rx = i2s(SAMPLE_RATE, MIC1_WS, MIC1_SD, MIC1_SCK);

    static int32_t buffer[BUFFER_LEN];

    printf("i'm alive\n\n\n\n\n\n\n\n");

    while (true) {
        uint32_t r = esp_random() % 255;
        uint32_t g = esp_random() % 255;
        uint32_t b = esp_random() % 255;

        screen_fill(r, g, b);

        printf("\x1b[7A");

        tic();
        ESP_ERROR_CHECK(i2s_channel_read(rx, buffer, sizeof(buffer), NULL, portMAX_DELAY));
        uint64_t took = tac_us();

        printf("read took %lluµs (%lluµs per side) [buffer of %u elements - %u per side]\n", took, took / 2, BUFFER_LEN, BUFFER_LEN / 2);
        printf("\tran with %lluHz of sampling rate\n", took / BUFFER_SIZE * 2);

        ccmax_t matches = matched_filter_mixed_buffer(buffer, BUFFER_LEN / 2);
        printf("matched filter results\n");
        printf("\tleft mic: %f\n", matches.a);
        printf("\tright mic: %f\n", matches.b);

        static float at_max = -999;
        static float at_min = 999;
        at_max = max(at_max, matches.a);
        at_min = min(at_min, matches.a);

        printf("atmin: %f\t\tatmax: %f\n", at_min, at_max);

        vTaskDelay(1);
        // vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
