#pragma once

#include <esp_lcd_io_spi.h>

typedef struct lcd_pins_t {
    gpio_num_t cs;
    gpio_num_t reset;
    gpio_num_t dc;
    gpio_num_t mosi;
    gpio_num_t sck;
} lcd_pins_t;

typedef struct lcd_opts_t {
    bool disabled : 1;
    bool vertical : 1;
    bool inverted : 1;
    bool mirror_x : 1;
    bool mirror_y : 1;
} lcd_opts_t;

typedef struct lcd_t {
    uint16_t width;
    uint16_t height;

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_handle_t panel;

    lcd_opts_t opts;
    lcd_pins_t pins;
} lcd_t;

lcd_t lcd_init(int width, int height, lcd_opts_t opts, lcd_pins_t pins);

void lcd_power(lcd_t* lcd, bool state);
void lcd_power_toggle(lcd_t* lcd);

void lcd_vertical(lcd_t* lcd, bool state);
void lcd_invert_color(lcd_t* lcd, bool state);
void lcd_mirror(lcd_t* lcd, bool x, bool y);

typedef uint16_t lcd_color_t;

#define LCD_BLACK   lcd_hex(0x000000)
#define LCD_RED     lcd_hex(0xFF0000)
#define LCD_BROWN   lcd_hex(0xAA5500)
#define LCD_ORANGE  lcd_hex(0xFFAA00)
#define LCD_YELLOW  lcd_hex(0xFFFF00)
#define LCD_LIME    lcd_hex(0xAAFF00)
#define LCD_GREEN   lcd_hex(0x00FF00)
#define LCD_AQUA    lcd_hex(0x00FFAA)
#define LCD_CYAN    lcd_hex(0x00FFDD)
#define LCD_BLUE    lcd_hex(0x0000FF)
#define LCD_PURPLE  lcd_hex(0x7700FF)
#define LCD_MAGENTA lcd_hex(0xAA00FF)
#define LCD_PINK    lcd_hex(0xFF00FF)
#define LCD_WHITE   lcd_hex(0xFFFFFF)

lcd_color_t lcd_hex(uint32_t hex);
lcd_color_t lcd_rgb(uint8_t r, uint8_t g, uint8_t b);
lcd_color_t lcd_rgbf(float r, float g, float b);
lcd_color_t lcd_hsl(uint16_t hue, float saturation, float luminance);
lcd_color_t lcd_hsv(uint16_t hue, float saturation, float value);

typedef struct lcd_pos_t {
    uint16_t x, y;
} lcd_pos_t;

// typedef struct {
//     lcd_pos_t start, end;
//     int thickness;
// } lcd_line_t;

typedef enum lcd_anchor_t {
    LCD_TOP_LEFT,
    LCD_TOP_CENTER,
    LCD_TOP_RIGHT,

    LCD_MID_LEFT,
    LCD_MID_CENTER,
    LCD_MID_RIGHT,

    LCD_BOT_LEFT,
    LCD_BOT_CENTER,
    LCD_BOT_RIGHT,
} lcd_anchor_t;

const char* lcd_anchor_str(lcd_anchor_t anchor);

typedef struct lcd_rect_t {
    uint16_t x, y;
    uint16_t width, height;

    lcd_anchor_t anchor;
} lcd_rect_t;

// typedef struct lcd_circle_t {
//     float radius;
//     lcd_pos_t pos;
// } lcd_circle_t;

typedef struct lcd_bmp_t {
    uint16_t width, height;
    uint16_t* data;
} lcd_bmp_t;

lcd_bmp_t lcd_bmp_new(uint16_t width, uint16_t height);

void lcd_fill(lcd_t* lcd, lcd_color_t col);
void lcd_bmp(lcd_t* lcd, lcd_bmp_t bmp, lcd_pos_t pos);
void lcd_pixel(lcd_t* lcd, lcd_pos_t pos, lcd_color_t col);
// void lcd_line(lcd_t* lcd, lcd_line_t line, lcd_color_t col);
void lcd_rect(lcd_t* lcd, lcd_rect_t rect, lcd_color_t col);
// void lcd_circ(lcd_t* lcd, lcd_circ_t circ, lcd_color_t col);

// void lcd_text(lcd_t* lcd, const char* text, int size, lcd_pos_t pos, lcd_color_t col);

// TODO: virtual buffer to enable forced pixelization
//
// This approach is better for direct control because it
// reduces the memory needed to store the entire buffer
// and also allow some C QoL buffer manipulation features.
//
// A potential implementation is to render each line alone,
// just like fill is doing, and map the colors to row buffer.
//
// #define CHUNK 4 // 320x240 -> 80x60 (4px/cell)
// for (int y = 0; y < bmp.height / CHUNK; y++) {
//     float v = 1.0f - (float) y * CHUNK / (bmp.height - 1);
//
//     for (int x = 0; x < bmp.width / CHUNK; x++) {
//         float h = 360.0f * (float) x * CHUNK / (bmp.width - 1);
//
//         lcd_color_t color = lcd_hsv(h, 1.0f, v);
//
//         for (int i = 0; i < CHUNK; i++) {
//             for (int j = 0; j < CHUNK; j++) {
//                 bmp.data[(y * CHUNK + i) * bmp.width + (x * CHUNK + j)] = color;
//             }
//         }
//     }
// }
