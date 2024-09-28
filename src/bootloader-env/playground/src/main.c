#include <stdint.h>
#include "io.h"
#include <math.h>
#include <string.h>
#include "system.h"
#include "arm32.h"
#include "display.h"
#include "f1c100s_uart.h"
#include "f1c100s_gpio.h"
#include "f1c100s_clock.h"
#include "f1c100s_de.h"

static uint16_t fb[320 * 240];

static void writeLine(const char* str)
{
    for (char *ptr = str; *ptr != 0; ptr++)
    {
        char c = *ptr;

        while (!(uart_get_status(UART1) & UART_LSR_THRE))
            ;
        uart_tx(UART1, c);
    }
}

static int16_t charsToInt(uint8_t char1, uint8_t char2)
{
    if (char1 < '0' || char1 > '9' || char2 < '0' || char2 > '9')
    {
        return -1;
    }

    int val1 = char1 - '0';
    int val2 = char2 - '0';

    return val1 * 10 + val2;
}

static de_lcd_config_t config;

static void init_display()
{
    de_lcd_init(&config);

    debe_set_bg_color(0x00FFFFFF);
    debe_load(DEBE_UPDATE_AUTO);

    debe_layer_init(0);
    debe_layer_set_size(0, 320, 240);
    debe_layer_set_addr(0, fb);
    debe_layer_set_mode(0, DEBE_MODE_16BPP_RGB_565);
    debe_layer_enable(0);

    for (int x = 0; x < 320 * 240; x++) {
        fb[x] = 0x000F;
    }

    for (int y = 0; y < 50; y++) {
        for (int x = 0; x < 50; x++) {
            fb[x + 320 * y] = 0xFFFF;
        }
    }
}

int main(void)
{
    system_init();
    arm32_interrupt_enable(); // Enable interrupts

    writeLine("Playground");

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

    init_display();

    uint8_t commandBuffer[3] = {0};
    int commandPtr = 0;

    while (1)
    {
        if (uart_get_status(UART1) & UART_LSR_DR)
        {
            uint8_t rec = uart_get_rx(UART1);
            commandBuffer[commandPtr] = rec;
            commandPtr += 1;

            if (rec == '\r')
            {
                uart_tx(UART1, '\r');
                uart_tx(UART1, '\n');

                memset(commandBuffer, 0, 3);
                commandPtr = 0;
            }
            else
            {
                uart_tx(UART1, rec);
            }

            if (commandPtr >= 3)
            {
                uart_tx(UART1, '\r');
                uart_tx(UART1, '\n');

                // Execute command
                switch (commandBuffer[0])
                {
                    case '?':
                    writeLine("Hello :)");
                    break;

                    case 'h':
                    {
                        int16_t val = charsToInt(commandBuffer[1], commandBuffer[2]);
                        if (val < 0)
                        {
                            writeLine("Invalid number");
                        }

                        config.h_front_porch = val;
                        init_display();

                        writeLine("h_front_porch set");
                        break;
                    }

                    default:
                    writeLine("Unknown command");
                    break;
                }

                memset(commandBuffer, 0, 3);
                commandPtr = 0;
            }
        }
    }

    return 0;
}
