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

static void test()
{
    FATFS fs;
    FRESULT res;

    res = f_mount(&fs, "", 1);
    if (res != FR_OK)
    {
        printf("Mount FS failed %d1\n", res);
        return;
    }

    printf("Mount filesystem OK\n");

    DIR dir;
    FILINFO fno;

    res = f_opendir(&dir, "/");
    if (res != FR_OK)
    {
        printf("Failed to open root %d1\n", res);
        return;
    }

    printf("Root open OK\n");

    do
    {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK)
        {
            printf("Readdir failed %d1\n", res);
            break;
        }

        if (fno.fname[0] == 0)
        {
            break;
        }

        printf("File '%s'\n", fno.fname);
    }
    while (fno.fname[0] != 0);
    
    f_closedir(&dir);
}

int main(void)
{
    system_init();
    arm32_interrupt_enable(); // Enable interrupts

    test();

    while (1)
    {
    }

    return 0;
}
