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
#include "f1c100s_timer.h"
#include "f1c100s_intc.h"
#include "dma.h"

#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240

static uint16_t fb1[DISPLAY_WIDTH * DISPLAY_HEIGHT];
static uint16_t fb2[DISPLAY_WIDTH * DISPLAY_HEIGHT];

static uint16_t *fb;

volatile uint32_t systime = 0;
static de_lcd_config_t config;

static uint32_t lastMoveTime = 0;

int rectx = 0;
int recty = 0;

int rectDirX = 1;
int rectDirY = 1;

float rect2x = 0;
float rect2y = 0;
int rect2Color = 0;
int rect2ChangeColor = 0;

#define RGB565(R, G, B) ((R << 11) | (G << 5) | B)

int rectColor = 0;
uint16_t RECT_COLORS[] = {
    RGB565(31, 0, 0),
    RGB565(0, 63, 0),
    RGB565(0, 0, 31),
    RGB565(31, 63, 0),
    RGB565(31, 0, 31),
    RGB565(0, 63, 31)
};

#define RECT_SIZE 50

static uint8_t hexLookup[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
static void writeHex8(uint8_t u8)
{
    uint8_t h = (u8 >> 4) & 0x0f;
    uint8_t l = (u8 & 0x0f);

    uart_tx(UART1, hexLookup[h]);
    uart_tx(UART1, hexLookup[l]);
}

static void writeHex32(uint32_t u32)
{
    union
    {
        uint32_t val;
        uint8_t ar[4];
    } helper;
    
    helper.val = u32;

    for (int i = 3; i >= 0; i--)
    {
        writeHex8(helper.ar[i]);
    }
}

static void writeUart(const char* str)
{
    for (char *ptr = str; *ptr != 0; ptr++)
    {
        char c = *ptr;

        while (!(uart_get_status(UART1) & UART_LSR_THRE))
            ;
        uart_tx(UART1, c);
    }
}

static uint16_t charToInt(const uint8_t* ptr, int len)
{
    uint16_t acc = 0;

    int mul = 1;
    for (int x = len - 1; x >= 0; x--)
    {
        uint8_t charValue = ptr[x];

        if (charValue < '0' || charValue > '9')
        {
            return -1;
        }

        acc += (charValue - '0') * mul;
        mul *= 10;
    }

    return acc;
}

static void printInt16(uint16_t val)
{
    bool first = true;

    int div = 10000; // 16bit can only be 60k max
    while (div > 0)
    {
        int num = val / div;
        val = val % div;

        div /= 10;

        char charVal = '0' + num;
        if ((num == 0 && !first) || num != 0)
        {
            first = false;
            uart_tx(UART1, charVal);
        }
    }

    if (first)
    {
        uart_tx(UART1, '0');
    }
}

static void init_display()
{
    de_lcd_init(&config);

    debe_set_bg_color(0x00FFFFFF);
    debe_load(DEBE_UPDATE_AUTO);

    fb = fb1;

    debe_layer_init(1);
    debe_layer_set_size(1, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    debe_layer_set_addr(1, fb2);
    debe_layer_set_mode(1, DEBE_MODE_16BPP_RGB_565);
    debe_layer_enable(1);
}

void timer_irq_handler(void) 
{
    systime++;
    tim_clear_irq(TIM0);
}

void timer_init(void) 
{
    intc_set_irq_handler(IRQ_TIMER0, timer_irq_handler);
    intc_enable_irq(IRQ_TIMER0);

    tim_init(TIM0, TIM_MODE_CONT, TIM_SRC_HOSC, TIM_PSC_1);
    tim_set_period(TIM0, 24000000UL / 1000UL);
    tim_int_enable(TIM0);
    tim_start(TIM0);

    writeUart("TIM0 enabled\n");
}

static void drawRect(int x, int y, int w, int h, uint16_t color)
{
    for (int dy = y; dy < y + h; dy++)
    {
        uint16_t *linePtr = &fb[dy * DISPLAY_WIDTH];

        for (int dx = x; dx < x + w; dx++)
        {
            linePtr[dx] = color;
        }        
    }
}

static void exec_command(const uint8_t* commandBuffer, int length) 
{
    if (length < 2)
    {
        return;
    }

    int val = charToInt(&commandBuffer[1], length - 1);

    switch (commandBuffer[0])
    {
        case 'h':
            config.h_front_porch = val;
            init_display();

            writeUart("h_front_porch: ");
            printInt16(config.h_front_porch);
            uart_tx(UART1, '\n');
            break;

        case 'j':
            config.h_back_porch = val;
            init_display();

            writeUart("h_back_porch: ");
            printInt16(config.h_back_porch);
            uart_tx(UART1, '\n');
            break;

        case 'k':
            config.h_sync_len = val;
            init_display();

            writeUart("h_sync_len: ");
            printInt16(config.h_sync_len);
            uart_tx(UART1, '\n');
            break;

        case 'v':
            config.v_front_porch = val;
            init_display();

            writeUart("v_front_porch: ");
            printInt16(config.v_front_porch);
            uart_tx(UART1, '\n');
            break;

        case 'b':
            config.v_back_porch = val;
            init_display();

            writeUart("v_back_porch: ");
            printInt16(config.v_back_porch);
            uart_tx(UART1, '\n');
            break;

        case 'n':
            config.v_sync_len = val;
            init_display();

            writeUart("v_sync_len: ");
            printInt16(config.v_sync_len);
            uart_tx(UART1, '\n');
            break;

        case 'q':
            config.h_sync_invert = val;
            init_display();

            writeUart("h_sync_invert: ");
            printInt16(config.h_sync_invert);
            uart_tx(UART1, '\n');
            break;

        case 'w':
            config.v_sync_invert = val;
            init_display();

            writeUart("v_sync_invert: ");
            printInt16(config.v_sync_invert);
            uart_tx(UART1, '\n');
            break;

        case 'f':
            config.pixel_clock_hz = val * 1000000;
            init_display();

            writeUart("pixel_clock_hz: ");
            printInt16(config.pixel_clock_hz);
            uart_tx(UART1, '\n');
            break;

        default:
            writeUart("Unknown command\n");
            break;
    }
}

static uint32_t dmaSource[4] =
{
    0xAABBCCDD,
    0x11223344,
    0xC0FEBABE,
    0xff00ff00,
};

static uint32_t dmaTarget[4] = {0};

static uint8_t dmaUARTTarget[32] = { 'H', 'e', 'l', 'l', 'o', ' ', 'f', 'r', 'o', 'm', ' ', 'D', 'M', 'A', '\n', '1', '2', '3', '\n' };

// https://github.com/VeiLiang/BoloRTT/blob/5c2ff1349af09f9df09a847109e2b5ddf3f70106/bsp/f1c/drivers/drv_dma.c#L9
static void dma_test()
{
    clk_enable(CCU_BUS_CLK_GATE0, 6);
    clk_reset_clear(CCU_BUS_SOFT_RST0, 6);

    DMA->INT_PRIO = DMA->INT_PRIO | (1 << 16); // Auto clock gating

    NDMA_T* dma = NDMA(0);

    writeUart("DMA test: ");
    writeHex32(NDMA_ADR(0));
    uart_tx(UART1, '\n');

    //dma->SRC = (uint32_t)&dmaSource;
    //dma->DST = (uint32_t)&dmaTarget;
    //dma->BYTE_COUNTER = 4 * sizeof(uint32_t);

    dma->SRC = (uint32_t)&dmaUARTTarget;
    dma->DST = UART1_BASE + UART_THR;
    dma->BYTE_COUNTER = 19;

    // Source=SDRAM, Source type = Linear, Source burst length = 1, Source data size = 32 bit,
    // Byte counte readout disable
    // Dest=SDRAM, Dest type = Linear, Dest burst length = 1, Dest data size = 32 bit
    // DMA wait state = 0, Continious mode = disabled
    // DMA load enabled
    //uint32_t cfg = (0x11 | (0 << 5) | (0 << 7) | (0x02 << 8) | (0 << 15) | (0x11 << 16) | (0 << 21) | (0 << 23) | (0x02 << 24) | (0 << 26) | (0 << 29) | (1 << 31));

    dma->CFG = (0x11 | // source = SDRAM
        (0 << 5) | // Linear mode
        (0 << 7) | // burst length
        (0 << 8) | // 8 bit
        (9 << 16) | // dest = UART1 TX
        (1 << 21) | // IO mode
        (0 << 23) | // burst length
        (0 << 24) | // 8 bit
        (1 << 31) // do the thing
    );

    /*writeUart("\nSRC: ");
    writeHex32((uint32_t)&dmaSource);

    writeUart("\nDST: ");
    writeHex32((uint32_t)&dmaUARTTarget);

    writeUart("\nBYT: ");
    writeHex32(dma->BYTE_COUNTER);

    uart_tx(UART1, '\n');*/

    while (dma->CFG & (1 << 31))
    {
        // Wait until load clears
    }

    while (dma->CFG & (1 << 30))
    {
        // Wait until busy
    }

    writeUart("\nCompare DMA:\n");
    /*for (int x = 0; x < 4; x++)
    {
        writeHex32(dmaSource[x]);
        uart_tx(UART1, ' ');
        writeHex32(dmaTarget[x]);
        uart_tx(UART1, '\n');
    }*/

    for (int x = 0; x < 32; x++)
    {
        writeHex8(dmaUARTTarget[x]);
        uart_tx(UART1, ' ');
    }
    uart_tx(UART1, '\n');
}

static uint8_t dmaUARTRead[32] = { 0 };

static void dma_test2()
{
    NDMA_T* dma = NDMA(0);

    writeUart("DMA test 2\n");

    dma->SRC = UART1_BASE + UART_RBR;
    dma->DST = (uint32_t)&dmaUARTRead;
    dma->BYTE_COUNTER = 8;

    dma->CFG = (0x09 | // source = UART1 RX
        (1 << 5) | // IO Mode
        (0 << 7) | // burst length
        (0 << 8) | // 8 bit
        (11 << 16) | // dest = SDRAM TX
        (0 << 21) | // Linear mode
        (0 << 23) | // burst length
        (0 << 24) | // 8 bit
        (1 << 31) // do the thing
    );

    while (dma->CFG & (1 << 31))
    {
        // Wait until load clears
    }

    while (dma->CFG & (1 << 30))
    {
        // Wait until busy
    }

    writeUart("\nCompare DMA:\n");
    for (int x = 0; x < 32; x++)
    {
        writeHex8(dmaUARTRead[x]);
        uart_tx(UART1, ' ');
    }
    uart_tx(UART1, '\n');
}

static void setup_uart2()
{
    uint32_t cfg0 = read32(GPIOE + GPIO_CFG0);
    cfg0 = (cfg0 & ~(7 << 4)) | (0x03 << 4); // Enable UART0_TX on pin PE1
    cfg0 = (cfg0 & ~7) | 0x03; // Enable UART0_RX on pin PE0
    write32(GPIOE + GPIO_CFG0, cfg0);

    gpio_init(GPIOE, PIN0 | PIN1, GPIO_MODE_AF5, GPIO_PULL_NONE, GPIO_DRV_3);

    clk_enable(CCU_BUS_CLK_GATE2, 20);
    clk_reset_clear(CCU_BUS_SOFT_RST2, 20);
    uart_init(UART0, 57600);
}

#define JOY_SYNC_NONE 0
#define JOY_SYNC_SYNC1 1
#define JOY_SYNC_SYNC2 2
static int joySync = JOY_SYNC_NONE;

static uint8_t leftStickX = 127;
static uint8_t leftStickY = 127;
static uint8_t rightStickX = 127;
static uint8_t rightStickY = 127;
static uint16_t buttons = 0;

static void process_controller_input()
{
    union controllerData
    {
        uint8_t raw[6];
        struct
        {
            uint8_t leftStickX;
            uint8_t leftStickY;
            uint8_t rightStickX;
            uint8_t rightStickY;
            uint16_t buttons;
        } data;
    } state;

    for (int x = 0; x < 6; x++)
    {
        state.raw[x] = uart_get_rx(UART0);
    }

    printInt16(state.data.leftStickX);
    uart_tx(UART1, ' ');
    printInt16(state.data.leftStickY);
    uart_tx(UART1, ' ');
    printInt16(state.data.rightStickX);
    uart_tx(UART1, ' ');
    printInt16(state.data.rightStickY);
    uart_tx(UART1, ' ');
    writeHex32(state.data.buttons);
    uart_tx(UART1, '\n');

    leftStickX = state.data.leftStickX;
    leftStickY = state.data.leftStickY;
    rightStickX = state.data.rightStickX;
    rightStickY = state.data.rightStickY;
    buttons = state.data.buttons;
}

static void joy_update()
{
    while (uart_get_rx_fifo_level(UART0) > 0)
    {
        switch (joySync)
        {
            case JOY_SYNC_NONE:
                uint8_t sync1 = uart_get_rx(UART0);
                if (sync1 == 0x55)
                {
                    joySync = JOY_SYNC_SYNC1;
                }
                break;
            case JOY_SYNC_SYNC1:
                uint8_t sync2 = uart_get_rx(UART0);
                if (sync2 == 0xAA)
                {
                    joySync = JOY_SYNC_SYNC2;
                }
                else
                {
                    joySync = JOY_SYNC_NONE;
                    return;
                }
                break;
            case JOY_SYNC_SYNC2:
                if (uart_get_rx_fifo_level(UART0) < 6)
                {
                    return;
                }

                process_controller_input();

                joySync = JOY_SYNC_NONE;
                return;
        }
    }
}

int main(void)
{
    system_init();
    arm32_interrupt_enable(); // Enable interrupts

    writeUart("Playground\n");

    setup_uart2();

    // dma_test();
    // dma_test2();

    config.width = 320;
    config.height = 240;
    config.bus_width = DE_LCD_R_5BITS | DE_LCD_G_6BITS | DE_LCD_B_5BITS;
    config.bus_mode = DE_LCD_PARALLEL_RGB;

    config.pixel_clock_hz = 16000000; // Mhz

    config.h_front_porch = 8; // h
    config.h_back_porch = 70; // j
    config.h_sync_len = 1;    // k

    config.v_front_porch = 4; // v
    config.v_back_porch = 13; // b
    config.v_sync_len = 1;    // n

    config.h_sync_invert = 1;
    config.v_sync_invert = 1;

    init_display();

    timer_init();

    uint8_t commandBuffer[4] = {0}; // [cmd char] [num1] [num2] [num3]
    int commandPtr = 0;

    drawRect(rectx, recty, RECT_SIZE, RECT_SIZE, 0xFFFF);

    writeUart("Hi\n");

    while (1)
    {
        joy_update();

        if (uart_get_status(UART1) & UART_LSR_DR)
        {
            uint8_t rec = uart_get_rx(UART1);
            uart_tx(UART0, rec);

            if (rec == '\r')
            {
                uart_tx(UART1, '\r');
                uart_tx(UART1, '\n');

                exec_command(commandBuffer, commandPtr);

                memset(commandBuffer, 0, 3);
                commandPtr = 0;
            }
            else if (rec == '\b')
            {
                if (commandPtr > 0)
                {
                    commandBuffer[commandPtr] = 0;
                    commandPtr -= 1;
                }
            }
            else if (commandPtr < 4)
            {
                commandBuffer[commandPtr] = rec;
                commandPtr += 1;

                uart_tx(UART1, rec);
            }
        }

        if (systime - lastMoveTime >= 16)
        {
            lastMoveTime = systime;

            // Move controller rect
            if (buttons & 0x800 && !rect2ChangeColor)
            {
                rect2ChangeColor = 1;
                rect2Color = (rect2Color + 1) % 6;
            }
            else if (!(buttons & 0x800) && rect2ChangeColor)
            {
                rect2ChangeColor = 0;
            }

            if (leftStickX < 120)
            {
                rect2x -= ((120 - (float)leftStickX) / 120) * 5;
            }
            else if (leftStickX > 134)
            {
                rect2x += (((float)leftStickX - 134) / (255 - 134)) * 5;
            }

            if (rect2x < 0)
            {
                rect2x = 0;
            }
            else if (rect2x + RECT_SIZE >= DISPLAY_WIDTH)
            {
                rect2x = DISPLAY_WIDTH - RECT_SIZE;
            }

            if (leftStickY < 120)
            {
                rect2y += ((120 - (float)leftStickY) / 120) * 5;
            }
            else if (leftStickY > 134)
            {
                rect2y -= (((float)leftStickY - 134) / (255 - 134)) * 5;
            }

            if (rect2y < 0)
            {
                rect2y = 0;
            }
            else if (rect2y + RECT_SIZE >= DISPLAY_HEIGHT)
            {
                rect2y = DISPLAY_HEIGHT - RECT_SIZE;
            }

            // Move bouncy rect
            rectx += 1 * rectDirX;
            recty += 1 * rectDirY;

            int top = recty;
            int bottom = top + RECT_SIZE;
            int left = rectx;
            int right = left + RECT_SIZE;

            int bounce = 0;

            if (top < 0)
            {
                recty = 0;
                rectDirY = 1;

                bounce = 1;
            }
            else if (bottom >= DISPLAY_HEIGHT)
            {
                recty = DISPLAY_HEIGHT - RECT_SIZE;
                rectDirY = -1;

                bounce = 1;
            }

            if (left < 0)
            {
                rectx = 0;
                rectDirX = 1;

                bounce = 1;
            }
            else if (right >= DISPLAY_WIDTH)
            {
                rectx = DISPLAY_WIDTH - RECT_SIZE;
                rectDirX = -1;

                bounce = 1;
            }

            if (bounce)
            {
                rectColor = (rectColor + 1) % 6;
                writeUart("Bounce\n");
            }

            // Draw things
            memset(fb, 0, DISPLAY_WIDTH * DISPLAY_HEIGHT * 2);
            drawRect(rectx, recty, RECT_SIZE, RECT_SIZE, RECT_COLORS[rectColor]);
            drawRect((int)rect2x, (int)rect2y, RECT_SIZE, RECT_SIZE, RECT_COLORS[rect2Color]);

            // Swap FBs
            if (fb == fb1) 
            {
                fb = fb2;
                debe_layer_set_addr(1, fb1);
            }
            else
            {
                fb = fb1;
                debe_layer_set_addr(1, fb2);
            }
        }
    }

    return 0;
}
