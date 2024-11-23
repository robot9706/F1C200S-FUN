#include "print.h"
#include "f1c100s_uart.h"
#include <stdarg.h>

#define OUT_UART UART1

static uint8_t hexLookup[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

void printChar(char chr)
{
    while (!(uart_get_status(OUT_UART) & UART_LSR_THRE));
    uart_tx(OUT_UART, chr);
}

static void printDec(uint32_t val, uint32_t div)
{
    int first = 1;

    while (div > 0)
    {
        int num = val / div;
        val = val % div;

        div /= 10;

        char charVal = '0' + num;
        if ((num == 0 && !first) || num != 0)
        {
            first = 0;
            printChar(charVal);
        }
    }

    if (first)
    {
        printChar('0');
    }
}

void printStr(const char* str)
{
    for (char *ptr = (char*)str; *ptr != 0; ptr++)
    {
        char c = *ptr;

        printChar(c);
    }
}

void print8(uint8_t u8)
{
    uint8_t h = (u8 >> 4) & 0x0f;
    uint8_t l = (u8 & 0x0f);

    printChar(hexLookup[h]);
    printChar(hexLookup[l]);
}

void print16(uint16_t u16)
{
    union
    {
        uint16_t val;
        uint8_t ar[2];
    } helper;
    
    helper.val = u16;

    for (int i = 1; i >= 0; i--)
    {
        print8(helper.ar[i]);
    }
}

void print32(uint32_t u32)
{
    union
    {
        uint32_t val;
        uint8_t ar[4];
    } helper;
    
    helper.val = u32;

    for (int i = 3; i >= 0; i--)
    {
        print8(helper.ar[i]);
    }
}

void print64(uint64_t u64)
{
    union
    {
        uint64_t val;
        uint8_t ar[8];
    } helper;
    
    helper.val = u64;

    for (int i = 7; i >= 0; i--)
    {
        print8(helper.ar[i]);
    }
}

void printDec8(uint8_t u8)
{
    printDec(u8, 100);
}

void printDec16(uint16_t u8)
{
    printDec(u8, 10000);
}

// Prints the format string using the following parameters:
// %x[1/2/4/8] Hexadecimal 1, 2, 4, 8 bytes
// %d[1/2] Decimal 1, 2 bytes
void printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    while (*fmt != 0)
    {
        if (*fmt == '%' && *(fmt + 1) != 0 && *(fmt + 2) != 0)
        {
            switch (*(fmt + 1))
            {
                case '%':
                    printChar('%');
                    break;

                case 'x': // Hexadecimal
                {
                    switch (*(fmt + 2))
                    {
                        case '1':
                            int b1 = va_arg(args, int);
                            print8(b1);
                            break;
                        case '2':
                            int b2 = va_arg(args, int);
                            print16(b2);
                            break;
                        case '4':
                            long int b4 = va_arg(args, long int);
                            print32(b4);
                            break;
                        case '8':
                            long long int b8 = va_arg(args, long long int);
                            print64(b8);
                            break;
                    }

                    break;
                }

                case 'd': // Decimal
                {
                    switch (*(fmt + 2))
                    {
                        case '1':
                            int d1 = va_arg(args, int);
                            printDec8(d1);
                            break;
                        case '2':
                            int d2 = va_arg(args, int);
                            printDec16(d2);
                            break;
                    }
                }
            }

            fmt += 2;
        }
        else
        {
            printChar(*fmt);
        }

        fmt++;
    }

    va_end(args);
}