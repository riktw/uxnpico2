#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "uxn11.h"
#include "devices/mouse.h"

/*
Copyright (c) 2021-2026 Devine Lu Linvega, Andrew Alderwick,
Andrew Richards, Eiríkr Åsheim, Sigrid Solveig Haflínudóttir

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.

cc -DNDEBUG -O2 -g0 -s src/uxn11.c -lX11 -lutil -o bin/uxn11
*/

Uint8 ram[0x10000], dev[0x100], stk[2][0x100], ptr[2];
static int console_vector, screen_vector, controller_vector;
static int rX, rY, rA, rMX, rMY, rMA, rML, rDX, rDY, rL1, rL2;

uint32_t instructionCount = 0;

/* clang-format off */

#define BANKS 0x10
#define BANKS_CAP BANKS * 0x10000
#define WIDTH (64 * 8)
#define HEIGHT (45 * 8)

/* clang-format on */

/*
@|System ------------------------------------------------------------ */

static char *system_boot_path;

/* TODO
static void system_print(char *name, int r)
{
	Uint8 i;
	fprintf(stderr, "%s%c", name, ptr[r] - 8 ? ' ' : '|');
	for(i = ptr[r] - 8; i != ptr[r]; i++)
		fprintf(stderr, "%02x%c", stk[r][i], i == 0xff ? '|' : ' ');
	fprintf(stderr, "<%02x\n", ptr[r]);
}

static unsigned int system_load(const char *rom_path)
{
	FILE *f = fopen(rom_path, "rb");
	if(f) {
		unsigned int i = 0, l = fread(ram + 0x100, 0x10000 - 0x100, 1, f);
		while(l && ++i < BANKS)
			l = fread(ram + i * 0x10000, 0x10000, 1, f);
		fclose(f);
	}
	return !!f;
}
	*/

static unsigned int system_boot(char *rom_path, const unsigned int has_args)
{
	system_boot_path = rom_path;
	dev[0x17] = has_args;
	return 0; // ram;// && system_load(rom_path);
}

static unsigned int system_reboot(const unsigned int soft)
{
	memset(dev, 0, 0x100);
	memset(stk[0], 0, 0x100);
	memset(stk[1], 0, 0x100);
	if (soft)
		memset(ram + 0x100, 0, 0xff00);
	else
		memset(ram, 0, 0x10000);
	ptr[0] = ptr[1] = 0;
	console_vector = screen_vector = controller_vector = 0;
	rX = rY = rA = rMX = rMY = rMA = rML = rDX = rDY = rL1 = rL2 = 0;
	return uxn_load_rom(system_boot_path);
}

/* clang-format off */

static Uint8 system_dei_wst(void) { return ptr[0]; }
static Uint8 system_dei_rst(void) { return ptr[1]; }
static void system_deo_wst(void) { ptr[0] = dev[4]; }
static void system_deo_rst(void) { ptr[1] = dev[5]; }
//static void system_deo_print(void) { system_print("WST", 0), system_print("RST", 1); }

/* clang-format on */

/*
@|Console ----------------------------------------------------------- */

#define CONSOLE_STD 0x1
#define CONSOLE_ARG 0x2
#define CONSOLE_EOA 0x3
#define CONSOLE_END 0x4

static unsigned int console_input(int c, unsigned int type)
{
	if (c == EOF)
		c = 0, type = CONSOLE_END;
	dev[0x12] = c, dev[0x17] = type;
	if (console_vector)
		uxn_eval(console_vector);
	return type != CONSOLE_END;
}

/* clang-format off */

//static Uint8 console_dei_childlive(void) { check_child(); return dev[CMD_LIVE]; }
//static Uint8 console_dei_childexit(void) { check_child(); return dev[CMD_EXIT]; }
static void console_deo_vector(void) { console_vector = peek2(&dev[0x10]); }
static void console_deo_stdout(void) { putchar(dev[0x18]); }
static void console_deo_stderr(void) { putchar(dev[0x19]); }
//static void console_deo_hb(void) { fprintf(stderr, "%02x", dev[0x1a]); }
//static void console_deo_lb(void) { fprintf(stderr, "%02x", dev[0x1b]); }

/* clang-format on */

/*
@|Screen ------------------------------------------------------------ */
#include "devices/screen.h"

void screen_update(void)
{
	screen_vector = screen_get_vector();
	if (screen_vector)
	{
		uxn_eval(screen_vector);
	}
	screen_run_update();
}

/*
@|Controller -------------------------------------------------------- */
#include "devices/controller.h"

/*
@|Mouse ------------------------------------------------------------- */
#include "devices/mouse.h"

/*
@|File -------------------------------------------------------------- */

#include "devices/file.h"

/* clang-format off */


