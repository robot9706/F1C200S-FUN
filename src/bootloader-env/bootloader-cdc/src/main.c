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

#define RAM_BASE 0x80000000
#define BL1_SIZE 0x20000

#define LOAD_ADDR (RAM_BASE + BL1_SIZE)

#define STATE_WAIT_HEADER 0
#define STATE_WAIT_DATA 1

int main(void)
{
    system_init();            // Initialize clocks, mmu, cache, uart, ...
    arm32_interrupt_enable(); // Enable interrupts

    // Log some stuff
    printf("USB CDC :)\n");

    printf("USB init\n");
    cdc_init();

    uint8_t *loadPtr = (uint8_t*)LOAD_ADDR;
    uint32_t loadAddress = 0;

    uint8_t dataLength = 0;
    uint8_t state = STATE_WAIT_HEADER;

    printf("Loop\n");
    while (1)
    {
        cdc_handler();

        switch (state)
        {
            case STATE_WAIT_HEADER:
            {
                if (cdc_bytes_in() > 0)
                {
                    int length = cdc_read_byte();
                    if (length < 0)
                    {
                        printf("Failed to read CDC\n");
                        continue;
                    }

                    printf("Read HEADER %d\n", length);

                    if (length == 0)
                    {
                        // No more data - ready to boot

                        printf("Doing the thing :)\n");
                        while (!(uart_get_status(UART1) & UART_LSR_THRE)); // Wait for TX fifo empty

                        void (*bootFunc)(void) = (void (*)())LOAD_ADDR;
                        bootFunc();

                        printf("HALT");

                        while (1)
                            ;
                    }

                    dataLength = length;
                    state = STATE_WAIT_DATA;
                }

                break;
            }

            case STATE_WAIT_DATA:
            {
                if (cdc_bytes_in() >= dataLength)
                {
                    // Got expected data length
                    printf("Read %d at %08lx\n", dataLength, (loadAddress + LOAD_ADDR));

                    for (int count = 0; count < dataLength; count++)
                    {
                        int read = cdc_read_byte();
                        if (read < 0)
                        {
                            printf("Failed to read CDC byte!\n");
                            break;
                        }

                        printf("%02x ", read);

                        loadPtr[loadAddress] = (uint8_t)read;
                        loadAddress += 1;
                    }
                    printf("\n");

                    dataLength = 0;
                    state = STATE_WAIT_HEADER;
                }

                break;
            }
        }
    }
    return 0;
}
