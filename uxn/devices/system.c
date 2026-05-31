#include "console.h"
#include "stdio.h"
#include "string.h"
#include "pico/stdlib.h"
#include "../uxn11.h"
#include "system.h"

/*
Copyright (c) 2022 Devine Lu Linvega, Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

static const char *errors[] = {
	"Working-stack underflow",
	"Return-stack underflow",
	"Working-stack overflow",
	"Return-stack overflow",
	"Working-stack division by zero",
	"Return-stack division by zero",
	"Execution timeout"};

extern Uint8 dev[0x100];
extern Uint8 ptr[2];
extern Uint8 stk[2][0x100];
extern Uint8 ram[BANKS_CAP];

static void system_print(char *name, int r)
{
	Uint8 i;
	printf("%s%c", name, ptr[r] - 8 ? ' ' : '|');
	for (i = ptr[r] - 8; i != ptr[r]; i++)
		printf("%02x%c", stk[r][i], i == 0xff ? '|' : ' ');
	printf("<%02x\n", ptr[r]);
}

void system_deo_expansion(void)
{
	const Uint16 exp = peek2(dev + 2);
	Uint8 *aptr = ram + exp;
	Uint16 length = peek2(aptr + 1);
	unsigned int bank = peek2(aptr + 3) * 0x10000;
	unsigned int addr = peek2(aptr + 5);
	if (ram[exp] == 0x0)
	{
		if (bank < BANKS_CAP)
			memset(ram + bank + addr, ram[exp + 7], length);
	}
	else if (ram[exp] == 1 || ram[exp] == 2)
	{
		unsigned int dst_bank = peek2(aptr + 7) * 0x10000;
		unsigned int dst_addr = peek2(aptr + 9);
		if (bank < BANKS_CAP && dst_bank < BANKS_CAP)
			memmove(ram + dst_bank + dst_addr, ram + bank + addr, length);
	}
	else
		printf("Unknown command: %s\n", &ram[exp]);
}

Uint8 system_dei_wst(void)
{
	return ptr[0];
}
Uint8 system_dei_rst(void)
{
	return ptr[1];
}
void system_deo_wst(void)
{
	ptr[0] = dev[4];
}
void system_deo_rst(void)
{
	ptr[1] = dev[5];
}
void system_deo_print(void)
{
	system_print("WST", 0), system_print("RST", 1);
}