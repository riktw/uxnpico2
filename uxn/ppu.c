#include <fcntl.h>
#include <string.h>
#include <termios.h>

#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/kd.h>
#include <sys/mman.h>

#include "ppu.h"

/*
   Copyright (c) 2021 Devine Lu Linvega
   Copyright (c) 2021 Andrew Alderwick
   Copyright (c) 2021 Bad Diode

   Permission to use, copy, modify, and distribute this software for any
   purpose with or without fee is hereby granted, provided that the above
   copyright notice and this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
   WITH REGARD TO THIS SOFTWARE.
   */

// Parameters.
static size_t zoom = 4;

static size_t screen_width = 0;
static size_t screen_height = 0;
static size_t bpp = 0;
static int fb_file = 0;

static u8 *framebuffer = 0;

static u32 palette[16];
static u8 *pixels_fg;
static u8 *pixels_bg;
static u8 *dirty_lines;
static u8 reqdraw = 0;
// TODO: Probably should consider this
static u32 rgb_order = 0;

static u8 blending[5][16] = {
    {0, 0, 0, 0, 1, 0, 1, 1, 2, 2, 0, 2, 3, 3, 3, 0},
    {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3},
    {1, 2, 3, 1, 1, 2, 3, 1, 1, 2, 3, 1, 1, 2, 3, 1},
    {2, 3, 1, 2, 2, 3, 1, 2, 2, 3, 1, 2, 2, 3, 1, 2},
    {1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0}};

u16
rgb565(u32 rgba) {
    u16 r = (rgba >> 16  & 0xFF);
    u16 g = (rgba >> 8   & 0xFF);
    u16 b = (rgba >> 0   & 0xFF);
    r = r >> 3;
    g = g >> 2;
    b = b >> 3;
    return (r << 11) | (g << 5) | b;
}

void
ppu_pixel(u8 *layer, u16 x, u16 y, u8 color) {
    if (x >= screen_width || y >= screen_height) {
        return;
    }
    Uint32 i = x + y *screen_width;
    if(color != layer[i]) {
        layer[i] = color;
    }
    dirty_lines[y] |= 1;
}

void
ppu_1bpp(u8 *layer, u16 x, u16 y, u8 *sprite, u8 color, u8 flipx, u8 flipy) {
    u16 v, h;
    for(v = 0; v < 8; v++)
        for(h = 0; h < 8; h++) {
            u8 ch1 = (sprite[v] >> (7 - h)) & 0x1;
            if(ch1 || blending[4][color])
                ppu_pixel(layer,
                        x + (flipx ? 7 - h : h),
                        y + (flipy ? 7 - v : v),
                        blending[ch1][color]);
        }
}

void
ppu_2bpp(u8 *layer, u16 x, u16 y, u8 *sprite, u8 color, u8 flipx, u8 flipy) {
    u16 v, h;
    for(v = 0; v < 8; v++)
        for(h = 0; h < 8; h++) {
            u8 ch1 = ((sprite[v] >> (7 - h)) & 0x1);
            u8 ch2 = ((sprite[v + 8] >> (7 - h)) & 0x1);
            u8 ch = ch1 + ch2 * 2;
            if(ch || blending[4][color])
                ppu_pixel(layer,
                        x + (flipx ? 7 - h : h),
                        y + (flipy ? 7 - v : v),
                        blending[ch][color]);
        }
}

void
redraw_screen(void) {
    for (size_t j = 0; j < screen_height; j++) {
        dirty_lines[j] = 1;
    }
}

static struct termios prev_t;

void
set_tty(bool graphics) {
    // Disable TTY echo and set graphics mode.
    int tty = open("/dev/tty0", O_RDWR);
    if (!tty) {
        fprintf(stderr,"error: couldn't open tty\n");
        exit(EXIT_FAILURE);
    }
    if (graphics) {
        if (ioctl(tty, KDSETMODE, KD_GRAPHICS)) {
            fprintf(stderr,"error: setting graphics mode failed\n");
            exit(EXIT_FAILURE);
        }
        struct termios t;
        if (tcgetattr(STDIN_FILENO, &t)) {
            fprintf(stderr, "error: couldn't disable terminal echo\n");
            exit(EXIT_FAILURE);
        }
        prev_t = t;
        t.c_lflag &= ~((tcflag_t) ECHO);
        if (tcsetattr(STDIN_FILENO, TCSANOW, &t)) {
            fprintf(stderr, "error: couldn't disable terminal echo\n");
            exit(EXIT_FAILURE);
        }
    } else {
        if (ioctl(tty, KDSETMODE, KD_TEXT)) {
            fprintf(stderr,"error: setting text mode failed\n");
            exit(EXIT_FAILURE);
        }
        if (tcsetattr(STDIN_FILENO, TCSANOW, &prev_t)) {
            fprintf(stderr, "error: couldn't restore terminal attributes\n");
            exit(EXIT_FAILURE);
        }
    }
    close(tty);
}

