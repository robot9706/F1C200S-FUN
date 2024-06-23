#include <stdint.h>
#include "io.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "system.h"
#include "arm32.h"
#include "f1c100s_uart.h"
#include "f1c100s_gpio.h"
#include "f1c100s_clock.h"
#include "usb_cdc.h"

int main(void)
{
    system_init();            // Initialize clocks, mmu, cache, uart, ...
    arm32_interrupt_enable(); // Enable interrupts

    // Log some stuff
    printf("USB CDC :)\n");

    printf("USB init\n");
    cdc_init();

    printf("Loop\n");
    while (1)
    {
        cdc_handler();

        if (cdc_bytes_in() > 0)
        {
            int read = cdc_read_byte();
            printf("%c\n", (char)read);

            if (read >= 0)
            {
                cdc_write_byte((uint8_t)read);
            }
        }
    }
    return 0;
}
