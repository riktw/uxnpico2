#include "ps2_shared.h"
#include "hardware/clocks.h"
#include "ps2_input.pio.h"

uint8_t SetBit(uint8_t input, uint8_t offset, bool bit)
{
    if (bit)
    {
        return (input | (1 << offset));
    }
    else
    {
        return input;
    }
}

bool CheckBit(uint8_t input, uint8_t offset)
{
    return (input & (1 << offset));
}

void ps2in_init(PIO pio, uint sm, uint dat, float freq)
{
    uint offset = pio_add_program(pio, &ps2in_program);
    ps2in_program_init(pio, sm, offset, dat, freq);
}
