/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#ifndef UXN_DEVICES_MOUSE_H
#define UXN_DEVICES_MOUSE_H

#include <stdint.h>

void initPS2Mouse(uint32_t dataPin);

void mouse_deo_vector(void);
void mouse_down(Uint8 mask);
void mouse_up(Uint8 mask);
void mouse_pos(Uint16 x, Uint16 y);
void mouse_scroll(Uint16 x, Uint16 y);

#endif