int
ppu_init(void) {
    // Open frambuffer and get the size.
    // TODO: Pass as macro or input parameter
    fb_file = open("/dev/fb0", O_RDWR);
    if (fb_file <= 0) {
        fprintf(stderr, "error: couldn't open the framebuffer\n");
        exit(EXIT_FAILURE);
    }

    struct fb_var_screeninfo info;
    if (ioctl(fb_file, FBIOGET_VSCREENINFO, &info) != 0) {
        fprintf(stderr, "error: couldn't get the framebuffer size\n");
        exit(EXIT_FAILURE);
    }

    // Mmap the framebuffer to a buffer object.
    screen_width = info.xres;
    screen_height = info.yres;
    bpp = info.bits_per_pixel;
    size_t len = bpp / 8 * screen_width * screen_height;
    framebuffer = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fb_file, 0);
    if (framebuffer == MAP_FAILED) {
        fprintf(stderr, "error: couldn't mmap the framebuffer\n");
        exit(EXIT_FAILURE);
    }
    screen_width /= zoom;
    screen_height /= zoom;

    // Allocate intermediate buffers.
    pixels_fg = malloc(screen_width * screen_height);
    pixels_bg = malloc(screen_width * screen_height);
    dirty_lines = malloc(screen_height);
    if (pixels_fg == NULL || pixels_bg == NULL || dirty_lines == NULL) {
        fprintf(stderr, "error: couldn't allocate memory for the ppu\n");
        exit(EXIT_FAILURE);
    }

    // Prepare TTY for graphics mode.
    set_tty(true);

    // Initialize default palette.
    palette[0] = 0x444444;
    palette[1] = 0xffffff;
    palette[2] = 0x7777ff;
    palette[3] = 0xff7777;

    // Clear pixel buffer memory.
    memset(pixels_fg, 0, screen_width * screen_height);
    memset(pixels_bg, 0, screen_width * screen_height);
    memset(dirty_lines, 1, screen_height);

    // Clear framebuffer.
    memset(framebuffer, 0xFF, len);

    // Make sure we perform an initial screen drawing.
    reqdraw = 1;
    redraw_screen();
    close(fb_file);

    return 1;
}

void
blit_framebuffer(void) {
    if (reqdraw == 0) {
        return;
    }
    // TODO: add some parameter to account for zoom? for example we may want to
    // have an internal resolution smaller than the screen!
    for (size_t j = 0; j < screen_height; j++) {
        if (dirty_lines[j] != 0) {
            for (size_t i = 0; i < screen_width; i++) {
                size_t idx = i + j * screen_width;
                if (bpp == 16) {
                    u16 *p = (u16*)framebuffer;
                    for (size_t zz = 0; zz < zoom; zz++) {
                        size_t fb_idx = (i * zoom + j * screen_width * zoom * zoom + zz * screen_width * zoom);
                        for (size_t z = 0; z < zoom; z++) {
                            p[fb_idx + z] = rgb565(palette[pixels_fg[idx] << 2 | pixels_bg[idx]]);
                        }
                    }
                } else if (bpp == 32) {
                    u32 *p = (u32*)framebuffer;
                    for (size_t zz = 0; zz < zoom; zz++) {
                        size_t fb_idx = (i * zoom + j * screen_width * zoom * zoom + zz * screen_width * zoom);
                        for (size_t z = 0; z < zoom; z++) {
                            p[fb_idx + z] = palette[pixels_fg[idx] << 2 | pixels_bg[idx]];
                        }
                    }
                } else {
                    fprintf(stderr, "error: wrong bits per pixel (expected 16 or 32)\n");
                    exit(EXIT_FAILURE);
                }
            }
        }
        dirty_lines[j] = 0;
    }
    reqdraw = 0;
}
