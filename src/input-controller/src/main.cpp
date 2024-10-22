#include <Arduino.h>

#define LEFT_STICK_X PA0
#define LEFT_STICK_Y PA2
#define LEFT_STICK_BUTTON PA1

#define RIGHT_STICK_X PA3
#define RIGHT_STICK_Y PA5
#define RIGHT_STICK_BUTTON PA4

#define BUTTONS_ROW_1 PB9
#define BUTTONS_ROW_2 PB8
#define BUTTONS_ROW_3 PB7
#define BUTTONS_COL_1 PB6
#define BUTTONS_COL_2 PB5
#define BUTTONS_COL_3 PB4
#define BUTTONS_COL_4 PB3

#define LEFT_STICK_X_MIN 155
#define LEFT_STICK_X_DEAD_MIN 500
#define LEFT_STICK_X_DEAD_MAX 600
#define LEFT_STICK_X_MAX 940

#define LEFT_STICK_Y_MIN 95
#define LEFT_STICK_Y_DEAD_MIN 450
#define LEFT_STICK_Y_DEAD_MAX 520
#define LEFT_STICK_Y_MAX 880

#define RIGHT_STICK_X_MIN 210
#define RIGHT_STICK_X_DEAD_MIN 450
#define RIGHT_STICK_X_DEAD_MAX 500
#define RIGHT_STICK_X_MAX 760

#define RIGHT_STICK_Y_MIN 210
#define RIGHT_STICK_Y_DEAD_MIN 480
#define RIGHT_STICK_Y_DEAD_MAX 510
#define RIGHT_STICK_Y_MAX 800

uint8_t lastRaw[6] = { 0 };

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

static int compareStates(uint8_t *state1, uint8_t *state2)
{
    for (int x = 0; x < 6; x++)
    {
        if (state1[x] != state2[x])
        {
            return true;
        }
    }

    return false;
}

static uint8_t stickToValue(uint32_t value, int min, int minDead, int maxDead, int max)
{
    if (value <= min)
    {
        return 0;
    }

    if (value <= minDead)
    {
        return (((float)value - min) / (minDead - min)) * 127;
    }

    if (value > max)
    {
        return 255;
    }

    if (value >= maxDead)
    {
        return 127 + (((float)value - maxDead) / (max - maxDead)) * 128;
    }

    return 127;
}

static uint8_t stickToValueInverted(uint32_t value, int min, int minDead, int maxDead, int max)
{
    if (value <= min)
    {
        return 255;
    }

    if (value <= minDead)
    {
        return 127 + (128 - (((float)value - min) / (minDead - min)) * 128);
    }

    if (value > max)
    {
        return 0;
    }

    if (value >= maxDead)
    {
        return 127 - (((float)value - maxDead) / (max - maxDead)) * 127;
    }

    return 127;
}

static inline void setButtonRows(int activeRow)
{
    digitalWrite(BUTTONS_ROW_1, activeRow == 1);
    digitalWrite(BUTTONS_ROW_2, activeRow == 2);
    digitalWrite(BUTTONS_ROW_3, activeRow == 3);
}

static uint8_t readButtonRow()
{
    int b1 = digitalRead(BUTTONS_COL_1);
    int b2 = digitalRead(BUTTONS_COL_2);
    int b3 = digitalRead(BUTTONS_COL_3);
    int b4 = digitalRead(BUTTONS_COL_4);

    return (b1 | (b2 << 1) | (b3 << 2) | (b4 << 3));
}

static uint16_t scanButtons()
{
    uint16_t buttons = 0;

    for (int row = 0; row < 3; row++)
    {
        setButtonRows(row + 1);

        delayMicroseconds(250);
        uint8_t rowValue = readButtonRow();

        for (int debounce = 0; debounce < 4; debounce++)
        {
            delayMicroseconds(250);
            rowValue &= readButtonRow();
        }

        buttons |= rowValue << (row * 4);
    }

    setButtonRows(0);

    return buttons;
}

void setup()
{
    Serial3.begin(57600);

    pinMode(LEFT_STICK_X, INPUT_ANALOG);
    pinMode(LEFT_STICK_Y, INPUT_ANALOG);
    pinMode(LEFT_STICK_BUTTON, INPUT_PULLUP);

    pinMode(RIGHT_STICK_X, INPUT_ANALOG);
    pinMode(RIGHT_STICK_Y, INPUT_ANALOG);
    pinMode(RIGHT_STICK_BUTTON, INPUT_PULLUP);

    pinMode(BUTTONS_ROW_1, OUTPUT);
    pinMode(BUTTONS_ROW_2, OUTPUT);
    pinMode(BUTTONS_ROW_3, OUTPUT);

    pinMode(BUTTONS_COL_1, INPUT_PULLDOWN);
    pinMode(BUTTONS_COL_2, INPUT_PULLDOWN);
    pinMode(BUTTONS_COL_3, INPUT_PULLDOWN);
    pinMode(BUTTONS_COL_4, INPUT_PULLDOWN);
}

void loop()
{
    delay(10);

    int leftButton = digitalRead(LEFT_STICK_BUTTON);
    uint32_t leftX = analogRead(LEFT_STICK_X);
    uint32_t leftY = analogRead(LEFT_STICK_Y);

    int rightButton = digitalRead(RIGHT_STICK_BUTTON);
    uint32_t rightX = analogRead(RIGHT_STICK_X);
    uint32_t rightY = analogRead(RIGHT_STICK_Y);

    uint8_t leftXValue = stickToValue(leftX, LEFT_STICK_X_MIN, LEFT_STICK_X_DEAD_MIN, LEFT_STICK_X_DEAD_MAX, LEFT_STICK_X_MAX);
    uint8_t leftYValue = stickToValue(leftY, LEFT_STICK_Y_MIN, LEFT_STICK_Y_DEAD_MIN, LEFT_STICK_Y_DEAD_MAX, LEFT_STICK_Y_MAX);

    uint8_t rightXValue = stickToValueInverted(rightX, RIGHT_STICK_X_MIN, RIGHT_STICK_X_DEAD_MIN, RIGHT_STICK_X_DEAD_MAX, RIGHT_STICK_X_MAX);
    uint8_t rightYValue = stickToValueInverted(rightY, RIGHT_STICK_Y_MIN, RIGHT_STICK_Y_DEAD_MIN, RIGHT_STICK_Y_DEAD_MAX, RIGHT_STICK_Y_MAX);

    uint16_t buttons = scanButtons();
    if (!leftButton) // Stick buttons are inverted
    {
        buttons |= (1 << 12);
    }
    if (!rightButton)
    {
        buttons |= (1 << 13);
    }

    state.data.leftStickX = leftXValue;
    state.data.leftStickY = leftYValue;
    state.data.rightStickX = rightXValue;
    state.data.rightStickY = rightYValue;
    state.data.buttons = buttons;

    // Check if state changed
    int changed = compareStates(lastRaw, state.raw);
    if (!changed)
    {
        return;
    }

    memcpy(lastRaw, state.raw, 6);

    Serial3.write((uint8_t)0x55);
    Serial3.write((uint8_t)0xAA);
    Serial3.write((const uint8_t*)state.raw, 6);
}

/*
buttons:
    left dpad:
        -left: 0x20
        -right: 0x40
        -top: 0x10
        -bottom: 0x80
        
    right dpad:
        -left: 0x200
        -right: 0x400
        -top: 0x100
        -bottom: 0x800
        
    other:
        -left shoulder: 0x01
        -right shoulder: 0x08
        -left action: 0x02
        -right action: 0x04
        
    sticks:
        -left: 0x1000
        -right: 0x2000
*/
