idf_component_register(
    SRCS "telecom.c"
         "lcd.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio
             esp_driver_uart
             esp_driver_i2s
             esp_timer
             esp_lcd
             esp_lcd_ili9341
)
