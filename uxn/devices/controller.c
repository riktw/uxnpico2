#include "../uxn11.h"
#include "controller.h"

#include "ps2_shared.h"
#include "hardware/dma.h"
#include "pico/stdlib.h"
#include <stdio.h>

// based on https://github.com/jimjag/JJ65c02/tree/main

extern Uint8 dev[0x100];
static int controller_vector = 0;

static uint ps2_sm;
static PIO ps2_pio;
static uint ps2_pio_irq;

static bool extended = 0;
static bool shift = 0;
static bool control = 0;
static bool alt = 0;
static bool release = 0;
static uint8_t storedControllerCode = 0;

#define CODE_EXTENDED 0xE0
#define CODE_RELEASED 0xF0
#define CODE_RIGHT_SHIFT 0x12
#define CODE_LEFT_SHIFT 0x59
#define CODE_CONTROL 0x14
#define CODE_ALT 0x11

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


// clang-format on
// this includes the USB keycodes
uint8_t GetControllerCode(uint8_t ascii)
{
	uint8_t mask = 0;
	if (ascii == 0x90 || ascii == 0x4a)
	{
		mask |= 0x08;
	}
	if (ascii == 0x11 || ascii == 0x52)
	{
		mask |= 0x10;
	}
	if (ascii == 0x12 || ascii == 0x51)
	{
		mask |= 0x20;
	}
	if (ascii == 0x14 || ascii == 0x50)
	{
		mask |= 0x40;
	}
	if (ascii == 0x13 || ascii == 0x4f)
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
		case CODE_EXTENDED: // key-release code 0xE0
			extended = 1;
			break;
		case CODE_RELEASED: // key-release code 0xF0
			release = 1;	// release flag
			if (extended)
			{
				extended = 0;
			}
			break;
		case CODE_LEFT_SHIFT:  // Left-side shift
		case CODE_RIGHT_SHIFT: // Right-side shift
			shift = 1;
			if (release)
			{
				shift = 0;
				release = 0;
			}
			break;
		case CODE_CONTROL: // Left or Right CNTL key (yep, same scancode)
			control = 1;
			if (release)
			{
				control = 0;
				release = 0;
			}

			break;
		case CODE_ALT: // Left or Right CNTL key (yep, same scancode)
			alt = 1;
			if (release)
			{
				alt = 0;
				release = 0;
			}
			break;
		default:
			code &= 0x7F;
			if (!release)
			{
				ascii = ps2_to_ascii_lower[code]; // default
				if (control)
				{
					// ascii = ps2_to_ascii_cntl[code]; // Seems uxn doesn't use these. just use normal ones?
					ascii = ps2_to_ascii_lower[code];
				}
				else if (shift)
				{
					ascii = ps2_to_ascii_upper[code];
				}
			}

			release = 0;
			extended = 0;
			break;
		}

		// Get the controller button, some like arrows are in ascii!
		uint8_t controllerButton = GetControllerCode(ascii);
		ascii = controllerButton ? 0 : ascii; // clear ascii if it was a controller button.
		controllerButton |= control;
		controllerButton |= (alt << 1);
		controllerButton |= (shift << 2);

		if (!release)
		{
			if (ascii)
			{
				controller_key(ascii);
			}
			else if (controllerButton)
			{
				storedControllerCode = controllerButton; // when released, the code needs to be send, hence it's stored
				controller_down(storedControllerCode);
			}
		}
		else
		{
			controller_up(storedControllerCode);
			storedControllerCode = 0;
		}
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

static uint32_t m_latchPin;
static uint32_t m_clockPin;
static uint32_t m_dataPin;
static uint8_t previousMask = 0;

void checkNesController()
{
	uint8_t mask = 0;
	gpio_put(m_latchPin, true);
	busy_wait_us(12);
	gpio_put(m_latchPin, false);
	busy_wait_us(6);

	for (int i = 0; i < 8; ++i)
	{
		mask |= !gpio_get(m_dataPin) << i;
		gpio_put(m_clockPin, true);
		busy_wait_us(6);
		gpio_put(m_clockPin, false);
		busy_wait_us(6);
	}

	uint8_t changes = previousMask ^ mask;
	if (changes & previousMask)
	{
		controller_up(changes);
	}
	else if (changes & mask)
	{
		controller_down(mask);
	}

	previousMask = mask;
}

void initNesController(uint32_t latchPin, uint32_t clockPin, uint32_t dataPin)
{
	m_latchPin = latchPin;
	m_clockPin = clockPin;
	m_dataPin = dataPin;

	gpio_init(m_latchPin);
	gpio_init(m_clockPin);
	gpio_init(m_dataPin);

	gpio_set_dir(m_latchPin, GPIO_OUT);
	gpio_set_dir(m_clockPin, GPIO_OUT);
	gpio_set_dir(m_dataPin, GPIO_IN);

	gpio_pull_up(m_dataPin);
}

// look up new key in previous keys
static inline bool find_key_in_report(hid_keyboard_report_t const *report, uint8_t keycode)
{
	for (uint8_t i = 0; i < 6; i++)
	{
		if (report->keycode[i] == keycode)
			return true;
	}

	return false;
}
static uint8_t const keycode2ascii[128][2] = {HID_KEYCODE_TO_ASCII};
// convert hid keycode to ascii and print via usb device CDC (ignore non-printable)
void process_kbd_report(uint8_t dev_addr, hid_keyboard_report_t const *report)
{
	(void)dev_addr;
	static hid_keyboard_report_t prev_report = {0, 0, {0}}; // previous report to check key released
	static uint8_t prevControllerButton = 0;

	// handle controller buttons
	uint8_t controllerButton = 0;
	for (uint8_t i = 0; i < 6; i++)
	{
		controllerButton |= GetControllerCode(report->keycode[i]);
	}
	controllerButton |= (report->modifier & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL)) ? 1 : 0;
	controllerButton |= ((report->modifier & (KEYBOARD_MODIFIER_LEFTALT | KEYBOARD_MODIFIER_RIGHTALT))) ? 2 : 0;
	controllerButton |= ((report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT))) ? 4 : 0;
	uint8_t changes = prevControllerButton ^ controllerButton;

	if (changes & prevControllerButton)
	{
		controller_up(changes);
	}
	else if (changes & controllerButton)
	{
		controller_down(controllerButton);
	}
	prevControllerButton = controllerButton;

	// handle ascii characters
	for (uint8_t i = 0; i < 6; i++)
	{
		uint8_t keycode = report->keycode[i];
		if (keycode)
		{
			if (find_key_in_report(&prev_report, keycode))
			{
				// exist in previous report means the current key is holding
			}
			else
			{
				bool const is_shift = report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
				uint8_t ch = keycode2ascii[keycode][is_shift ? 1 : 0];

				if (ch)
				{
					controller_key(ch);
				}
			}
		}
	}

	prev_report = *report;
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