//static void fileb_deo_stat(void) { poke2(&dev[0xb2], file_stat(1, peek2(&dev[0xb4]), rL2)); }
//static void fileb_deo_delete(void) { poke2(&dev[0xb2], file_delete(1)); }
//static void fileb_deo_name(void) { poke2(&dev[0xb2], file_init(1, peek2(&dev[0xb8]))); }
//static void fileb_deo_length(void) { rL2 = peek2(&dev[0xba]); }
//static void fileb_deo_read(void) { poke2(&dev[0xb2], file_read(1, peek2(&dev[0xbc]), rL2)); }
//static void fileb_deo_write(void) { poke2(&dev[0xb2], file_write(1, peek2(&dev[0xbe]), rL2, dev[0xb7])); }

/* clang-format on */

/*
@|Datetime ---------------------------------------------------------- */
#include "devices/datetime.h"

/* clang-format on */

/*
@|Core -------------------------------------------------------------- */

static const dei_handler dei_handlers[256] = {
	[0x04] = system_dei_wst,
	[0x05] = system_dei_rst,
	[0x28] = screen_dei_rxhb,
	[0x29] = screen_dei_rxlb,
	[0x2a] = screen_dei_ryhb,
	[0x2b] = screen_dei_rylb,
	[0x2c] = screen_dei_rahb,
	[0x2d] = screen_dei_ralb,
	[0xc0] = datetime_dei_yhb,
	[0xc1] = datetime_dei_ylb,
	[0xc2] = datetime_dei_mon,
	[0xc3] = datetime_dei_day,
	[0xc4] = datetime_dei_hou,
	[0xc5] = datetime_dei_min,
	[0xc6] = datetime_dei_sec,
	[0xc7] = datetime_dei_wday,
	[0xc8] = datetime_dei_ydayhb,
	[0xc9] = datetime_dei_ydaylb,
	[0xca] = datetime_dei_dst,
};

static const deo_handler deo_handlers[256] = {
	[0x04] = system_deo_wst,
	[0x05] = system_deo_rst,
	[0x08] = system_deo_colorize,
	[0x09] = system_deo_colorize,
	[0x0a] = system_deo_colorize,
	[0x0b] = system_deo_colorize,
	[0x0c] = system_deo_colorize,
	[0x0d] = system_deo_colorize,
	//[0x0e] = system_deo_print,
	[0x11] = console_deo_vector,
	[0x18] = console_deo_stdout,
	[0x19] = console_deo_stderr,
	//[0x1a] = console_deo_hb,
	//[0x1b] = console_deo_lb,
	[0x21] = screen_deo_vector,
	[0x23] = screen_deo_width,
	[0x25] = screen_deo_height,
	[0x26] = screen_deo_auto,
	[0x28] = screen_deo_x,
	[0x29] = screen_deo_x,
	[0x2a] = screen_deo_y,
	[0x2b] = screen_deo_y,
	[0x2c] = screen_deo_addr,
	[0x2d] = screen_deo_addr,
	[0x2e] = screen_deo_pixel,
	[0x2f] = screen_deo_sprite,
	[0x81] = controller_deo_vector,
	[0x91] = mouse_deo_vector,
	[0xa5] = filea_deo_stat,
	[0xa6] = filea_deo_delete,
	[0xa9] = filea_deo_name,
	[0xab] = filea_deo_length,
	[0xad] = filea_deo_read,
	[0xaf] = filea_deo_write};
//[0xb5] = fileb_deo_stat,
//[0xb6] = fileb_deo_delete,
//[0xb9] = fileb_deo_name,
//[0xbb] = fileb_deo_length,
//[0xbd] = fileb_deo_read,
//[0xbf] = fileb_deo_write};

static inline Uint8 emu_dei(const Uint8 port)
{
	dei_handler h = dei_handlers[port];
	return h ? h() : dev[port];
}

static inline void emu_deo(const Uint8 port, const Uint8 value)
{
	deo_handler h = deo_handlers[port];
	dev[port] = value;
	if (h)
		h();
}

/* clang-format off */

#define OPC(opc, A, B) {\
	case 0x00|opc: {const Uint8 d=0,r=0;A B} break;\
	case 0x20|opc: {const Uint8 d=1,r=0;A B} break;\
	case 0x40|opc: {const Uint8 d=0,r=1;A B} break;\
	case 0x60|opc: {const Uint8 d=1,r=1;A B} break;\
	case 0x80|opc: {const Uint8 d=0,r=0,k=ptr[0];A ptr[0]=k;B} break;\
	case 0xa0|opc: {const Uint8 d=1,r=0,k=ptr[0];A ptr[0]=k;B} break;\
	case 0xc0|opc: {const Uint8 d=0,r=1,k=ptr[1];A ptr[1]=k;B} break;\
	case 0xe0|opc: {const Uint8 d=1,r=1,k=ptr[1];A ptr[1]=k;B} break;}
