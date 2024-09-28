#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include "display.h"
#include "f1c100s_de.h"
#include "f1c100s_gpio.h"
#include "f1c100s_pwm.h"

void display_init(void) 
{
    de_lcd_config_t config;

    config.width = 320;
    config.height = 240;
    config.bus_width = DE_LCD_R_5BITS | DE_LCD_G_6BITS | DE_LCD_B_5BITS;
    config.bus_mode = DE_LCD_PARALLEL_RGB;

    config.pixel_clock_hz = 33000000;
    config.h_front_porch = 40;
    config.h_back_porch = 40;
    config.h_sync_len = 3;
    config.v_front_porch = 13;
    config.v_back_porch = 13;
    config.v_sync_len = 3;
    config.h_sync_invert = 1;
    config.v_sync_invert = 1;

    de_lcd_init(&config);

    debe_set_bg_color(0x00FFFFFF);
    debe_load(DEBE_UPDATE_AUTO);
}
