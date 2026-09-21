/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#ifndef UXN_DEVICES_CONTROLLER_H
#define UXN_DEVICES_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "pio_usb.h"
#include "tusb.h"

void initPS2Keyboard(uint32_t dataPin);
void initNesController(uint32_t latchPin, uint32_t clockPin, uint32_t dataPin);
void checkNesController();
void process_kbd_report(uint8_t dev_addr, hid_keyboard_report_t const *report);

void controller_deo_vector(void);
void controller_down(Uint8 mask);
void controller_up(Uint8 mask);
void controller_key(Uint8 key);
void controller_special(Uint8 key);

#endif