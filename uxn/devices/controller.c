#include "../uxn11.h"
#include "controller.h"

#include "../../ps2_shared.h"
#include "hardware/dma.h"
#include "pico/stdlib.h"

/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

extern Uint8 dev[0x100];
static int controller_vector = 0;

static uint ps2_sm;
static PIO ps2_pio;
static uint ps2_pio_irq;

// clean up this below, can probably simplify this entire routine and remove ringbuffer and such
static bool extended = 0;
static bool shift = 0;
static bool control = 0;
static bool alt = 0;
static bool releaseaction = 0;
static bool release = 0;

static uint16_t keybuf[128];
static uint8_t keybufIndexHead = 0;
static uint8_t keybufIndexTail = 0;

#define CODE_EXTENDED 0xE0
#define CODE_RELEASED 0xF0
#define CODE_RIGHT_SHIFT 0x12
#define CODE_LEFT_SHIFT 0x59
#define CODE_CONTROL 0x14
#define CODE_ALT 0x11

#define EXTENDED_BIT 6
#define SHIFT_BIT 5
#define CONTROL_BIT 4
#define ALT_BIT 3
#define RELEASE_BIT 1
#define KEYBOARD_EVENT_IDLE 0x80

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

// clang-format on

uint8_t GetControllerCode(uint8_t control, uint8_t ascii)
{
	uint8_t mask = 0;
	if (CheckBit(control, CONTROL_BIT))
	{
		mask |= 0x01;
	}
	if (CheckBit(control, ALT_BIT))
	{
		mask |= 0x02;
	}
	if (CheckBit(control, SHIFT_BIT))
	{
		mask |= 0x04;
	}
	if (ascii == 0x90)
	{
		mask |= 0x08;
	}
	if (ascii == 0x11)
	{
		mask |= 0x10;
	}
	if (ascii == 0x12)
	{
		mask |= 0x20;
	}
	if (ascii == 0x14)
	{
		mask |= 0x40;
	}
	if (ascii == 0x13)
	{
		mask |= 0x80;
	}

	return mask;
}

// ISR
void __not_in_flash_func(ps2_ihandler)(void)
{
	unsigned char ascii = 0;

	if (!pio_sm_is_rx_fifo_empty(ps2_pio, ps2_sm))
	{
		// pull a scan code from the PIO SM fifo
		uint8_t code = (pio_sm_get(ps2_pio, ps2_sm) >> 23) & 0xFF;

		switch (code)
		{
		case CODE_EXTENDED:
			// Extended code
			extended = 1;
			break;
		case CODE_RELEASED: // key-release code 0xF0
			release = 1;	// release flag
			break;
		case 0x58: // Caps Lock?
				   // caps lock, not handled for now
			break;
		case CODE_RIGHT_SHIFT: // Left-side shift
		case CODE_LEFT_SHIFT:  // Right-side shift
			shift = 1;
			break;
		case CODE_CONTROL: // Left or Right CNTL key (yep, same scancode)
			control = 1;
			break;
		case CODE_ALT: // Left or Right ALT key (yep, same scancode)
			alt = 1;
			break;
		default:
			code &= 0x7F;
			if (extended)
			{
				extended = 0;
			}

			if (control)
			{
				ascii = ps2_to_ascii_cntl[code];
			}
			else if (shift)
			{
				ascii = ps2_to_ascii_upper[code];
			}
			else
			{
				ascii = ps2_to_ascii_lower[code];
			}

			break;
		}

		// TODO: clean this all up. I think most control codes can go as nothing needs to be stored.
		uint8_t controlFlags = 0;
		controlFlags = SetBit(controlFlags, 7, 1); // always have 1 bit high for checking if an entry was there.
		controlFlags = SetBit(controlFlags, EXTENDED_BIT, extended);
		controlFlags = SetBit(controlFlags, SHIFT_BIT, shift);
		controlFlags = SetBit(controlFlags, CONTROL_BIT, control);
		controlFlags = SetBit(controlFlags, ALT_BIT, alt);
		controlFlags = SetBit(controlFlags, RELEASE_BIT, releaseaction);

		shift = 0;
		alt = 0;
		control = 0;

		if (!release)
		{
			if (controlFlags != KEYBOARD_EVENT_IDLE || ascii != 0)
			{
				uint8_t controllerCode = GetControllerCode(controlFlags, ascii);
				if (CheckBit(controlFlags, RELEASE_BIT))
				{
					controller_up(controllerCode);
				}
				else if (!CheckBit(controlFlags, EXTENDED_BIT))
				{
					if (ascii)
					{
						controller_key(ascii);
					}
					controller_down(controllerCode);
				}
			}
		}
		releaseaction = release;
		release = 0;
	}
	pio_interrupt_clear(ps2_pio, 0);
}

void initPS2Keyboard(uint32_t dataPin)
{
	ps2_pio = pio2;
	ps2_pio_irq = PIO2_IRQ_0;
	ps2_sm = pio_claim_unused_sm(ps2_pio, true);
	ps2in_init(ps2_pio, ps2_sm, dataPin, PS2FREQ);
	pio_set_irq0_source_enabled(ps2_pio, pis_interrupt0, true);
	irq_set_exclusive_handler(ps2_pio_irq, ps2_ihandler);
	irq_set_enabled(ps2_pio_irq, true);
	pio_sm_set_enabled(ps2_pio, ps2_sm, true);
	// get rid of noise on pins when the PS/2 keyboard is enabled
	sleep_ms(1);
	pio_sm_clear_fifos(ps2_pio, ps2_sm);
}

void controller_down(Uint8 mask)
{
	if (mask)
	{
		dev[0x82] |= mask;
		if (controller_vector)
			uxn_eval(controller_vector);
	}
}

void controller_up(Uint8 mask)
{
	if (mask)
	{
		dev[0x82] &= (~mask);
		if (controller_vector)
			uxn_eval(controller_vector);
	}
}

void controller_key(Uint8 key)
{
	if (key)
	{
		dev[0x83] = key;
		if (controller_vector)
			uxn_eval(controller_vector);
		dev[0x83] = 0;
	}
}

void controller_special(Uint8 key)
{
	if (key)
	{
		dev[0x84] = key;
		if (controller_vector)
			uxn_eval(controller_vector);
		dev[0x84] = 0;
	}
}

void controller_deo_vector(void)
{
	controller_vector = peek2(&dev[0x80]);
}
