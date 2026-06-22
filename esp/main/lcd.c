#include "lcd.h"
#include <math.h>
#include <esp_log.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_ili9341.h>
#include <esp_heap_caps.h>

#define TAG "lcd"

lcd_t lcd_init(int width, int height, lcd_opts_t opts, lcd_pins_t pins) {
    ESP_LOGI(TAG, "initializing lcd screen [%dx%d]", width, height);

    int color = sizeof(uint16_t);
    ESP_LOGI(TAG, "using colors of 16 bits (R5G6B5)");

    spi_bus_config_t buscfg = {
        .sclk_io_num = pins.sck,
        .mosi_io_num = pins.mosi,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = width * sizeof(uint16_t) * 80,
    };

    ESP_LOGI(TAG, "initializing 'spi_bus' on pins %d (SCK) and %d (MOSI)", pins.sck, pins.mosi);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = pins.dc,
        .cs_gpio_num = pins.cs,
        .pclk_hz = 20 * 1000 * 1000, // 20MHz
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_LOGI(TAG, "using %dMHz spi clock speed", io_config.pclk_hz / 1000 / 1000);

    ESP_LOGI(TAG, "initializing 'lcd_panel_io' using pins %d (DC) and %d (CS)", pins.dc, pins.cs);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &io));

    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = pins.reset,
        .bits_per_pixel = color * 8,
    };

    ESP_LOGI(TAG, "initializing 'lcd_panel_ili9341' using pin %d (RESET)", pins.reset);
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io, &panel_config, &panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

    ESP_LOGI(TAG, "configuring display with these options:");

    ESP_LOGI(TAG, "  disabled = %s", opts.disabled ? "yes" : "no");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, !opts.disabled));

    ESP_LOGI(TAG, "  vertical = %s", opts.vertical ? "yes" : "no");
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, opts.vertical));
    if (opts.vertical) {
        uint16_t temp = width;
        width = height;
        height = temp;
    }

    ESP_LOGI(TAG, "  inverted = %s", opts.inverted ? "yes" : "no");
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, opts.inverted));

    ESP_LOGI(TAG, "  mirror_x = %s", opts.mirror_x ? "yes" : "no");
    ESP_LOGI(TAG, "  mirror_y = %s", opts.mirror_y ? "yes" : "no");
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, opts.mirror_x, opts.mirror_y));

    return (lcd_t) {
        .width = width,
        .height = height,

        .io = io,
        .panel = panel,

        .opts = opts,
        .pins = pins,
    };
}

void lcd_power(lcd_t* lcd, bool state) {
    if (!lcd->opts.disabled == state) {
        ESP_LOGW(TAG, "lcd is already powered %s",
            state ? "on" : "off");

        return;
    }

    ESP_LOGI(TAG, "powering lcd %s",
        state ? "on" : "off");

    ESP_ERROR_CHECK(
        esp_lcd_panel_disp_on_off(
            lcd->panel,
            !(lcd->opts.disabled = !state)
        )
    );
}

void lcd_power_toggle(lcd_t* lcd) {
    lcd_power(lcd, !lcd->opts.disabled);
}

void lcd_vertical(lcd_t* lcd, bool state) {
    if (lcd->opts.vertical == state) {
        ESP_LOGW(TAG, "lcd orientation is already %s",
            state ? "vertical" : "horizontal");

        return;
    }

    ESP_LOGI(TAG, "changing lcd orientation to %s",
         state ? "vertical" : "horizontal");

    ESP_ERROR_CHECK(
        esp_lcd_panel_swap_xy(
            lcd->panel,
            (lcd->opts.vertical = state)
        )
    );

    uint16_t temp = lcd->width;
    lcd->width = lcd->height;
    lcd->height = temp;
}

void lcd_invert_color(lcd_t* lcd, bool state) {
    if (lcd->opts.inverted == state) {
        ESP_LOGW(TAG, "colors are already %s",
            state ? "bgr" : "rgb");

        return;
    }

    ESP_LOGI(TAG, "inverting colors from %s to %s",
         state ? "rgb" : "bgr",
         state ? "bgr" : "rgb");

    ESP_ERROR_CHECK(
        esp_lcd_panel_invert_color(
            lcd->panel,
            (lcd->opts.inverted = state)
        )
    );
}

void lcd_mirror(lcd_t* lcd, bool x, bool y) {
    ESP_LOGI(TAG, "mirroring %s", x &&  y ? "x and y"
                                : x && !y ? "x"
                               : !x &&  y ? "y"
                                          : "none");

    ESP_ERROR_CHECK(
        esp_lcd_panel_mirror(
            lcd->panel,
            (lcd->opts.mirror_x = x),
            (lcd->opts.mirror_y = y)
        )
    );
}

#define SWAP_BYTES_U16(x) (((x) >> 8) | ((x) << 8))

lcd_color_t lcd_hex(uint32_t hex) {
    uint8_t r = (hex >> 16) & 0xFF;
    uint8_t g = (hex >> 8)  & 0xFF;
    uint8_t b = (hex)       & 0xFF;

    lcd_color_t color = lcd_rgb(r, g, b);

    return color;
}

lcd_color_t lcd_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return lcd_rgbf((float) r / 0xFF,
                    (float) g / 0xFF,
                    (float) b / 0xFF);
}

