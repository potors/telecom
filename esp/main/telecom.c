#include <freertos/FreeRTOS.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <stdio.h>

// See: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2s.html
// Also: https://esp32.com/viewtopic.php?t=15185
#include <driver/i2s_std.h>
#include <driver/gpio.h>

#define PIN_SCK GPIO_NUM_18

i2s_chan_handle_t i2s(int sample_rate, int ws, int sd) {
    i2s_chan_handle_t rx;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, NULL, &rx);

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_24BIT,
            .slot_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = 32,
            .ws_pol = false,
            .bit_shift = true,
            .msb_right = false,
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = PIN_SCK,
            .ws = ws,
            .dout = I2S_GPIO_UNUSED,
            .din = sd,
            .invert_flags = {
                .bclk_inv = false,
                .mclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx, &std_cfg));

    return rx;
}

#define WS GPIO_NUM_4
#define SD GPIO_NUM_2

#define BUFFER_SIZE 1024
#define SAMPLE_RATE 48000

void app_main() {
    i2s_chan_handle_t rx = i2s(SAMPLE_RATE, WS, SD);
    ESP_ERROR_CHECK(i2s_channel_enable(rx));

    static int buffer[BUFFER_SIZE];

    unsigned int reads = 0;
    esp_err_t status;

    while (1) {
        static int CHUNKS = 1;
        float start = (float) esp_timer_get_time() / (1000 * 1000);
        for (int i = 0; i < CHUNKS; i++) {
            status = i2s_channel_read(rx, &buffer, BUFFER_SIZE * sizeof(int), &reads, portMAX_DELAY);
            if (status != ESP_OK) {
                ESP_LOGE("wut", "read error: %d", status);
                continue;
            }
        }
        float time = (float) esp_timer_get_time() / (1000 * 1000) - start;

        float secs_per_chunk = time / CHUNKS;
        float secs_per_read = secs_per_chunk / reads;

        printf("Read Tracing:\n");
        printf("\t%d chunks\n", CHUNKS);
        printf("\t%u samples\n", BUFFER_SIZE);
        printf("\t%fs total\n", time);
        printf("\t%fs per chunk\n", secs_per_chunk);
        printf("\t%fms per chunk\n", secs_per_chunk * 1000);
        printf("\t%fµs per chunk\n", secs_per_chunk * 1000 * 1000);
        printf("\t%f chunks per sec\n", 1 / secs_per_chunk);
        printf("\t%fs per read\n", secs_per_read);
        printf("\t%fms per read\n", secs_per_read * 1000);
        printf("\t%fµs per read\n", secs_per_read * 1000 * 1000);
        printf("\t%f reads per sec\n", 1 / secs_per_read);

        // start = (float) esp_timer_get_time() / (1000 * 1000);
        // for (int i = 0; i < BUFFER_SIZE / 2; i++) {
        //     printf("buffer_l[%d/%d]: %d\n", i, BUFFER_SIZE / 2, buffer[i * 2]);
        // }
        //
        // for (int i = 0; i < BUFFER_SIZE / 2; i++) {
        //     printf("buffer_r[%d/%d]: %d\n", i, BUFFER_SIZE / 2, buffer[i * 2 + 1]);
        // }
        //
        // time = (float) esp_timer_get_time() / (1000 * 1000) - start;
        // printf("printing all values took %fs\n", time);

        // vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
