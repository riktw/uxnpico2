#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "uxn11.h"

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

static Uint8 ram[0x10000], dev[0x100], stk[2][0x100], ptr[2];
static int console_vector, screen_vector, controller_vector, mouse_vector;
static int rX, rY, rA, rMX, rMY, rMA, rML, rDX, rDY, rL1, rL2;

/* clang-format off */

#define BANKS 0x10
#define BANKS_CAP BANKS * 0x10000
#define WIDTH (64 * 8)
#define HEIGHT (45 * 8)

#define CLAMP(v, a, b) { if(v < a) v = a; else if(v >= b) v = b; }
#define TWOS(v) (v & 0x8000 ? (int)v - 0x10000 : (int)v)

static inline Uint16 peek2(const Uint8 *d) { return ((Uint16)d[0] << 8) | d[1]; }
static inline void poke2(Uint8 *d, Uint16 v) { d[0] = v >> 8, d[1] = v; }

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
	return 0;//ram;// && system_load(rom_path);
}

static unsigned int system_reboot(const unsigned int soft)
{
	memset(dev, 0, 0x100);
	memset(stk[0], 0, 0x100);
	memset(stk[1], 0, 0x100);
	if(soft)
		memset(ram + 0x100, 0, 0xff00);
	else
		memset(ram, 0, 0x10000);
	ptr[0] = ptr[1] = 0;
	console_vector = screen_vector = controller_vector = mouse_vector = 0;
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
	if(c == EOF) c = 0, type = CONSOLE_END;
	dev[0x12] = c, dev[0x17] = type;
	if(console_vector) uxn_eval(console_vector);
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
#include "../core1.h"
static int screen_zoom = 1, screen_reqsize, screen_reqdraw;
static int screen_width, screen_height, screen_wmar2, screen_hmar2;
static int screen_x1, screen_y1, screen_x2, screen_y2;

static const Uint8 blend_lut[16][2][4] = {
	{{0, 0, 1, 2}, {0, 0, 4, 8}},
	{{0, 1, 2, 3}, {0, 4, 8, 12}},
	{{0, 2, 3, 1}, {0, 8, 12, 4}},
	{{0, 3, 1, 2}, {0, 12, 4, 8}},
	{{1, 0, 1, 2}, {4, 0, 4, 8}},
	{{1, 1, 2, 3}, {4, 4, 8, 12}},
	{{1, 2, 3, 1}, {4, 8, 12, 4}},
	{{1, 3, 1, 2}, {4, 12, 4, 8}},
	{{2, 0, 1, 2}, {8, 0, 4, 8}},
	{{2, 1, 2, 3}, {8, 4, 8, 12}},
	{{2, 2, 3, 1}, {8, 8, 12, 4}},
	{{2, 3, 1, 2}, {8, 12, 4, 8}},
	{{3, 0, 1, 2}, {12, 0, 4, 8}},
	{{3, 1, 2, 3}, {12, 4, 8, 12}},
	{{3, 2, 3, 1}, {12, 8, 12, 4}},
	{{3, 3, 1, 2}, {12, 12, 4, 8}}};

static const Uint8 alpha_lut[16] = {0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0};

static void screen_change(const int x1, const int y1, const int x2, const int y2)
{
	screen_x1 = x1 < screen_x1 ? x1 : screen_x1;
	screen_y1 = y1 < screen_y1 ? y1 : screen_y1;
	screen_x2 = x2 > screen_x2 ? x2 : screen_x2;
	screen_y2 = y2 > screen_y2 ? y2 : screen_y2;
}

static void system_deo_colorize(void)
{
    uint8_t colors[4];
    for(size_t i = 0; i < 4; ++i) 
    {
        Uint8 r = ((dev[0x8 + i / 2] >> (!(i % 2) << 2)) & 0x0f) * 0x11;
        Uint8 g = ((dev[0xa + i / 2] >> (!(i % 2) << 2)) & 0x0f) * 0x11;
        Uint8 b = ((dev[0xc + i / 2] >> (!(i % 2) << 2)) & 0x0f) * 0x11;

        colors[i] = ((r & 0xE0) + ((g & 0xE0) >> 3) + ((b & 0xC0) >> 6));
    }
    core1_setPalette(colors);
    core1_setBackground(0);
}

static void screen_resize(int width, int height)
{
	if(width != screen_width || height != screen_height) {
		screen_width = width, screen_wmar2 = width + 0x10;
		poke2(&dev[0x22], screen_width);
		screen_height = height, screen_hmar2 = height + 0x10;
		poke2(&dev[0x24], screen_height);
		screen_reqsize = screen_reqdraw = 1;
	}
}

/*
static void screen_redraw(void)
{
	if(screen_zoom == 1) {
		const int colmar = (screen_y1 + 8) * screen_wmar2 + (screen_x1 + 8);
		const int row_width = screen_x2 - screen_x1;
		const int src_stride = screen_wmar2 - row_width;
		const int dst_stride = screen_width - row_width;
		int y, *dst_ptr = &screen_pixels[screen_y1 * screen_width + screen_x1];
		Uint8 *src_ptr = &screen_layers[colmar];
		for(y = screen_y1; y < screen_y2; y++) {
			int *dst_end = dst_ptr + row_width;
			while(dst_ptr < dst_end)
				*dst_ptr++ = screen_palette[*src_ptr++];
			dst_ptr += dst_stride;
			src_ptr += src_stride;
		}
	} else {
		int x, y, i, k, l;
		const int stride = screen_width * screen_zoom;
		for(y = screen_y1; y < screen_y2; y++) {
			const int ys = y * screen_zoom;
			for(x = screen_x1, i = (x + 8) + (y + 8) * screen_wmar2; x < screen_x2; x++, i++) {
				const int c = screen_palette[screen_layers[i]];
				for(k = 0; k < screen_zoom; k++) {
					const int oo = (ys + k) * stride + x * screen_zoom;
					for(l = 0; l < screen_zoom; l++)
						screen_pixels[oo + l] = c;
				}
			}
		}
	}
	emu_redraw();
	screen_x1 = screen_y1 = screen_x2 = screen_y2 = screen_reqdraw = 0;
}
*/

void screen_update(void)
{
	if(screen_vector)
		uxn_eval(screen_vector);
	if(screen_reqsize) {

	}
	if(screen_reqdraw) {
		screen_x1 = screen_y1 = 0;
		screen_x2 = screen_width;
		screen_y2 = screen_height;
		core1_updateScreen();
	} else if(screen_x2 > screen_x1 && screen_y2 > screen_y1) {
		CLAMP(screen_x1, 0, screen_width);
		CLAMP(screen_y1, 0, screen_height);
		CLAMP(screen_x2, 0, screen_width);
		CLAMP(screen_y2, 0, screen_height);
		core1_updateScreen();
	}
}

static void screen_deo_pixel(void)
{
	const int ctrl = dev[0x2e];
	const int hi = ctrl & 0x40;
	const Uint8 mask = hi ? 0x3 : 0xc;
	const Uint8 color = hi ? ((ctrl & 0x3) << 2) : (ctrl & 0x3);
	// fill mode 
	if(ctrl & 0x80) {
		int px;
		const int x1 = (ctrl & 0x10) ? 8 : rX + 8;
		const int x2 = (ctrl & 0x10) ? rX : screen_width;
		const int y1 = (ctrl & 0x20) ? 8 : rY + 8;
		const int y2 = (ctrl & 0x20) ? rY : screen_height;
		const int hor = (x2 + 8) - x1;
		const int ver = (y2 + 8) - y1;
		//TODO: fill mode
		/*
		Uint8 *row = &screen_layers[y1 * screen_wmar2 + x1];
		Uint8 *end = row + ver * screen_wmar2;
		for(; row < end; row += screen_wmar2) {
			Uint8 *dst = row;
			for(px = 0; px < hor; px++)
				dst[px] = (dst[px] & mask) | color;
		}
		*/
		screen_reqdraw = 1;
	}
	// pixel mode 
	else {
		const Uint16 x = rX, y = rY;
		if(x < screen_width && y < screen_height) {
			//Uint8 *dst = &screen_layers[(y + 8) * screen_wmar2 + (x + 8)];
			//*dst = (*dst & mask) | color;  //TODO: apply mask
			core1_setPixel(x+8, y+8, mask, color);
			screen_reqdraw = 1;
		}
		if(rMX) rX++;
		if(rMY) rY++;
	}
}

static void screen_deo_sprite(void)
{
	int i, j, x = rX, y = rY;
	const int ctrl = dev[0x2f];
	const int flipx = ctrl & 0x10, dx = flipx ? -rDY : rDY;
	const int flipy = ctrl & 0x20, dy = flipy ? -rDX : rDX;
	const int row_start = flipx ? 0 : 7, row_delta = flipx ? 1 : -1;
	const int col_start = flipy ? 7 : 0, col_delta = flipy ? -1 : 1;
	const int layer = ctrl & 0x40, layer_mask = layer ? 0x3 : 0xc;
	const int mode_2bpp = ctrl >> 7, addr_2bpp = mode_2bpp ? rMA << 2 : rMA << 1;
	const int blend = ctrl & 0xf;
	const Uint8 opaque = alpha_lut[blend], *table = blend_lut[blend][layer >> 6];
	for(i = 0; i <= rML; i++, x += dx, y += dy, rA += addr_2bpp) {
		const Uint16 x0 = x + 8, y0 = y + 8;
		if(x0 + 8 >= screen_wmar2 || y0 + 8 >= screen_hmar2) continue;
		const Uint8 *col = &ram[rA + col_start];
		for(j = 0; j < 8; j++, col += col_delta) {
			const int ch1 = *col, ch2 = mode_2bpp ? col[8] : 0;
			for(int k = 0, row = row_start; k < 8; k++, row += row_delta) {
				const int color = ((ch1 >> row) & 1) | (((ch2 >> row) & 1) << 1);
				if(opaque || color)
				{
					core1_setPixel(x0+k, y0+j, layer_mask, table[color]);
				}
			}
		}
	}
	if(!screen_reqdraw) {
		int x1 = flipx ? x : rX, x2 = flipx ? rX : x;
		int y1 = flipy ? y : rY, y2 = flipy ? rY : y;
		screen_change(x1 - 8, y1 - 8, x2 + 8, y2 + 8);
	}
	if(rMX) rX += flipx ? -rDX : rDX;
	if(rMY) rY += flipy ? -rDY : rDY;
}

/* clang-format off */

static Uint8 screen_dei_rxhb(void) { return rX >> 8; }
static Uint8 screen_dei_rxlb(void) { return rX; }
static Uint8 screen_dei_ryhb(void) { return rY >> 8; }
static Uint8 screen_dei_rylb(void) { return rY; }
static Uint8 screen_dei_rahb(void) { return rA >> 8; }
static Uint8 screen_dei_ralb(void) { return rA; }
static void screen_deo_vector(void) { screen_vector = peek2(&dev[0x20]); }
static void screen_deo_width(void) { /*screen_resize(peek2(&dev[0x22]) & 0xfff, screen_height & 0xfff);*/ }
static void screen_deo_height(void) { /*screen_resize(screen_width & 0xfff, peek2(&dev[0x24]) & 0xfff);*/ }
static void screen_deo_auto(void) { rMX = dev[0x26] & 0x1, rMY = dev[0x26] & 0x2, rMA = dev[0x26] & 0x4, rML = dev[0x26] >> 4, rDX = rMX << 3, rDY = rMY << 2; }
static void screen_deo_x(void) { rX = (dev[0x28] << 8) | dev[0x29], rX = TWOS(rX); }
static void screen_deo_y(void) { rY = (dev[0x2a] << 8) | dev[0x2b], rY = TWOS(rY); }
static void screen_deo_addr(void) { rA = (dev[0x2c] << 8) | dev[0x2d]; }

/* clang-format on */

/*
@|Controller -------------------------------------------------------- */
/*
static void controller_down(Uint8 mask)
{
	if(mask) {
		dev[0x82] |= mask;
		if(controller_vector) uxn_eval(controller_vector);
	}
}

static void controller_up(Uint8 mask)
{
	if(mask) {
		dev[0x82] &= (~mask);
		if(controller_vector) uxn_eval(controller_vector);
	}
}

static void controller_key(Uint8 key)
{
	if(key) {
		dev[0x83] = key;
		if(controller_vector) uxn_eval(controller_vector);
		dev[0x83] = 0;
	}
}

void
controller_deo_vector(void)
{
	controller_vector = peek2(&dev[0x80]);
}
*/
/*
@|Mouse ------------------------------------------------------------- */
/*
static void mouse_down(Uint8 mask)
{
	dev[0x96] |= mask;
	if(mouse_vector) uxn_eval(mouse_vector);
}

static void mouse_up(Uint8 mask)
{
	dev[0x96] &= (~mask);
	if(mouse_vector) uxn_eval(mouse_vector);
}

static void mouse_pos(Uint16 x, Uint16 y)
{
	poke2(&dev[0x92], x);
	poke2(&dev[0x94], y);
	if(mouse_vector) uxn_eval(mouse_vector);
}

static void mouse_scroll(Uint16 x, Uint16 y)
{
	poke2(&dev[0x9a], x);
	poke2(&dev[0x9c], -y);
	if(mouse_vector) uxn_eval(mouse_vector);
	poke2(&dev[0x9a], 0);
	poke2(&dev[0x9c], 0);
}

void mouse_deo_vector(void)
{
	mouse_vector = peek2(&dev[0x90]);
}
*/
/*
@|File -------------------------------------------------------------- */

#include "devices/file.h"

static UxnFile ufs[2];

/* clang-format off */

static void filea_deo_stat(void)   { poke2(&dev[0xa2], file_stat(&ufs[0], &ram[peek2(&dev[0xa4])], rL1)); }   
static void filea_deo_delete(void) { poke2(&dev[0xa2], file_delete(&ufs[0])); }
static void filea_deo_name(void)   { poke2(&dev[0xa2], file_init(&ufs[0], (char *)&ram[peek2(&dev[0xa8])])); }
static void filea_deo_length(void) { rL1 = peek2(&dev[0xaa]); }
static void filea_deo_read(void)   { poke2(&dev[0xa2], file_read(&ufs[0], &ram[peek2(&dev[0xac])], rL1)); }
static void filea_deo_write(void)  { poke2(&dev[0xa2], file_write(&ufs[0], &ram[peek2(&dev[0xae])], rL1, dev[0xa7])); }
//static void fileb_deo_stat(void) { poke2(&dev[0xb2], file_stat(1, peek2(&dev[0xb4]), rL2)); }
//static void fileb_deo_delete(void) { poke2(&dev[0xb2], file_delete(1)); }
//static void fileb_deo_name(void) { poke2(&dev[0xb2], file_init(1, peek2(&dev[0xb8]))); }
//static void fileb_deo_length(void) { rL2 = peek2(&dev[0xba]); }
//static void fileb_deo_read(void) { poke2(&dev[0xb2], file_read(1, peek2(&dev[0xbc]), rL2)); }
//static void fileb_deo_write(void) { poke2(&dev[0xb2], file_write(1, peek2(&dev[0xbe]), rL2, dev[0xb7])); }

/* clang-format on */

/*
@|Datetime ---------------------------------------------------------- */
#include <time.h>
time_t seconds = 0;
struct tm *datetime_t;

void SecondPasssed()
{
	seconds++;
}

void datetime_update()
{
	struct tm *t = localtime(&seconds);
	datetime_t = t;
}

/* clang-format off */

static Uint8 datetime_dei_yhb(void) { datetime_update(); return (datetime_t->tm_year + 1900) >> 8; }
static Uint8 datetime_dei_ylb(void) { datetime_update(); return (datetime_t->tm_year + 1900); }
static Uint8 datetime_dei_mon(void) { datetime_update(); return datetime_t->tm_mon; }
static Uint8 datetime_dei_day(void) { datetime_update(); return datetime_t->tm_mday; }
static Uint8 datetime_dei_hou(void) { datetime_update(); return datetime_t->tm_hour; }
static Uint8 datetime_dei_min(void) { datetime_update(); return datetime_t->tm_min; }
static Uint8 datetime_dei_sec(void) { datetime_update(); return datetime_t->tm_sec; }
static Uint8 datetime_dei_wday(void) { datetime_update(); return datetime_t->tm_wday; }
static Uint8 datetime_dei_ydayhb(void) { datetime_update(); return datetime_t->tm_yday >> 8; }
static Uint8 datetime_dei_ydaylb(void) { datetime_update(); return datetime_t->tm_yday; }
static Uint8 datetime_dei_dst(void) { datetime_update(); return datetime_t->tm_isdst; }

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
	//[0x81] = controller_deo_vector,
	//[0x91] = mouse_deo_vector,
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
	if(h) h();
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

unsigned int __not_in_flash("") uxn_eval(Uint16 pc)
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
	}
	return 0;
}

