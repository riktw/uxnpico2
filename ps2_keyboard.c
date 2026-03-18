/**
 * MIT License
 * Copyright (c) 2021-2024 Jim Jagielski
 * https://github.com/jimjag/JJ65c02/tree/main
 */

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <stdlib.h>
// Our assembled program:
// pioasm converts foo.pio to fio.pio.h
#include "ps2_keyboard.pio.h"
// Header file
#include "ps2_keyboard.h"

static uint ps2_offset;
static uint ps2_sm;
static PIO ps2_pio;
static uint ps2_pio_irq;
static void ps2_ihandler();

static bool extended;
static bool event;
static bool release;
volatile static keyboardStatus_t status;

static unsigned char keybuf[128];
volatile static unsigned char *rptr = keybuf;
volatile static unsigned char *wptr = keybuf;

void initPS2(void)
{
    ps2_pio = pio2;
    ps2_pio_irq = PIO2_IRQ_1;
    ps2_offset = pio_add_program(ps2_pio, &ps2_program);
    ps2_sm = pio_claim_unused_sm(ps2_pio, true);
    ps2_program_init(ps2_pio, ps2_sm, ps2_offset, PS2_DATA_PIN, PS2FREQ);
    pio_set_irq1_source_enabled(ps2_pio, pis_interrupt1, true);
    irq_set_exclusive_handler(ps2_pio_irq, ps2_ihandler);
    irq_set_enabled(ps2_pio_irq, true);
    pio_sm_set_enabled(ps2_pio, ps2_sm, true);
    // get rid of noise on pins when the PS/2 keyboard is enabled
    sleep_ms(1);
    pio_sm_clear_fifos(ps2_pio, ps2_sm);

    rptr = wptr = keybuf;
}

// Run this even after we INIT and before we start ps2Task()
void clearPS2(void)
{
    pio_sm_clear_fifos(ps2_pio, ps2_sm);
    release = status.shift = status.cntl = status.caps = 0;
    rptr = wptr = keybuf;
}
// clang-format off

