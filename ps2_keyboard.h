
#ifndef P2_KEYBOARD_H_
#define P2_KEYBOARD_H_
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <stdlib.h>

#define PS2FREQ 133600.0f

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

uint8_t SetBit(uint8_t input, uint8_t offset, bool bit);
bool CheckBit(uint8_t input, uint8_t offset);

void initPS2(void);
uint16_t ps2GetEvent();
void clearPS2(void);
uint8_t ps2GetController(uint8_t, uint8_t);

#endif // P2_KEYBOARD_H_