lcd_color_t lcd_rgbf(float r, float g, float b) {
    lcd_color_t color = 0;

    color |= (uint8_t) (r * 0b11111) << 6 << 5;
    color |= (uint8_t) (g * 0b11111) << 6;
    color |= (uint8_t) (b * 0b11111);

    ESP_LOGD(TAG, "Converted rgb(%u, %u, %u) [#%02X%02X%02X] to #%04X",
        (uint8_t) r * 255,  (uint8_t) g * 255,  (uint8_t) b * 255,
        (uint8_t) r * 0xFF, (uint8_t) g * 0xFF, (uint8_t) b * 0xFF,
        color);

    return SWAP_BYTES_U16(color);
}

lcd_color_t lcd_hsl(uint16_t hue, float saturation, float luminance) {
    float chroma = fabsf(2.0f * luminance - 1.0f);
    chroma = (1.0f - chroma) * saturation;

    float x = hue / 60.0f;
    x = fmodf(x, 2.0f) - 1.0f;
    x = 1.0f - fabsf(x);
    x *= chroma;

    float r = luminance - chroma / 2.0f;
    float g = luminance - chroma / 2.0f;
    float b = luminance - chroma / 2.0f;

    if      (hue < 60 * 1) { r += chroma; g += x;      b += 0;      }
    else if (hue < 60 * 2) { r += x;      g += chroma; b += 0;      }
    else if (hue < 60 * 3) { r += 0;      g += chroma; b += x;      }
    else if (hue < 60 * 4) { r += 0;      g += x;      b += chroma; }
    else if (hue < 60 * 5) { r += x;      g += 0;      b += chroma; }
    else                   { r += chroma; g += 0;      b += x;      }

    return lcd_rgbf(r, g, b);
}

lcd_color_t lcd_hsv(uint16_t hue, float saturation, float value) {
    float chroma = value * saturation;

    float x = hue / 60.0f;
    x = fmodf(x, 2.0f) - 1.0f;
    x = 1.0f - fabsf(x);
    x *= chroma;

    float r = value - chroma;
    float g = value - chroma;
    float b = value - chroma;

    if      (hue < 60 * 1) { r += chroma; g += x;      b += 0;      }
    else if (hue < 60 * 2) { r += x;      g += chroma; b += 0;      }
    else if (hue < 60 * 3) { r += 0;      g += chroma; b += x;      }
    else if (hue < 60 * 4) { r += 0;      g += x;      b += chroma; }
    else if (hue < 60 * 5) { r += x;      g += 0;      b += chroma; }
    else                   { r += chroma; g += 0;      b += x;      }

    return lcd_rgbf(r, g, b);
}

const char* lcd_anchor_str(lcd_anchor_t anchor) {
    switch (anchor) {
        case LCD_TOP_LEFT: return "TOP_LEFT";
        case LCD_TOP_CENTER: return "TOP_CENTER";
        case LCD_TOP_RIGHT: return "TOP_RIGHT";

        case LCD_MID_LEFT: return "MID_LEFT";
        case LCD_MID_CENTER: return "MID_CENTER";
        case LCD_MID_RIGHT: return "MID_RIGHT";

        case LCD_BOT_LEFT: return "BOT_LEFT";
        case LCD_BOT_CENTER: return "BOT_CENTER";
        case LCD_BOT_RIGHT: return "BOT_RIGHT";
    }

    return NULL;
}

lcd_bmp_t lcd_bmp_new(uint16_t width, uint16_t height) {
    size_t size = width * height * sizeof(uint16_t);
    uint16_t* data = heap_caps_malloc(size, MALLOC_CAP_DMA);

    return (lcd_bmp_t) { width, height, data };
}

void lcd_fill(lcd_t* lcd, lcd_color_t col) {
    ESP_LOGI(TAG, "filling the screen with #%04X", SWAP_BYTES_U16(col));

    uint16_t* buffer = heap_caps_malloc(lcd->width * sizeof(uint16_t), MALLOC_CAP_DMA);

    for (int i = 0; i < lcd->width; i++) {
        buffer[i] = col;
    }

    for (int i = 0; i < lcd->height; i++) {
        esp_lcd_panel_draw_bitmap(lcd->panel,
            0, i, lcd->width, i + 1, buffer);
    }

    free(buffer);
}

void lcd_bmp(lcd_t* lcd, lcd_bmp_t bmp, lcd_pos_t pos) {
    ESP_LOGI(TAG, "drawing bmp of %ux%u at [%u, %u]",
         bmp.width, bmp.height,
         pos.x, pos.y);

    for (int i = 0; i < bmp.height; i++) {
        esp_lcd_panel_draw_bitmap(lcd->panel,
            pos.x,             pos.y + i,
            pos.x + bmp.width, pos.y + i + 1,
            bmp.data + bmp.width * i);
    }
}

// TODO: handle anchoring
void lcd_rect(lcd_t* lcd, lcd_rect_t rect, lcd_color_t col) {
    ESP_LOGI(TAG, "drawing rect of %ux%u at [%u, %u] with #%04X on %s",
         rect.width, rect.height,
         rect.x, rect.y,
         SWAP_BYTES_U16(col), lcd_anchor_str(rect.anchor));

    uint16_t* buffer = heap_caps_malloc(rect.width * sizeof(uint16_t), MALLOC_CAP_DMA);

    for (int i = 0; i < rect.width; i++) {
        buffer[i] = col;
    }

    for (int i = 0; i < rect.height; i++) {
        esp_lcd_panel_draw_bitmap(lcd->panel,
            rect.x,              rect.y + i,
            rect.x + rect.width, rect.y + i + 1, buffer);
    }

    free(buffer);
}
