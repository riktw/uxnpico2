/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#include "../uxn11.h"
#include "mouse.h"

#include "ps2_shared.h"
#include "hardware/dma.h"
#include "pico/stdlib.h"

typedef struct
{
	bool middleButton;
	bool rightButton;
	bool leftButton;
	int16_t movementX;
	int16_t movementY;

} mouseEvent_t;

extern Uint8 dev[0x100];
static int mouse_vector = 0;

static uint ps2_sm;
static PIO ps2_pio;
static uint ps2_pio_irq;

static uint initCounter = 0;
static bool initDone = false;

static uint32_t ps2_frame(uint8_t byte)
{
	bool parity = 1;
	for (uint8_t i = 0; i < 8; i++)
	{
		parity = parity ^ (byte >> i & 1);
	}
	return ((1 << 10) | (parity << 9) | (byte << 1)) ^ 0x7ff;
}

// ISR
static void __not_in_flash_func(ps2_ihandler)(void)
{
	static uint8_t movementArray[3] = {0};
	static uint8_t movementCounter = 0;
	static mouseEvent_t mouseEvent, previousMouseEvent;
	static int32_t xpos = 0;
	static int32_t ypos = 0;

	if (!pio_sm_is_rx_fifo_empty(ps2_pio, ps2_sm))
	{
		if (initDone)
		{
			movementArray[movementCounter] = (pio_sm_get(ps2_pio, ps2_sm) >> 23) & 0xFF;
			movementCounter++;
			if (movementCounter >= 3)
			{
				movementCounter = 0;
				mouseEvent.movementX = movementArray[1];
				mouseEvent.movementY = movementArray[2];

				mouseEvent.leftButton = CheckBit(movementArray[0], 0);
				mouseEvent.rightButton = CheckBit(movementArray[0], 1);
				mouseEvent.middleButton = CheckBit(movementArray[0], 2);

				mouseEvent.movementX = CheckBit(movementArray[0], 4) ? 0xFF00 | mouseEvent.movementX : mouseEvent.movementX;
				mouseEvent.movementY = CheckBit(movementArray[0], 5) ? 0xFF00 | mouseEvent.movementY : mouseEvent.movementY;

				if ((previousMouseEvent.leftButton == false) && (previousMouseEvent.middleButton == false) && (previousMouseEvent.rightButton == false))
				{
					if ((mouseEvent.leftButton == true) || (mouseEvent.middleButton == true) || (mouseEvent.rightButton == true))
					{
						mouse_down((mouseEvent.leftButton << 0) | (mouseEvent.middleButton << 1) | (mouseEvent.rightButton << 2));
					}
				}
				if ((mouseEvent.leftButton == false) && (mouseEvent.middleButton == false) && (mouseEvent.rightButton == false))
				{
					if ((previousMouseEvent.leftButton == true) || (previousMouseEvent.middleButton == true) || (previousMouseEvent.rightButton == true))
					{
						mouse_up((previousMouseEvent.leftButton << 0) | (previousMouseEvent.middleButton << 1) | (previousMouseEvent.rightButton << 2));
					}
				}
				uint16_t screen_width = peek2(&dev[0x22]);
				uint16_t screen_height = peek2(&dev[0x24]);
				previousMouseEvent = mouseEvent;
				xpos += mouseEvent.movementX;
				ypos -= mouseEvent.movementY;
				xpos = (xpos <= 0) ? 0 : xpos;
				ypos = (ypos <= 0) ? 0 : ypos;
				xpos = (xpos >= screen_width) ? screen_width : xpos;
				ypos = (ypos >= screen_height) ? screen_height : ypos; // program size, not max size
				mouse_pos(xpos, ypos);
			}
		}
		else
		{
			pio_sm_get(ps2_pio, ps2_sm);
			initCounter++;
		}
	}
	pio_interrupt_clear(ps2_pio, 0);
}

void initPS2Mouse(uint32_t dataPin)
{
	ps2_pio = pio1;
	ps2_pio_irq = PIO1_IRQ_0;
	ps2_sm = pio_claim_unused_sm(ps2_pio, true);
	ps2in_init(ps2_pio, ps2_sm, dataPin, PS2FREQ);
	pio_set_irq0_source_enabled(ps2_pio, pis_interrupt0, true);
	irq_set_exclusive_handler(ps2_pio_irq, ps2_ihandler);

	pio_sm_set_enabled(ps2_pio, ps2_sm, true);
	// get rid of noise on pins when PS/2 is enabled
	sleep_ms(1);
	pio_sm_clear_fifos(ps2_pio, ps2_sm);
	irq_set_enabled(ps2_pio_irq, true);

	pio_sm_put(ps2_pio, ps2_sm, ps2_frame(0xFF));
	while (initCounter < 3)
	{
		sleep_ms(10);
	}
	sleep_ms(10);

	pio_sm_put(ps2_pio, ps2_sm, ps2_frame(0xF6));
	while (initCounter < 4)
	{
		sleep_ms(10);
	}
	sleep_ms(10);

	pio_sm_put(ps2_pio, ps2_sm, ps2_frame(0xF4));
	while (initCounter < 5)
	{
		sleep_ms(10);
	}
	sleep_ms(10);

	initDone = true;
}

void mouse_down(Uint8 mask)
{
	dev[0x96] |= mask;
	uxn_eval(mouse_vector);
}

void mouse_up(Uint8 mask)
{
	dev[0x96] &= (~mask);
	uxn_eval(mouse_vector);
}

void mouse_pos(Uint16 x, Uint16 y)
{
	poke2(&dev[0x92], x);
	poke2(&dev[0x94], y);
	uxn_eval(mouse_vector);
}

void mouse_scroll(Uint16 x, Uint16 y)
{
	poke2(&dev[0x9A], x);
	poke2(&dev[0x9C], -y);
	uxn_eval(mouse_vector);
	poke2(&dev[0x9A], 0);
	poke2(&dev[0x9C], 0);
}

void mouse_deo_vector(void)
{
	mouse_vector = peek2(&dev[0x90]);
}