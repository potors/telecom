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
#define PROFILE(MSG, CALL) ({ \
    uint64_t start = esp_timer_get_time(); CALL; \
    float took = (esp_timer_get_time() - start) / 1000.0f; \
    ESP_LOGI("profiler", "%s [ln. %d] took %.03fms", MSG, __LINE__, took); \
})

#define MIC_SCK GPIO_NUM_22
#define MIC_WS GPIO_NUM_19
#define MIC1_SD GPIO_NUM_21
#define MIC2_SD GPIO_NUM_18

//TODO: builtin map visualization
#define LCD_SCK GPIO_NUM_1
#define LCD_MOSI GPIO_NUM_1
#define LCD_DC GPIO_NUM_1
#define LCD_RESET GPIO_NUM_1
#define LCD_CS GPIO_NUM_1

#define SAMPLE_RATE 48000
#define SAMPLES 128

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
        .data = 24,
        .slot = 32,
        .stereo = true,
        .shift = true,
        .slave = false,
    }, (i2s_pins_t) {
        .ws = MIC_WS,
        .sd = MIC1_SD,
        .sck = MIC_SCK,
    });

    // second channel should be slave to sync the inputs.
    i2s_t mic2 = i2s_init(1, SAMPLE_RATE, (i2s_opts_t) {
        .data = 24,
        .slot = 32,
        .stereo = false,
        .shift = true,
        .slave = true,
    }, (i2s_pins_t) {
        .ws = MIC_WS,
        .sd = MIC2_SD,
        .sck = MIC_SCK,
    });

    {
        char buffer[SAMPLES];
        i2s_channel_read(mic1.rx, buffer, sizeof(buffer), NULL, pdMS_TO_TICKS(100));
        i2s_channel_read(mic2.rx, buffer, sizeof(buffer), NULL, pdMS_TO_TICKS(100));
    }

    while (true) {
        ESP_LOGE(TAG, "free %.02fkB", heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024.0f);

        void* buffer1 = i2s_read(&mic1, SAMPLES);
        void* buffer2 = i2s_read(&mic2, SAMPLES);

        i2s_data_t data1 = i2s_parse(&mic1, buffer1, SAMPLES);
        i2s_data_t data2 = i2s_parse(&mic2, buffer2, SAMPLES);

        free(buffer1);
        free(buffer2);

        float* a = data1.l;
        float* b = data1.r;
        float* c = data2.l;

        ESP_LOGI(TAG, "mic(a): min/max=%.06f/%.06f rms=%.0f freq=%.0fHz",
            snd_min(a, SAMPLES), snd_max(a, SAMPLES),
            snd_rms(a, SAMPLES) * 0xFFFFFF,
            snd_zero_crossings(a, SAMPLES) * SAMPLE_RATE);

        ESP_LOGI(TAG, "mic(b): min/max=%.06f/%.06f rms=%.0f freq=%.0fHz",
            snd_min(b, SAMPLES), snd_max(b, SAMPLES),
            snd_rms(b, SAMPLES) * 0xFFFFFF,
            snd_zero_crossings(b, SAMPLES) * SAMPLE_RATE);

        ESP_LOGI(TAG, "mic(c): min/max=%.06f/%.06f rms=%.0f freq=%.0fHz",
            snd_min(c, SAMPLES), snd_max(c, SAMPLES),
            snd_rms(c, SAMPLES) * 0xFFFFFF,
            snd_zero_crossings(c, SAMPLES) * SAMPLE_RATE);


        match_t ab, ac, bc;

        PROFILE("ab matching", ab = snd_matched_filter(a, b, SAMPLES));
        PROFILE("ac matching", ac = snd_matched_filter(a, c, SAMPLES));
        PROFILE("bc matching", bc = snd_matched_filter(b, c, SAMPLES));

        ESP_LOGW(TAG, "ab match: %.0f%% (lag of %d samples - %.03fms)", ab.corr * 100, ab.lag, 1.0f / SAMPLE_RATE * ab.lag * 1000);
        ESP_LOGW(TAG, "ac match: %.0f%% (lag of %d samples - %.03fms)", ac.corr * 100, ac.lag, 1.0f / SAMPLE_RATE * ac.lag * 1000);
        ESP_LOGW(TAG, "bc match: %.0f%% (lag of %d samples - %.03fms)", bc.corr * 100, bc.lag, 1.0f / SAMPLE_RATE * bc.lag * 1000);

        free(a);
        free(b);
        free(c);


        // TODO: run the triangulation algorithm hosted on arcjth/tloc2

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
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