#define DEC(m) stk[m][--ptr[m]]
#define INC(m) stk[m][ptr[m]++]
#define IMM a = ram[pc++] << 8, a |= ram[pc++];
#define MOV pc = d ? (Uint16)a : pc + (Sint8)a;
#define POx(o,m) o = DEC(r); if(m) o |= DEC(r) << 8;
#define PUx(i,m,s) if(m) c = (i), INC(s) = c >> 8, INC(s) = c; else INC(s) = i;
#define GOT(o) if(d) o[1] = DEC(r); o[0] = DEC(r);
#define PUT(i,s) INC(s) = i[0]; if(d) INC(s) = i[1];
#define DEO(o,v) emu_deo(o, v[0]); if(d) emu_deo(o + 1, v[1]);
#define DEI(i,v) v[0] = emu_dei(i); if(d) v[1] = emu_dei(i + 1); PUT(v,r)
#define POK(o,v,m) ram[o] = v[0]; if(d) ram[(o + 1) & m] = v[1];
#define PEK(i,v,m) v[0] = ram[i]; if(d) v[1] = ram[(i + 1) & m]; PUT(v,r)

unsigned int __not_in_flash_func(uxn_eval)(Uint16 pc)
{
	unsigned int a, b, c, x[2], y[2], z[2];
	while(1)
	{
		switch(ram[pc++]) 
		{
		/* BRK */ case 0x00: return 1;
		/* JCI */ case 0x20: if(DEC(0)) { IMM pc += a; } else pc += 2; break;
		/* JMI */ case 0x40: IMM pc += a; break;
		/* JSI */ case 0x60: IMM PUx(pc, 1, 1) pc += a; break;
		/* LI2 */ case 0xa0: INC(0) = ram[pc++]; /* fall-through */
		/* LIT */ case 0x80: INC(0) = ram[pc++]; break;
		/* L2r */ case 0xe0: INC(1) = ram[pc++]; /* fall-through */
		/* LIr */ case 0xc0: INC(1) = ram[pc++]; break;
		/* INC */ OPC(0x01,POx(a,d),PUx(a + 1,d,r))
		/* POP */ OPC(0x02,ptr[r] -= 1 + d;,{})
		/* NIP */ OPC(0x03,GOT(x) ptr[r] -= 1 + d;,PUT(x,r))
		/* SWP */ OPC(0x04,GOT(x) GOT(y),PUT(x,r) PUT(y,r))
		/* ROT */ OPC(0x05,GOT(x) GOT(y) GOT(z),PUT(y,r) PUT(x,r) PUT(z,r))
		/* DUP */ OPC(0x06,GOT(x),PUT(x,r) PUT(x,r))
		/* OVR */ OPC(0x07,GOT(x) GOT(y),PUT(y,r) PUT(x,r) PUT(y,r))
		/* EQU */ OPC(0x08,POx(a,d) POx(b,d),PUx(b == a,0,r))
		/* NEQ */ OPC(0x09,POx(a,d) POx(b,d),PUx(b != a,0,r))
		/* GTH */ OPC(0x0a,POx(a,d) POx(b,d),PUx(b > a,0,r))
		/* LTH */ OPC(0x0b,POx(a,d) POx(b,d),PUx(b < a,0,r))
		/* JMP */ OPC(0x0c,POx(a,d),MOV)
		/* JCN */ OPC(0x0d,POx(a,d) POx(b,0),if(b) MOV)
		/* JSR */ OPC(0x0e,POx(a,d),PUx(pc,1,!r) MOV)
		/* STH */ OPC(0x0f,GOT(x),PUT(x,!r))
		/* LDZ */ OPC(0x10,POx(a,0),PEK(a, x, 0xff))
		/* STZ */ OPC(0x11,POx(a,0) GOT(y),POK(a, y, 0xff))
		/* LDR */ OPC(0x12,POx(a,0),PEK(pc + (Sint8)a, x, 0xffff))
		/* STR */ OPC(0x13,POx(a,0) GOT(y),POK(pc + (Sint8)a, y, 0xffff))
		/* LDA */ OPC(0x14,POx(a,1),PEK(a, x, 0xffff))
		/* STA */ OPC(0x15,POx(a,1) GOT(y),POK(a, y, 0xffff))
		/* DEI */ OPC(0x16,POx(a,0),DEI(a, x))
		/* DEO */ OPC(0x17,POx(a,0) GOT(y),DEO(a, y))
		/* ADD */ OPC(0x18,POx(a,d) POx(b,d),PUx(b + a, d,r))
		/* SUB */ OPC(0x19,POx(a,d) POx(b,d),PUx(b - a, d,r))
		/* MUL */ OPC(0x1a,POx(a,d) POx(b,d),PUx(b * a, d,r))
		/* DIV */ OPC(0x1b,POx(a,d) POx(b,d),PUx(a ? b / a : 0, d,r))
		/* AND */ OPC(0x1c,POx(a,d) POx(b,d),PUx(b & a, d,r))
		/* ORA */ OPC(0x1d,POx(a,d) POx(b,d),PUx(b | a, d,r))
		/* EOR */ OPC(0x1e,POx(a,d) POx(b,d),PUx(b ^ a, d,r))
		/* SFT */ OPC(0x1f,POx(a,0) POx(b,d),PUx(b >> (a & 0xf) << (a >> 4), d,r))
		} 
		instructionCount++;
	}
	return 0;
}

int uxn_init()
{
	initPS2Mouse(6);
	initPS2Keyboard(8);
}

int uxn_load_rom(char *filename)
{
	load_rom(ram, filename);
	screen_resize(WIDTH, HEIGHT);
}

