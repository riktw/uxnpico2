#ifndef PS2_KEYBOARD_H_
#define PS2_KEYBOARD_H_

#include "hardware/gpio.h"
#include <stdio.h>
#include <stdlib.h>
#include "ps2_shared.h"

// GPIO pins to VIA chip
enum ps2_pins
{
    PS2_DATA_PIN = 8,
    PS2_CLK_PIN
};

#define EXTENDED_BIT 6
#define SHIFT_BIT 5
#define CONTROL_BIT 4
#define ALT_BIT 3
#define RELEASE_BIT 1
#define KEYBOARD_EVENT_IDLE 0x8000

void initPS2(void);
uint16_t ps2GetEvent();
void clearPS2(void);
uint8_t ps2GetController(uint8_t, uint8_t);

#endif // PS2_KEYBOARD_H_
