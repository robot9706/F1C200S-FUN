#include <stdint.h>
#include "io.h"
#include <stdio.h>
#include <math.h>
#include "system.h"
#include "arm32.h"
#include "f1c100s_uart.h"
#include "sdcard.h"
#include "f1c100s_gpio.h"
#include "f1c100s_clock.h"
#include "f1c100s_sdc.h"
#include "f1c100s_usbm.h"

static sdcard_t sdcard;

static void usb_block_read(uint8_t* buffer, uint32_t blockIndex, uint32_t numBlocks)
{
    sdcard_read(&sdcard, buffer, blockIndex, numBlocks);
}

static void usb_block_write(uint8_t* buffer, uint32_t blockIndex, uint32_t numBlocks)
{
    sdcard_write(&sdcard, buffer, blockIndex, numBlocks);
}

int main(void)
{
    system_init();            // Initialize clocks, mmu, cache, uart, ...
    arm32_interrupt_enable(); // Enable interrupts

    // Init MMC interface
    clk_reset_set(CCU_BUS_SOFT_RST0, 8);
    clk_enable(CCU_BUS_CLK_GATE0, 8);
    clk_reset_clear(CCU_BUS_SOFT_RST0, 8);

    gpio_init(GPIOF, PIN1 | PIN2 | PIN3, GPIO_MODE_AF2, GPIO_PULL_NONE, GPIO_DRV_3);

    sdcard.sdc_base = SDC0_BASE;
    sdcard.voltage = MMC_VDD_27_36;
    sdcard.width = MMC_BUS_WIDTH_1;
    sdcard.clock = 50000000;

    // Log some stuff
    printf("PLL_CPU: %ld\n", clk_pll_get_freq(PLL_CPU));
    printf("PLL_AUDIO: %ld\n", clk_pll_get_freq(PLL_AUDIO));
    printf("PLL_VIDEO: %ld\n", clk_pll_get_freq(PLL_VIDEO));
    printf("PLL_VE: %ld\n", clk_pll_get_freq(PLL_VE));
    printf("PLL_DDR: %ld\n", clk_pll_get_freq(PLL_DDR));
    printf("PLL_PERIPH: %ld\n", clk_pll_get_freq(PLL_PERIPH));
    printf("\n");
    printf("CPU: %ld\n", clk_cpu_get_freq());
    printf("HCLK: %ld\n", clk_hclk_get_freq());
    printf("AHB: %ld\n", clk_ahb_get_freq());
    printf("APB: %ld\n", clk_apb_get_freq());
    printf("\n");

    while (1)
    {
        if (sdcard_detect(&sdcard))
        {
            printf("SD card detected\n");

            printf("Version: %lu\n", sdcard.version);
            printf("High capacity: %lu\n", sdcard.high_capacity);
            printf("Read block length: %lu\n", sdcard.read_bl_len);
            printf("Write block length: %lu\n", sdcard.write_bl_len);
            printf("Block count: %llu\n", sdcard.blk_cnt);

            printf("Init USB mux\n");
            usb_mux(USB_MUX_DEVICE);
            usbd_init(sdcard.blk_cnt, usb_block_read, usb_block_write);

            while (sdcard_status(&sdcard))
            {
                usbd_handler();
            }

            printf("USB deinit\n");
            usb_deinit();
        }
        else
        {
            printf("SD card not detected\n");
            while (!sdcard_detect(&sdcard))
            {
                sdelay(5000);
            }
            printf("yissss\n");
        }
    }
    return 0;
}
