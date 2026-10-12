#ifndef UXN_UXN11_H
#define UXN_UXN11_H

/*
Copyright (c) 2021 Devine Lu Linvega

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#include "uxntypes.h"

#define PAGE_PROGRAM 0x0100
#define BANK_SIZE 0x10000
#define BANKS 0x2
#define BANKS_CAP BANKS *BANK_SIZE
#define WIDTH (64 * 8)
#define HEIGHT (45 * 8)

/* clang-format off */

#define DEVPEEK16(o, x) { (o) = (d->dat[(x)] << 8) + d->dat[(x) + 1]; }
#define DEVPOKE16(x, y) { d->dat[(x)] = (y) >> 8; d->dat[(x) + 1] = (y); }
#define GETVECTOR(d) ((d)->dat[0] << 8 | (d)->dat[1])

/* clang-format on */

typedef struct
{
	Uint8 ptr, dat[255];
} Stack;

typedef struct Device
{
	struct Uxn *u;
	Uint8 dat[16];
	Uint8 (*dei)(struct Device *d, Uint8);
	void (*deo)(struct Device *d, Uint8);
} Device;

typedef struct Uxn
{
	Uint8 *ram;
	Uint8 stk[2][0x100];
	Uint8 ptr[2];
	Device dev[16];
} Uxn11;

void screen_update(void);
unsigned int console_input(int c, unsigned int type);
int uxn_boot(Uxn11 *u, Uint8 *ram);
int uxn_init();
unsigned int uxn_eval(Uint16 pc);
int uxn_load_rom(char *filename);
// int uxn_interrupt(void);
// int uxn_halt(Uxn *u, Uint8 error, Uint16 addr);
Device *uxn_port(Uxn11 *u, Uint8 id, Uint8 (*deifn)(Device *, Uint8), void (*deofn)(Device *, Uint8));
#endif /* UXN_UXN_H */
