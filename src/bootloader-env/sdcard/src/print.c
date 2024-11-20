#include "print.h"
#include "f1c100s_uart.h"

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
    for (char *ptr = str; *ptr != 0; ptr++)
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
