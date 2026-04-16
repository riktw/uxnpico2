#include <stdint-gcc.h>
#include <stdlib.h>

#include "screen.h"
#include "../core1.h"

static int screen_vector;
static int rX, rY, rA, rMX, rMY, rMA, rML, rDX, rDY, rL1, rL2;
extern Uint8 ram[0x10000];
extern Uint8 dev[0x100];

static int screen_zoom = 1, screen_reqsize, screen_reqdraw;
static int screen_width, screen_height, screen_wmar2, screen_hmar2;
static int screen_width_offset, screen_height_offset;
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

void system_deo_colorize(void)
{
	uint16_t colors[4];
	for (size_t i = 0; i < 4; ++i)
	{
		uint8_t r = ((dev[0x8 + i / 2] >> (!(i % 2) << 2)) & 0x0f) * 0x11;
		uint8_t g = ((dev[0xa + i / 2] >> (!(i % 2) << 2)) & 0x0f) * 0x11;
		uint8_t b = ((dev[0xc + i / 2] >> (!(i % 2) << 2)) & 0x0f) * 0x11;

		// RGB565
		colors[i] = (((r & 0xF8) << 8) + ((g & 0xFC) << 3) + ((b & 0xF8) >> 3));
	}
	core1_setPalette(colors);
	core1_setBackground(0);
}

void screen_resize(int width, int height)
{
	if (width != screen_width || height != screen_height)
	{
		screen_width = width, screen_wmar2 = width + 0x10;
		poke2(&dev[0x22], screen_width);
		screen_height = height, screen_hmar2 = height + 0x10;
		poke2(&dev[0x24], screen_height);
		core1_setResolution(screen_hmar2, screen_wmar2);
		screen_reqsize = screen_reqdraw = 1;

		screen_width_offset = (UXN_WIDTH - screen_width) / 2;
		screen_height_offset = (UXN_HEIGHT - screen_height) / 2;
	}
}

int screen_get_height()
{
	return screen_height;
}

int screen_get_width()
{
	return screen_width;
}

int screen_get_vector(void)
{
	return screen_vector;
}

void screen_run_update(void)
{
	if (screen_reqsize)
	{
	}
	if (screen_reqdraw)
	{
		screen_x1 = screen_y1 = 0;
		screen_x2 = screen_width;
		screen_y2 = screen_height;
	}
	else if (screen_x2 > screen_x1 && screen_y2 > screen_y1)
	{
		CLAMP(screen_x1, 0, screen_width);
		CLAMP(screen_y1, 0, screen_height);
		CLAMP(screen_x2, 0, screen_width);
		CLAMP(screen_y2, 0, screen_height);
	}
}
#include <stdio.h>
void screen_deo_pixel(void)
{
	const int ctrl = dev[0x2e];
	const int hi = ctrl & 0x40;
	const Uint8 mask = hi ? 0x3 : 0xc;
	const Uint8 color = hi ? ((ctrl & 0x3) << 2) : (ctrl & 0x3);
	// fill mode
	if (ctrl & 0x80)
	{
		int px;
		const int x1 = (ctrl & 0x10) ? 8 : rX + 8;
		const int x2 = (ctrl & 0x10) ? rX : screen_width;
		const int y1 = (ctrl & 0x20) ? 8 : rY + 8;
		const int y2 = (ctrl & 0x20) ? rY : screen_height;

		core1_fillPixels(x1 + screen_width_offset, x2 + screen_width_offset + UXN_PADDING, y1 + screen_height_offset, y2 + screen_height_offset + UXN_PADDING, mask, color);

		screen_reqdraw = 1;
	}
	// pixel mode
	else
	{
		const Uint16 x = rX, y = rY;
		if (x < screen_width && y < screen_height)
		{
			core1_setPixel(x + (UXN_PADDING / 2) + screen_width_offset, y + (UXN_PADDING / 2) + screen_height_offset, mask, color);
			screen_reqdraw = 1;
		}
		if (rMX)
			rX++;
		if (rMY)
			rY++;
	}
}

void screen_deo_sprite(void)
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
	for (i = 0; i <= rML; i++, x += dx, y += dy, rA += addr_2bpp)
	{
		const Uint16 x0 = x + 8, y0 = y + 8;
		if (x0 + 8 >= screen_wmar2 || y0 + 8 >= screen_hmar2)
			continue;
		const Uint8 *col = &ram[rA + col_start];
		for (j = 0; j < 8; j++, col += col_delta)
		{
			const int ch1 = *col, ch2 = mode_2bpp ? col[8] : 0;
			for (int k = 0, row = row_start; k < 8; k++, row += row_delta)
			{
				const int color = ((ch1 >> row) & 1) | (((ch2 >> row) & 1) << 1);
				if (opaque || color)
				{
					core1_setPixel(x0 + k + +screen_width_offset, y0 + j + screen_height_offset, layer_mask, table[color]);
				}
			}
		}
	}
	if (!screen_reqdraw)
	{
		int x1 = flipx ? x : rX, x2 = flipx ? rX : x;
		int y1 = flipy ? y : rY, y2 = flipy ? rY : y;
		screen_change(x1 - 8, y1 - 8, x2 + 8, y2 + 8);
	}
	if (rMX)
		rX += flipx ? -rDX : rDX;
	if (rMY)
		rY += flipy ? -rDY : rDY;
}

void screen_init()
{
	screen_vector = 0;
	rX = rY = rA = rMX = rMY = rMA = rML = rDX = rDY = rL1 = rL2 = 0;
}

Uint8 screen_dei_rxhb(void) { return rX >> 8; }
Uint8 screen_dei_rxlb(void) { return rX; }
Uint8 screen_dei_ryhb(void) { return rY >> 8; }
Uint8 screen_dei_rylb(void) { return rY; }
Uint8 screen_dei_rahb(void) { return rA >> 8; }
Uint8 screen_dei_ralb(void) { return rA; }
void screen_deo_vector(void) { screen_vector = peek2(&dev[0x20]); }
void screen_deo_width(void) { screen_resize(peek2(&dev[0x22]) & 0xfff, screen_height & 0xfff); }
void screen_deo_height(void) { screen_resize(screen_width & 0xfff, peek2(&dev[0x24]) & 0xfff); }
void screen_deo_auto(void) { rMX = dev[0x26] & 0x1, rMY = dev[0x26] & 0x2, rMA = dev[0x26] & 0x4, rML = dev[0x26] >> 4, rDX = rMX << 3, rDY = rMY << 2; }
void screen_deo_x(void) { rX = (dev[0x28] << 8) | dev[0x29], rX = TWOS(rX); }
void screen_deo_y(void) { rY = (dev[0x2a] << 8) | dev[0x2b], rY = TWOS(rY); }
void screen_deo_addr(void) { rA = (dev[0x2c] << 8) | dev[0x2d]; }