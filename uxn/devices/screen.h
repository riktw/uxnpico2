/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#ifndef UXN_DEVICES_DISPLAY_H
#define UXN_DEVICES_DISPLAY_H

#include "../uxntypes.h"

void screen_init();
void screen_resize(int width, int height);
int screen_get_height();
int screen_get_width();

void screen_run_update(void);
int screen_get_vector(void);

void system_deo_colorize(void);

Uint8 screen_dei_rxhb(void);
Uint8 screen_dei_rxlb(void);
Uint8 screen_dei_ryhb(void);
Uint8 screen_dei_rylb(void);
Uint8 screen_dei_rahb(void);
Uint8 screen_dei_ralb(void);
void screen_deo_vector(void);
void screen_deo_width(void);
void screen_deo_height(void);
void screen_deo_auto(void);
void screen_deo_x(void);
void screen_deo_y(void);
void screen_deo_addr(void);
void screen_deo_pixel(void);
void screen_deo_sprite(void);

#endif