// SPECIAL CASE:
//   Left Arrow:  scancode 107 -> Ascii 0x14   Esc[D
//   Right Arrow: scancode 116 -> Ascii 0x13   Esc[C
//   Down Arrow:  scancode 114 -> Ascii 0x12   Esc[B
//   Up Arrow:    scancode 117 -> Ascii 0x11   Esc[A
//   Home:        scancode 108 -> Abuse and use 0x90 lol
//
static const char __in_flash() ps2_to_ascii_lower[] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09,  '`', 0x00,
0x00, 0x00, 0x00, 0x00, 0x00,  'q',  '1', 0x00, 0x00, 0x00,  'z',  's',  'a',  'w',  '2', 0x00,
0x00,  'c',  'x',  'd',  'e',  '4',  '3', 0x00, 0x00,  ' ',  'v',  'f',  't',  'r',  '5', 0x00,
0x00,  'n',  'b',  'h',  'g',  'y',  '6', 0x00, 0x00, 0x00,  'm',  'j',  'u',  '7',  '8', 0x00,
0x00,  ',',  'k',  'i',  'o',  '0',  '9', 0x00, 0x00,  '.',  '/',  'l',  ';',  'p',  '-', 0x00,
0x00, 0x00, '\'', 0x00,  '[',  '=', 0x00, 0x00, 0x00, 0x00, 0x0D,  ']', 0x00, '\\', 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x14, 0x90, 0x00, 0x00, 0x00,
0x00, 0x00, 0x12, 0x00, 0x13, 0x11, 0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const char __in_flash() ps2_to_ascii_upper[] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09,  '~', 0x00,
0x00, 0x00, 0x00, 0x00, 0x00,  'Q',  '!', 0x00, 0x00, 0x00,  'Z',  'S',  'A',  'W',  '@', 0x00,
0x00,  'C',  'X',  'D',  'E',  '$',  '#', 0x00, 0x00,  ' ',  'V',  'F',  'T',  'R',  '%', 0x00,
0x00,  'N',  'B',  'H',  'G',  'Y',  '^', 0x00, 0x00, 0x00,  'M',  'J',  'U',  '&',  '*', 0x00,
0x00,  '<',  'K',  'I',  'O',  ')',  '(', 0x00, 0x00,  '>',  '?',  'L',  ':',  'P',  '_', 0x00,
0x00, 0x00, 0x22, 0x00,  '{',  '+', 0x00, 0x00, 0x00, 0x00, 0x0D,  '}', 0x00,  '|', 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x14, 0x90, 0x00, 0x00, 0x00,
0x00, 0x00, 0x12, 0x00, 0x13, 0x11, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const char __in_flash() ps2_to_ascii_cntl[] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09,  '~', 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x11,  '!', 0x00, 0x00, 0x00, 0x1A, 0x13, 0x01, 0x17,  '@', 0x00,
0x00, 0x03, 0x18, 0x04, 0x05,  '$',  '#', 0x00, 0x00,  ' ', 0x16, 0x06, 0x14, 0x12,  '%', 0x00,
0x00, 0x0E, 0x02, 0x08, 0x07, 0x19,  '^', 0x00, 0x00, 0x00, 0x0D, 0x0A, 0x15,  '&',  '*', 0x00,
0x00,  '<', 0x0B, 0x09, 0x0F,  ')',  '(', 0x00, 0x00,  '>',  '?', 0x0C,  ':', 0x10,  '_', 0x00,
0x00, 0x00, 0x22, 0x00,  '{',  '+', 0x00, 0x00, 0x00, 0x00, 0x0D,  '}', 0x00,  '|', 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x14, 0x90, 0x00, 0x00, 0x00,
0x00, 0x00, 0x12, 0x00, 0x13, 0x11, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// ISR
void __not_in_flash_func(ps2_ihandler)(void)   
{ 
    unsigned char ascii = 0; 
    if (pio_sm_is_rx_fifo_empty(ps2_pio, ps2_sm)) {
        pio_interrupt_clear(ps2_pio, 1);
        return;
    }
    event = 1;
    // pull a scan code from the PIO SM fifo
    // uint8_t code = *((io_rw_8*)&ps2_pio->rxf[ps2_sm] + 3);
    uint8_t code = pio_sm_get(ps2_pio, ps2_sm) >> 24;
    switch (code) {
        case 0xE0:
            extended = 1;
            event = 0;  // no event yet!
            break;
        case 0xF0:               // key-release code 0xF0
            release = 1;         // release flag
            status.release = 1;
            break;
        case 0x58:               // Caps Lock?
            if (status.release) {
                release = 0;
            } else {
                status.caps = !status.caps;
            }
            break;

        case 0x12:               // Left-side shift
        case 0x59:               // Right-side shift
            if (release) {
                status.shift = 0;
                release = 0;
            } else {
                status.shift = 1;
            }
            break;
        case 0x14:               // Left or Right CNTL key (yep, same scancode)
            if (release) {
                status.cntl = 0;
                release = 0;
            } else {
                status.cntl = 1;
            }
            break;
        case 0x11:               // Left or Right ALT key (yep, same scancode)
            if (release) {
                status.alt = 0;
                release = 0;
            } else {
                status.alt = 1;
            }
            break;
        default:
            code &= 0x7F;
            if (!status.release) {
                if (status.cntl) {
                    ascii = ps2_to_ascii_cntl[code];
                } else if (status.shift) {
                    ascii = ps2_to_ascii_upper[code];
                } else {
                    ascii = ps2_to_ascii_lower[code];
                    if (status.caps && (ascii >= 'a' && ascii <= 'z')) {
                        ascii -= 32;
                    }
                }
                
            }
            release = 0;
            extended = 0;
            break;
    }
    if(!extended || !release)
        *wptr++ = ascii;
    
    if (wptr >= (keybuf + sizeof(keybuf)))
        wptr = keybuf;
    pio_interrupt_clear(ps2_pio, 1);
}

keyboardStatus_t ps2GetChar() 
{
    unsigned char ascii = 0;
    keyboardStatus_t returnstatus = status;
    status.release = 0;
    returnstatus.event = event;
    if (rptr != wptr) {
        returnstatus.ascii = *rptr++;
        if (rptr >= (keybuf + sizeof(keybuf)))
            rptr = keybuf;
    }
    if (rptr == wptr) { //If no more ascii characters are left to read, we are done with the event. 
        event = 0;
    }
    return returnstatus;
}

uint8_t ps2GetController()
{
    uint8_t mask = 0;
    if(status.cntl)
    {
        mask |= 0x01;
    }
    if(status.alt)
    {
        mask |= 0x02;
    }
    if(status.shift)
    {
        mask |= 0x04;
    }
    if(*rptr == 0x90)
    {
        mask |= 0x08;
    }
    if(*rptr == 0x11)
    {
        mask |= 0x10;
    }
    if(*rptr == 0x12)
    {
        mask |= 0x20;
    }
    if(*rptr == 0x14)
    {
        mask |= 0x40;
    }
    if(*rptr == 0x13)
    {
        mask |= 0x80;
    }
    return mask;
}
