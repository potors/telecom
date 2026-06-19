#pragma once

#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"

typedef struct {
    int width;
    int height;
    int stride;

    esp_lcd_panel_handle_t panel_handle;
} screen_t;

static screen_t _screen;

static void screen(
    gpio_num_t sck,
    gpio_num_t mosi,
    gpio_num_t miso,
    int width,
    int height,
    gpio_num_t dc,
    gpio_num_t cs,
    int hz,
    gpio_num_t reset
) {
    int stride = width * sizeof(uint16_t);

    spi_bus_config_t buscfg = {
        .sclk_io_num = sck,
        .mosi_io_num = mosi,
        .miso_io_num = miso,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = stride * 80,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    _screen.width = width;
    _screen.height = height;
    _screen.stride = stride;

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = dc,
        .cs_gpio_num = cs,
        .pclk_hz = hz,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = reset,
        .bits_per_pixel = 16,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));
    _screen.panel_handle = panel_handle;

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
}

void screen_fill(char r, char g, char b) {
    uint16_t color = 0;

    color |= 0b11111 * (int) (r / 255.0f);
    color <<= 5;

    color |= 0b111111 * (int) (g / 255.0f);
    color <<= 6;

    color |= 0b11111 * (int) (b / 255.0f);

    uint16_t* buffer = heap_caps_malloc(_screen.stride * _screen.height, MALLOC_CAP_DMA);
    for (int i = 0; i < _screen.width * _screen.height; i++) {
        buffer[i] = color;
    }

    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(_screen.panel_handle, 0, 0, _screen.width, _screen.height, buffer));
}

// void lcd_fill_solid_color(esp_lcd_panel_handle_t panel_handle, uint16_t color) {
//     // Allocate 1 pixel in DMA-capable memory
//     uint16_t *dma_color_buffer = heap_caps_malloc(sizeof(uint16_t), MALLOC_CAP_DMA);
//     if (dma_color_buffer == NULL) {
//         return; // Handle memory allocation failure
//     }
//
//     *dma_color_buffer = color;
//
//     // Flush to the entire screen. esp_lcd will handle hardware DMA repeating.
//     esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_H_RES, LCD_V_RES, dma_color_buffer);
//
//     // Note: Do not free dma_color_buffer immediately if using async DMA.
//     // Free it when the 'on_color_trans_done' callback triggers.
// }
