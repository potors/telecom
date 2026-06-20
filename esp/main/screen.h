#pragma once

#include <string.h>
#include <esp_lcd_io_spi.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_ili9341.h>

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

    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
}

void screen_fill(char r, char g, char b) {
    uint16_t color = 0;

    color |= (char) (0b11111 * (r / 255.0f));
    color <<= 5;

    color |= (char) (0b111111 * (g / 255.0f));
    color <<= 6;

    color |= (char) (0b11111 * (b / 255.0f));

    printf("Color: rgb(%d %d %d) -> 0x%X\n", (int) r, (int) g, (int) b, color);

    // uint16_t* row = heap_caps_malloc(_screen.stride, MALLOC_CAP_DMA);
    uint16_t* buffer = heap_caps_malloc(_screen.stride * _screen.height, MALLOC_CAP_DMA);

    // for (int x = 0; x < _screen.width; x++) {
    //     row[x] = color;
    // }

    memset(buffer, color, _screen.stride * _screen.height);

    // for (int y = 0; y < _screen.height; y++) {
    //     esp_lcd_panel_draw_bitmap(
    //         _screen.panel_handle,
    //         0, y,
    //         _screen.width, y + 1,
    //         row
    //     );
    // }

    esp_lcd_panel_draw_bitmap(
        _screen.panel_handle,
        0, 0, _screen.width, _screen.height,
        buffer);

    // free(row);
    free(buffer);
}