int uxn_load_rom(char *filename)
{
	load_rom(ram, filename);
	screen_resize(WIDTH, HEIGHT);
}

/* clang-format on */

/*
static Display *emu_dpy;
static Window emu_win;
static XImage *emu_img;
static int emu_x11;
static int emu_timer;

#define CONINBUFSIZE 256

void emu_resize(void)
{
	const int width = screen_width * screen_zoom;
	const int height = screen_height * screen_zoom;
	if(emu_img) {
		emu_img->data = NULL;
		XDestroyImage(emu_img);
	}
	emu_img = XCreateImage(emu_dpy, DefaultVisual(emu_dpy, 0), DefaultDepth(emu_dpy, DefaultScreen(emu_dpy)), ZPixmap, 0, (char *)screen_pixels, width, height, 32, 0);
	XResizeWindow(emu_dpy, emu_win, width, height);
}

void emu_redraw(void)
{
	const int ax = screen_x1 * screen_zoom;
	const int ay = screen_y1 * screen_zoom;
	const int aw = screen_x2 * screen_zoom - ax;
	const int ah = screen_y2 * screen_zoom - ay;
	XPutImage(emu_dpy, emu_win, DefaultGC(emu_dpy, 0), emu_img, ax, ay, ax, ay, aw, ah);
}

static void emu_restart(unsigned int soft)
{
	system_reboot(soft);
	screen_width = screen_height = 0;
	screen_resize(WIDTH, HEIGHT);
	uxn_eval(0x100);
}

static Uint8 get_button(KeySym sym)
{
	switch(sym) {
	case XK_Up: return 0x10;
	case XK_Down: return 0x20;
	case XK_Left: return 0x40;
	case XK_Right: return 0x80;
	case XK_Control_L: return 0x01;
	case XK_Alt_R:
	case XK_Alt_L: return 0x02;
	case XK_Shift_L: return 0x04;
	case XK_Home: return 0x08;
	case XK_Meta_L: return 0x02;
	}
	return 0x00;
}

static void display_center(void)
{
	Screen *screen = DefaultScreenOfDisplay(emu_dpy);
	const int w = screen_width * screen_zoom;
	const int h = screen_height * screen_zoom;
	const int x = screen->width / 2 - w / 2;
	const int y = screen->height / 2 - h / 2;
	emu_win = XCreateSimpleWindow(emu_dpy, RootWindow(emu_dpy, 0), x, y, w, h, 1, 0, 0);
}

static void display_hidecursor(void)
{
	XColor black = {0};
	static const char empty[1] = {0};
	Pixmap bitmap = XCreateBitmapFromData(emu_dpy, emu_win, empty, 1, 1);
	Cursor blank = XCreatePixmapCursor(emu_dpy, bitmap, bitmap, &black, &black, 0, 0);
	XDefineCursor(emu_dpy, emu_win, blank);
	XFreeCursor(emu_dpy, blank);
	XFreePixmap(emu_dpy, bitmap);
}

static void display_floating(void)
{
	Atom wmDelete = XInternAtom(emu_dpy, "WM_DELETE_WINDOW", True);
	Atom wmType = XInternAtom(emu_dpy, "_NET_WM_WINDOW_TYPE", False);
	Atom wmDialog = XInternAtom(emu_dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	XClassHint class = {"uxn11", "Uxn"};
	XSetWMProtocols(emu_dpy, emu_win, &wmDelete, 1);
	XChangeProperty(emu_dpy, emu_win, wmType, 4, 32, PropModeReplace, (Uint8 *)&wmDialog, 1);
	XSelectInput(emu_dpy, emu_win, ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ExposureMask | KeyPressMask | KeyReleaseMask | LeaveWindowMask);
	XStoreName(emu_dpy, emu_win, "uxn11");
	XSetClassHint(emu_dpy, emu_win, &class);
}

static void emu_event(void)
{
	char buf[7];
	XEvent ev;
	XNextEvent(emu_dpy, &ev);
	switch(ev.type) {
	case Expose:
		screen_reqdraw = 1;
		break;
	case ClientMessage:
		dev[0x0f] = 0x80;
		break;
	case KeyPress: {
		KeySym sym;
		XLookupString((XKeyPressedEvent *)&ev, buf, 7, &sym, 0);
		switch(sym) {
		case XK_F1: screen_zoom = (screen_zoom % 3) + 1, screen_reqsize = screen_reqdraw = 1; break;
		case XK_F2: emu_deo(0xe, 0x1); break;
		case XK_F4: console_close(), emu_restart(0); break;
		case XK_F5: console_close(), emu_restart(1); break;
		}
		controller_down(get_button(sym));
		if(sym == XK_Tab)
			buf[0] = 0x9;
		controller_key(sym < 0x80 ? sym : (Uint8)buf[0]);
	} break;
	case KeyRelease: {
		KeySym sym;
		XLookupString((XKeyPressedEvent *)&ev, buf, 7, &sym, 0);
		controller_up(get_button(sym));
	} break;
	case ButtonPress: {
		XButtonPressedEvent *e = (XButtonPressedEvent *)&ev;
		switch(e->button) {
		case 4: mouse_scroll(0, 1); break;
		case 5: mouse_scroll(0, -1); break;
		case 6: mouse_scroll(1, 0); break;
		case 7: mouse_scroll(-1, 0); break;
		default: mouse_down(0x1 << (e->button - 1));
		}
	} break;
	case ButtonRelease: {
		XButtonPressedEvent *e = (XButtonPressedEvent *)&ev;
		mouse_up(0x1 << (e->button - 1));
	} break;
	case LeaveNotify:
	case MotionNotify: {
		XMotionEvent *e = (XMotionEvent *)&ev;
		mouse_pos(e->x / screen_zoom, e->y / screen_zoom);
	} break;
	}
}

static int emu_init(void)
{
	static const struct itimerspec screen_tspec = {
		{0, 16666666},
		{0, 16666666}};
	emu_dpy = XOpenDisplay(NULL);
	if(!emu_dpy)
		return !fprintf(stderr, "Display: failed\n");
	emu_x11 = ConnectionNumber(emu_dpy);
	display_center();
	display_hidecursor();
	display_floating();
	emu_timer = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
	if(emu_timer < 0)
		return !fprintf(stderr, "timerfd_create: failed\n");
	timerfd_settime(emu_timer, 0, &screen_tspec, NULL);
	XMapWindow(emu_dpy, emu_win);
	XFlush(emu_dpy);
	return 1;
}

static void emu_run(void)
{
	int has_input, has_eof, stdin_active = 1;
	char expirations[8], coninp[CONINBUFSIZE];
	struct pollfd fds[3];
	fds[0].fd = emu_x11, fds[0].events = POLLIN;
	fds[1].fd = emu_timer, fds[1].events = POLLIN;
	fds[2].fd = STDIN_FILENO, fds[2].events = POLLIN | POLLHUP;
	while(!dev[0x0f]) {
		// only poll stdin while it's still active 
		if(poll(fds, stdin_active ? 3 : 2, -1) <= 0)
			continue;
		while(XPending(emu_dpy))
			emu_event();
		if(fds[1].revents & POLLIN) {
			read(emu_timer, expirations, sizeof expirations);
			screen_update();
		}
		has_input = stdin_active && (fds[2].revents & POLLIN);
		has_eof = stdin_active && (fds[2].revents & POLLHUP);
		if(has_input || has_eof) {
			int i, n = read(STDIN_FILENO, coninp, CONINBUFSIZE - 1);
			if(n > 0) {
				for(i = 0; i < n; i++)
					console_input(coninp[i], CONSOLE_STD);
			} else {
				console_input('\n', CONSOLE_END);
				stdin_active = 0;
			}
		}
		datetime_busy = 0;
	}
	if(emu_timer >= 0)
		close(emu_timer);
	if(emu_win)
		XDestroyWindow(emu_dpy, emu_win);
	if(emu_dpy)
		XCloseDisplay(emu_dpy);
	console_close();
}
*/