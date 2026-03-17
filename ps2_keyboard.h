
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

void initPS2(void);
unsigned char ps2GetChar();
unsigned char ps2GetCharBlk();
void clearPS2(void);
uint8_t ps2IsReleased();
uint8_t ps2GetController(char c);

#endif // P2_KEYBOARD_H_
