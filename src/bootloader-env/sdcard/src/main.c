#include <stdint.h>
#include "io.h"
#include <math.h>
#include <string.h>
#include "system.h"
#include "arm32.h"
#include "f1c100s_uart.h"
#include "f1c100s_gpio.h"
#include "f1c100s_clock.h"
#include "f1c100s_de.h"
#include "f1c100s_timer.h"
#include "f1c100s_intc.h"
#include "print.h"
#include "ff.h"
#include <stdarg.h>

int main(void)
{
    system_init();
    arm32_interrupt_enable(); // Enable interrupts

    FATFS fs;
    uint8_t state = f_mount(&fs, "", 1);
    printf("Mount %d1\n", state);
    if (state == FR_OK)
    {
        
    }

    while (1)
    {
    }

    return 0;
}
