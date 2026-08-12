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

// #define MIC_SCK GPIO_NUM_18
// #define MIC_WS GPIO_NUM_14
// #define MIC_SD1 GPIO_NUM_13
// #define MIC_SD2 GPIO_NUM_12

#define MIC_SCK GPIO_NUM_18
#define MIC_WS GPIO_NUM_32
#define MIC_SD1 GPIO_NUM_4
#define MIC_SD2 GPIO_NUM_33

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
    //
    // second channel should be slave to sync the inputs.
    i2s_t mic2 = i2s_init(1, SAMPLE_RATE, (i2s_opts_t) {
        .data = 24,
        .slot = 32,
        .stereo = true,
        .shift = true,
        .slave = true,
    }, (i2s_pins_t) {
        .ws = MIC_WS,
        .sd = MIC_SD2,
        .sck = MIC_SCK,
    });

    vTaskDelay(pdMS_TO_TICKS(5000));

    i2s_t mic1 = i2s_init(0, SAMPLE_RATE, (i2s_opts_t) {
        .data = 24,
        .slot = 32,
        .stereo = true,
        .shift = true,
        .slave = false,
    }, (i2s_pins_t) {
        .ws = MIC_WS,
        .sd = MIC_SD1,
        .sck = MIC_SCK,
    });

    // { // Syncronize DMA buffers? I don't know if this works actually...
    //     int32_t* buffer = malloc(SAMPLES * sizeof(*buffer));
    //     i2s_channel_read(mic1.rx, buffer, SAMPLES, NULL, pdMS_TO_TICKS(100));
    //     i2s_channel_read(mic2.rx, buffer, SAMPLES, NULL, pdMS_TO_TICKS(100));
    //     free(buffer);
    // }

    while (true) {
        ESP_LOGE(TAG, "free %.02fkB", heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024.0f);

        void* buffer1 = i2s_read(&mic1, SAMPLES);
        void* buffer2 = i2s_read(&mic2, SAMPLES);

        i2s_data_t data1 = i2s_parse(&mic1, buffer1, SAMPLES);
        i2s_data_t data2 = i2s_parse(&mic2, buffer2, SAMPLES);

        free(buffer1);
        free(buffer2);

        float* z = data1.l;
        float* a = data1.r;
        float* b = data2.l;
        float* c = data2.r;

        ESP_LOGI(TAG, "mic(0): min/max=%.06f/%.06f rms=%.0f freq=%.0fHz",
            snd_min(z, SAMPLES), snd_max(z, SAMPLES),
            snd_rms(z, SAMPLES) * 0xFFFFFF,
            snd_freq(z, SAMPLES) * SAMPLE_RATE);

        ESP_LOGI(TAG, "mic(a): min/max=%.06f/%.06f rms=%.0f freq=%.0fHz",
            snd_min(a, SAMPLES), snd_max(a, SAMPLES),
            snd_rms(a, SAMPLES) * 0xFFFFFF,
            snd_freq(a, SAMPLES) * SAMPLE_RATE);

        ESP_LOGI(TAG, "mic(b): min/max=%.06f/%.06f rms=%.0f freq=%.0fHz",
            snd_min(b, SAMPLES), snd_max(b, SAMPLES),
            snd_rms(b, SAMPLES) * 0xFFFFFF,
            snd_freq(b, SAMPLES) * SAMPLE_RATE);

        ESP_LOGI(TAG, "mic(c): min/max=%.06f/%.06f rms=%.0f freq=%.0fHz",
            snd_min(c, SAMPLES), snd_max(c, SAMPLES),
            snd_rms(c, SAMPLES) * 0xFFFFFF,
            snd_freq(c, SAMPLES) * SAMPLE_RATE);


        match_t A, B, C;

        PROFILE("A matching", A = snd_match(a, b, SAMPLES));
        PROFILE("B matching", B = snd_match(a, c, SAMPLES));
        PROFILE("C matching", C = snd_match(b, c, SAMPLES));

        ESP_LOGW(TAG, "A match: %.0f%% (lag of %d samples - %.03fms)", A.corr * 100, A.lag, 1.0f / SAMPLE_RATE * A.lag * 1000);
        ESP_LOGW(TAG, "B match: %.0f%% (lag of %d samples - %.03fms)", B.corr * 100, B.lag, 1.0f / SAMPLE_RATE * B.lag * 1000);
        ESP_LOGW(TAG, "C match: %.0f%% (lag of %d samples - %.03fms)", C.corr * 100, C.lag, 1.0f / SAMPLE_RATE * C.lag * 1000);

        vec_t pos = snd_locate(MICA_POS, MICB_POS, MICC_POS, A.lag * SAMPLE_RATE, B.lag * SAMPLE_RATE, C.corr * SAMPLE_RATE);

        ESP_LOGI(TAG, "x: %f, y: %f", pos.x, pos.y);

        free(z);
        free(a);
        free(b);
        free(c);

        // TODO: make a header only for protocol communication
        // uint16_t data[] = { SAMPLES, mic1.opts.bytes };
        // uint8_t event[] = { 0xED, 0x80 + (sizeof(data) / sizeof(*data)) };
        //
        // PROFILE("log to serial", {
        //     fwrite(event, sizeof(*event), sizeof(event) / sizeof(*event), stdout);
        //     fwrite(data, sizeof(*data), sizeof(data) / sizeof,e(*data), stdout);
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
