#include "input.h"
#include <stdint.h>
#include <stdio.h>
#include "f1c100s_gpio.h"
#include "f1c100s_clock.h"
#include "f1c100s_uart.h"
#include "io.h"
#include "d_event.h"
#include "doomkeys.h"

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

    // printf("%d %d %d %d %02x\n", state.data.leftStickX, state.data.leftStickY, state.data.rightStickX, state.data.rightStickY, state.data.buttons);

    int lastLeft = (buttons & 0x20) != 0;
    int lastRight = (buttons & 0x40) != 0;
    int lastUp = (buttons & 0x10) != 0;
    int lastDown = (buttons & 0x80) != 0;
    int lastUse = (buttons & 0x800) != 0;
    int lastFire = (buttons & 0x08) != 0;
    int lastEnter = (buttons & 0x04) != 0;
    int lastEscape = (buttons & 0x02) != 0;
    int lastPrevWeapon = (buttons & 0x200) != 0;
    int lastNextWeapon = (buttons & 0x400) != 0;
    int lastMap = (buttons & 0x100) != 0;
    int lastStrafeLeft = 0;
    int lastStrafeRight = 0;

    if (leftStickY > 200)
    {
        lastUp = true;
    }
    else if (leftStickY < 100)
    {
        lastDown = true;
    }

    if (leftStickX < 100)
    {
        lastStrafeLeft = true;
    }
    else if (leftStickX > 200)
    {
        lastStrafeRight = true;
    }

    if (rightStickX < 100)
    {
        lastLeft = true;
    }
    else if (rightStickX > 200)
    {
        lastRight = true;
    }

    int currentLeft = (state.data.buttons & 0x20) != 0;
    int currentRight = (state.data.buttons & 0x40) != 0;
    int currentUp = (state.data.buttons & 0x10) != 0;
    int currentDown = (state.data.buttons & 0x80) != 0;
    int currentUse = (state.data.buttons & 0x800) != 0;
    int currentFire = (state.data.buttons & 0x08) != 0;
    int currentEnter = (state.data.buttons & 0x04) != 0;
    int currentEscape = (state.data.buttons & 0x02) != 0;
    int currentPrevWeapon = (state.data.buttons & 0x200) != 0;
    int currentNextWeapon = (state.data.buttons & 0x400) != 0;
    int currentMap = (state.data.buttons & 0x100) != 0;
    int currentStrafeLeft = 0;
    int currentStrafeRight = 0;

    if (state.data.leftStickY > 200)
    {
        currentUp = true;
    }
    else if (state.data.leftStickY < 100)
    {
        currentDown = true;
    }

    if (state.data.leftStickX > 200)
    {
        currentStrafeRight = true;
    }
    else if (state.data.leftStickX < 100)
    {
        currentStrafeLeft = true;
    }

    if (state.data.rightStickX < 100)
    {
        currentLeft = true;
    }
    else if (state.data.rightStickX > 200)
    {
        currentRight = true;
    }

    if (lastLeft != currentLeft)
    {
        event_t evt;
        evt.type = currentLeft ? ev_keydown : ev_keyup;
        evt.data1 = KEY_LEFTARROW;
        D_PostEvent(&evt);
    }
    if (lastRight != currentRight)
    {
        event_t evt;
        evt.type = currentRight ? ev_keydown : ev_keyup;
        evt.data1 = KEY_RIGHTARROW;
        D_PostEvent(&evt);
    }
    if (lastUp != currentUp)
    {
        event_t evt;
        evt.type = currentUp ? ev_keydown : ev_keyup;
        evt.data1 = KEY_UPARROW;
        D_PostEvent(&evt);
    }
    if (lastDown != currentDown)
    {
        event_t evt;
        evt.type = currentDown ? ev_keydown : ev_keyup;
        evt.data1 = KEY_DOWNARROW;
        D_PostEvent(&evt);
    }
    if (lastUse != currentUse)
    {
        event_t evt;
        evt.type = currentUse ? ev_keydown : ev_keyup;
        evt.data1 = KEY_USE;
        D_PostEvent(&evt);
    }
    if (lastFire != currentFire)
    {
        event_t evt;
        evt.type = currentFire ? ev_keydown : ev_keyup;
        evt.data1 = KEY_FIRE;
        D_PostEvent(&evt);
    }
    if (lastEnter != currentEnter)
    {
        event_t evt;
        evt.type = currentEnter ? ev_keydown : ev_keyup;
        evt.data1 = KEY_ENTER;
        D_PostEvent(&evt);
    }
    if (lastStrafeLeft != currentStrafeLeft)
    {
        event_t evt;
        evt.type = currentStrafeLeft ? ev_keydown : ev_keyup;
        evt.data1 = KEY_STRAFE_L;
        D_PostEvent(&evt);
    }
    if (lastStrafeRight != currentStrafeRight)
    {
        event_t evt;
        evt.type = currentStrafeRight ? ev_keydown : ev_keyup;
        evt.data1 = KEY_STRAFE_R;
        D_PostEvent(&evt);
    }
    if (lastEscape != currentEscape)
    {
        event_t evt;
        evt.type = currentEscape ? ev_keydown : ev_keyup;
        evt.data1 = KEY_ESCAPE;
        D_PostEvent(&evt);
    }
    if (lastPrevWeapon != currentPrevWeapon)
    {
        event_t evt;
        evt.type = currentPrevWeapon ? ev_keydown : ev_keyup;
        evt.data1 = KEY_PREV_WEAPON;
        D_PostEvent(&evt);
    }
    if (lastNextWeapon != currentNextWeapon)
    {
        event_t evt;
        evt.type = currentNextWeapon ? ev_keydown : ev_keyup;
        evt.data1 = KEY_NEXT_WEAPON;
        D_PostEvent(&evt);
    }
    if (lastMap != currentMap)
    {
        event_t evt;
        evt.type = currentMap ? ev_keydown : ev_keyup;
        evt.data1 = KEY_TAB;
        D_PostEvent(&evt);
    }

    leftStickX = state.data.leftStickX;
    leftStickY = state.data.leftStickY;
    rightStickX = state.data.rightStickX;
    rightStickY = state.data.rightStickY;
    buttons = state.data.buttons;
}

void input_init()
{
    printf("Input init on UART0\n");

    uint32_t cfg0 = read32(GPIOE + GPIO_CFG0);
    cfg0 = (cfg0 & ~(7 << 4)) | (0x03 << 4); // Enable UART0_TX on pin PE1
    cfg0 = (cfg0 & ~7) | 0x03; // Enable UART0_TX on pin PE0
    write32(GPIOE + GPIO_CFG0, cfg0);

    gpio_init(GPIOE, PIN0 | PIN1, GPIO_MODE_AF5, GPIO_PULL_NONE, GPIO_DRV_3);

    clk_enable(CCU_BUS_CLK_GATE2, 20);
    clk_reset_clear(CCU_BUS_SOFT_RST2, 20);
    uart_init(UART0, 57600);
}

void input_update()
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
