#include <stdint.h>
#include "io.h"
#include <math.h>
#include <string.h>
#include "system.h"
#include "arm32.h"
#include "f1c100s_uart.h"
#include "f1c100s_gpio.h"
#include "f1c100s_clock.h"

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

int main(void)
{
    //system_init();            // Initialize clocks, mmu, cache, uart, ...
    //arm32_interrupt_enable(); // Enable interrupts

    writeLine("Hello world :)");

    while (1);  

    return 0;
}
