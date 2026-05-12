
#ifndef PS2_SHARED_H_
#define PS2_SHARED_H_

#define PS2FREQ 133600.0f

#include "hardware/pio.h"

void ps2in_init(PIO pio, uint sm, uint dat, float freq);
uint8_t SetBit(uint8_t input, uint8_t offset, bool bit);
bool CheckBit(uint8_t input, uint8_t offset);

#endif // PS2_SHARED_H_