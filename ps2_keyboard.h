
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

typedef struct
{
    bool event;
    bool release; // Flag indicates the release of a key
    bool shift;   // Shift indication
    bool cntl;    // Control indication
    bool caps;    // Caps Lock
    bool alt;     // Alt key
    char ascii;
} keyboardStatus_t;

void initPS2(void);
keyboardStatus_t ps2GetChar();
void clearPS2(void);
uint8_t ps2GetController();

#endif // P2_KEYBOARD_H_
