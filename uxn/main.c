#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <linux/input.h>

#include "shorthand.h"
#include "ppu.c"
#include "uxn/src/devices/system.c"
#include "uxn/src/devices/datetime.c"
#include "uxn/src/devices/file.c"
#include "uxn/src/uxn.c"

#define CLAMP(X, MIN, MAX) ((X) <= (MIN) ? (MIN) : (X) > (MAX) ? (MAX): (X))

static Uxn u;
static Device *devscreen;
static Device *devctrl;
static Device *devmouse;

typedef struct timespec Time;

void
halt(int stub) {
    (void)stub;
    set_tty(false);
    exit(EXIT_SUCCESS);
}

int
uxn_interrupt(void) {
    return 1;
}

Time
time_now(){
    struct timespec t;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);
    return t;
}

size_t
time_elapsed(Time since){
    struct timespec now = time_now();
    return (now.tv_sec - since.tv_sec) * 1e9 + (now.tv_nsec - since.tv_nsec);
}

typedef struct Mouse {
    s32 x;
    s32 y;
    u8 buttons;
    bool update;
} Mouse;

typedef struct Input {
    int kbd_fd;
    int mouse_fd;
    char map[KEY_MAX / 8 + 1];
    u8 controller;
    Mouse mouse;
} Input;

// NOTE: For event codes and input documentation:
// - https://www.kernel.org/doc/Documentation/input/event-codes.txt
// - /usr/include/linux/input.h
static Input in;

void
init_input(void) {
    memset(&in, 0, sizeof(in));
    in.kbd_fd = -1;
    in.mouse_fd = -1;

    in.kbd_fd = open(KBD_PATH, O_RDONLY | O_NONBLOCK);
    if (in.kbd_fd == -1) {
        // NOTE: Some applications may not require a keyboard so this is
        //       optional, but we are still displaying an error.
        fprintf(stderr, "error: couldn't open keyboard %s: %s.\n", KBD_PATH, strerror(errno));
    }

    in.mouse_fd = open(MOUSE_PATH, O_RDONLY | O_NONBLOCK);
    if (in.mouse_fd == -1) {
        // NOTE: Some applications may not require a mouse so this is
        //       optional, but we are still displaying an error.
        fprintf(stderr, "error: couldn't open mouse %s: %s.\n", MOUSE_PATH, strerror(errno));
    }
}

void
poll_keyboard(void) {
    if (in.kbd_fd == -1) {
        return;
    }

    // TODO: use read() instead to avoid updating the keyboard if no events have
    //       occurred. Similar to the mouse implementation.
    char map[KEY_MAX / 8 + 1];
    memset(map, 0, sizeof(map));
    ioctl(in.kbd_fd, EVIOCGKEY(sizeof(map)), map);
    for (size_t i = 0; i < sizeof(map); i++) {
        in.map[i] |= map[i];
    }
}

void
poll_mouse(void) {
    if (in.mouse_fd == -1) {
        return;
    }

    struct input_event mouse_event;
    if (read(in.mouse_fd, &mouse_event, sizeof(mouse_event)) != -1) {
        if (mouse_event.type == EV_REL) {
            if (mouse_event.code == REL_X) {
                in.mouse.x = CLAMP(
                    in.mouse.x + (s32)mouse_event.value, 0, (s32)screen_width);
            } else  if (mouse_event.code == REL_Y) {
                in.mouse.y = CLAMP(
                    in.mouse.y + (s32)mouse_event.value, 0, (s32)screen_height);
            }
        } else if (mouse_event.type == EV_ABS) {
            if (mouse_event.code == ABS_X) {
                in.mouse.x = CLAMP((s32)mouse_event.value / (s32)zoom, 0, (s32)screen_width);
            } else  if (mouse_event.code == ABS_Y) {
                in.mouse.y = CLAMP((s32)mouse_event.value / (s32)zoom, 0, (s32)screen_height);
            }
        } else if (mouse_event.type == EV_KEY) {
            switch (mouse_event.code) {
                case BTN_LEFT: {
                    if (mouse_event.value == 1) {
                        in.mouse.buttons |= 0x01;
                    } else {
                        in.mouse.buttons &= ~0x01;
                    }
                } break;
                case BTN_RIGHT: {
                    if (mouse_event.value == 1) {
                        in.mouse.buttons |= 0x10;
                    } else {
                        in.mouse.buttons &= ~0x10;
                    }
                } break;
                default: break;
            }
        }
        in.mouse.update = true;
    }
}

void
poll_input(void) {
    poll_keyboard();
    poll_mouse();
}

void
handle_keyboard(void) {
    // Find mod keys.
    bool shift_mod = false;
    // bool ctrl_mod = false;
    // bool alt_mod = false;
    // bool meta_mod = false;
    for (size_t i = 0; i < sizeof(in.map); i++) {
        for (size_t j = 0; j < 8; j++) {
            char key = in.map[i] & (1 << j);
            if (key) {
                char key_code = i * 8 + j;
                switch (key_code) {
                    case KEY_LEFTSHIFT:
                    case KEY_RIGHTSHIFT: { shift_mod = true; } break;
                    // case KEY_LEFTCTRL:
                    // case KEY_RIGHTCTRL: { ctrl_mod = true; } break;
                    // case KEY_LEFTALT:
                    // case KEY_RIGHTALT: { alt_mod = true; } break;
                    // case KEY_LEFTMETA:
                    // case KEY_RIGHTMETA: { meta_mod = true; } break;
                    default: break;
                }
            }
        }
    }

    // Handle normal keys.
    u8 controller_now = 0;
    for (size_t i = 0; i < sizeof(in.map); i++) {
        for (size_t j = 0; j < 8; j++) {
            char key = in.map[i] & (1 << j);
            if (key) {
                char key_code = i * 8 + j;
                // Normal keys.
                u8 rune = '\0';
                switch (key_code) {
                    case KEY_KP1:
                    case KEY_1: { rune = shift_mod ? '!' : '1'; } break;
                    case KEY_KP2:
                    case KEY_2: { rune = shift_mod ? '@' : '2'; } break;
                    case KEY_KP3:
                    case KEY_3: { rune = shift_mod ? '#' : '3'; } break;
                    case KEY_KP4:
                    case KEY_4: { rune = shift_mod ? '$' : '4'; } break;
                    case KEY_KP5:
                    case KEY_5: { rune = shift_mod ? '%' : '5'; } break;
                    case KEY_KP6:
                    case KEY_6: { rune = shift_mod ? '^' : '6'; } break;
                    case KEY_KP7:
                    case KEY_7: { rune = shift_mod ? '&' : '7'; } break;
                    case KEY_KP8:
                    case KEY_8: { rune = shift_mod ? '*' : '8'; } break;
                    case KEY_KP9:
                    case KEY_9: { rune = shift_mod ? '(' : '9'; } break;
                    case KEY_KP0:
                    case KEY_0: { rune = shift_mod ? ')' : '0'; } break;
                    case KEY_KPMINUS:
                    case KEY_MINUS: { rune = shift_mod ? '_' : '-'; } break;
                    case KEY_KPEQUAL:
                    case KEY_EQUAL: { rune = shift_mod ? '+' : '+'; } break;
                    case KEY_Q: { rune = shift_mod ? 'Q' : 'q'; } break;
                    case KEY_W: { rune = shift_mod ? 'W' : 'w'; } break;
                    case KEY_E: { rune = shift_mod ? 'E' : 'e'; } break;
                    case KEY_R: { rune = shift_mod ? 'T' : 't'; } break;
                    case KEY_T: { rune = shift_mod ? 'T' : 't'; } break;
                    case KEY_Y: { rune = shift_mod ? 'Y' : 'y'; } break;
                    case KEY_U: { rune = shift_mod ? 'U' : 'u'; } break;
                    case KEY_I: { rune = shift_mod ? 'I' : 'i'; } break;
                    case KEY_O: { rune = shift_mod ? 'O' : 'o'; } break;
                    case KEY_P: { rune = shift_mod ? 'P' : 'p'; } break;
                    case KEY_LEFTBRACE: { rune = shift_mod ? '{' : '['; } break;
                    case KEY_RIGHTBRACE: { rune = shift_mod ? '}' : ']'; } break;
                    case KEY_A: { rune = shift_mod ? 'A' : 'a'; } break;
                    case KEY_S: { rune = shift_mod ? 'S' : 's'; } break;
                    case KEY_D: { rune = shift_mod ? 'D' : 'd'; } break;
                    case KEY_F: { rune = shift_mod ? 'F' : 'f'; } break;
                    case KEY_G: { rune = shift_mod ? 'G' : 'g'; } break;
                    case KEY_H: { rune = shift_mod ? 'H' : 'h'; } break;
                    case KEY_J: { rune = shift_mod ? 'J' : 'j'; } break;
                    case KEY_K: { rune = shift_mod ? 'K' : 'k'; } break;
                    case KEY_L: { rune = shift_mod ? 'L' : 'l'; } break;
                    case KEY_SEMICOLON: { rune = shift_mod ? ':' : ';'; } break;
                    case KEY_APOSTROPHE: { rune = shift_mod ? '"' : '\''; } break;
                    case KEY_GRAVE: { rune = shift_mod ? '~' : '`'; } break;
                    case KEY_BACKSLASH: { rune = shift_mod ? '|' : '\\'; } break;
                    case KEY_Z: { rune = shift_mod ? 'Z' : 'z'; } break;
                    case KEY_X: { rune = shift_mod ? 'X' : 'x'; } break;
                    case KEY_C: { rune = shift_mod ? 'C' : 'c'; } break;
                    case KEY_V: { rune = shift_mod ? 'V' : 'v'; } break;
                    case KEY_B: { rune = shift_mod ? 'B' : 'b'; } break;
                    case KEY_N: { rune = shift_mod ? 'N' : 'n'; } break;
                    case KEY_M: { rune = shift_mod ? 'M' : 'm'; } break;
                    case KEY_COMMA: { rune = shift_mod ? '<' : ','; } break;
                    case KEY_DOT: { rune = shift_mod ? '>' : '.'; } break;
                    case KEY_KPSLASH:
                    case KEY_SLASH: { rune = shift_mod ? '?' : '/'; } break;
                    case KEY_KPASTERISK: { rune = '*'; } break;
                    case KEY_KPPLUS: { rune = '+'; } break;
                    case KEY_KPCOMMA: { rune = '.'; } break;
                    case KEY_SPACE: { rune = ' '; } break;
                    case KEY_TAB: { rune = '\t'; } break;
                    case KEY_ESC: { rune = 0x1b; } break;
                    case KEY_BACKSPACE: { rune = 0x08; } break;
                    case KEY_ENTER:
                    case KEY_KPENTER: { rune = 0x0d; } break;
                    default: break;
                }
                if (rune) {
                    devctrl->dat[3] = rune;
                    uxn_eval(&u, GETVECTOR(devctrl));
                    devctrl->dat[3] = 0;
                    continue;
                }

                // Special keys.
                switch (key_code) {
                    case KEY_LEFTCTRL: { rune = 0x01; } break;
                    case KEY_LEFTALT: { rune = 0x02; } break;
                    case KEY_LEFTSHIFT: { rune = 0x04; } break;
                    case KEY_HOME: { rune = 0x08; } break;
                    case KEY_UP: { rune = 0x10; } break;
                    case KEY_DOWN: { rune = 0x20; } break;
                    case KEY_LEFT: { rune = 0x40; } break;
                    case KEY_RIGHT: { rune = 0x80; } break;
                    default: break;
                }

                if (rune) {
                    controller_now |= rune;
                    continue;
                }
            }
        }
    }

    if (controller_now != in.controller) {
        devctrl->dat[2] = controller_now;
        uxn_eval(&u, GETVECTOR(devctrl));
        in.controller = controller_now;
    }

    // Reset input state.
    devctrl->dat[3] = 0;
    memset(in.map, 0, sizeof(in.map));
}

void
mouse_pos(Device *d, u16 x, u16 y) {
    DEVPOKE16(0x2, x);
    DEVPOKE16(0x4, y);
    uxn_eval(d->u, GETVECTOR(d));
}

void
handle_mouse(void) {
    if (in.mouse.update) {
        // Handle mouse keys.
        devmouse->dat[6] = in.mouse.buttons;
        if(in.mouse.buttons == 0x10 && (devmouse->dat[6] & 0x01)) {
            devmouse->dat[7] = 0x01;
        }
        if(in.mouse.buttons == 0x01 && (devmouse->dat[6] & 0x10)) {
            devmouse->dat[7] = 0x10;
        }

        // Handle mouse location.
        mouse_pos(devmouse, in.mouse.x, in.mouse.y);
        uxn_eval(&u, GETVECTOR(devmouse));
        in.mouse.update = false;
    }
}

void
handle_input(void) {
    handle_keyboard();
    handle_mouse();
}

static u8
nil_dei(Device *d, u8 port) {
    return d->dat[port];
}

static void
nil_deo(Device *d, u8 port) {
    (void)d;
    (void)port;
}

static void
console_deo(Device *d, u8 port) {
    FILE *fd = port == 0x8 ? stdout : port == 0x9 ? stderr : 0;
    if(fd) {
        fputc(d->dat[port], fd);
        fflush(fd);
    }
}

u8
screen_dei(Device *d, u8 port) {
    switch(port) {
        case 0x2: return screen_width >> 8;
        case 0x3: return screen_width;
        case 0x4: return screen_height >> 8;
        case 0x5: return screen_height;
        default: return d->dat[port];
    }
}

void
screen_deo(Device *d, u8 port) {
    switch(port) {
        // case 0x3:
        //     if(!FIXED_SIZE) {
        //         Uint16 w;
        //         DEVPEEK16(w, 0x2);
        //         screen_resize(&uxn_screen, clamp(w, 1, 1024), uxn_screen.height);
        //     }
        //     break;
        // case 0x5:
        //     if(!FIXED_SIZE) {
        //         Uint16 h;
        //         DEVPEEK16(h, 0x4);
        //         screen_resize(&uxn_screen, uxn_screen.width, clamp(h, 1, 1024));
        //     }
        //     break;
        case 0xe: {
            u16 x, y;
            u8 layer = d->dat[0xe] & 0x40;
            DEVPEEK16(x, 0x8);
            DEVPEEK16(y, 0xa);
            ppu_pixel(layer ? pixels_fg : pixels_bg, x, y, d->dat[0xe] & 0x3);
            if(d->dat[0x6] & 0x01) DEVPOKE16(0x8, x + 1); /* auto x+1 */
            if(d->dat[0x6] & 0x02) DEVPOKE16(0xa, y + 1); /* auto y+1 */
        } break;
        case 0xf: {
            u16 x, y, dx, dy, addr;
            u8 twobpp = !!(d->dat[0xf] & 0x80);
            DEVPEEK16(x, 0x8);
            DEVPEEK16(y, 0xa);
            DEVPEEK16(addr, 0xc);
            u8 n = d->dat[0x6] >> 4;
            dx = (d->dat[0x6] & 0x01) << 3;
            dy = (d->dat[0x6] & 0x02) << 2;
            if(addr > 0x10000 - ((n + 1) << (3 + twobpp))) {
                return;
            }
            u8 *layer = (d->dat[0xf] & 0x40) ? pixels_fg : pixels_bg;
            u8 color = d->dat[0xf] & 0xf;
            u8 flipx = d->dat[0xf] & 0x10;
            u8 flipy = d->dat[0xf] & 0x20;
            for(size_t i = 0; i <= n; i++) {
                u8 *sprite = &d->u->ram[addr];
                if (twobpp) {
                    ppu_2bpp(layer, x + dy * i, y + dx * i, sprite, color, flipx, flipy);
                } else {
                    ppu_1bpp(layer, x + dy * i, y + dx * i, sprite, color, flipx, flipy);
                }
                addr += (d->dat[0x6] & 0x04) << (1 + twobpp);
            }
            DEVPOKE16(0xc, addr);   /* auto addr+length */
            DEVPOKE16(0x8, x + dx); /* auto x+8 */
            DEVPOKE16(0xa, y + dy); /* auto y+8 */
        } break;
	}
    reqdraw = 1;
}

static void
screen_palette(Device *d) {
    for(size_t i = 0; i < 4; ++i) {
        u8 r = ((d->dat[0x8 + i / 2] >> (!(i % 2) << 2)) & 0x0f) * 0x11;
        u8 g = ((d->dat[0xa + i / 2] >> (!(i % 2) << 2)) & 0x0f) * 0x11;
        u8 b = ((d->dat[0xc + i / 2] >> (!(i % 2) << 2)) & 0x0f) * 0x11;

        if (rgb_order) {
            palette[i] = (b << 16) | (g << 8) | r;
        } else {
            palette[i] = (r << 16) | (g << 8) | b;
        }
    }
    for(size_t i = 4; i < 16; ++i) {
        palette[i] = palette[i / 4];
    }

    // Redraw the screen if we change the color palette.
    reqdraw = 1;
    redraw_screen();
}

void
system_deo_special(Device *d, u8 port) {
    if(port > 0x7 && port < 0xe) {
        screen_palette(d);
    }
}

void
load_uxn_rom(char *file_name) {
    FILE *file = fopen(file_name, "r");
    if (!file) {
        fprintf(stderr, "error: couldn't open file: %s\n", file_name);
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    size_t rom_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *uxn_rom = malloc(rom_size);
    fread(uxn_rom, 1, rom_size, file);
    memcpy(u.ram + PAGE_PROGRAM, uxn_rom, rom_size);

    fclose(file);
    free(uxn_rom);
}

void
init_uxn(Uxn *u, char *file_name) {
    signal(SIGINT, halt);

    // Setup UXN memory.
	uxn_boot(u, calloc(0x10000, sizeof(u8)));

    // Copy rom to VM.
    load_uxn_rom(file_name);

    // Initialize framebuffer.
    ppu_init();

    // Initialize keybord.
    init_input();

    // Prepare devices.
    /* system   */ uxn_port(u, 0x0, system_dei, system_deo);
    /* console  */ uxn_port(u, 0x1, nil_dei, console_deo);
    /* screen   */ devscreen = uxn_port(u, 0x2, screen_dei, screen_deo);
    // TODO:
    // /* audio0   */ devaudio0 = uxn_port(u, 0x3, audio_dei, audio_deo);
    // /* audio1   */ uxn_port(u, 0x4, audio_dei, audio_deo);
    // /* audio2   */ uxn_port(u, 0x5, audio_dei, audio_deo);
    // /* audio3   */ uxn_port(u, 0x6, audio_dei, audio_deo);
    /* audio0   */ uxn_port(u, 0x3, nil_dei, nil_deo);
    /* audio1   */ uxn_port(u, 0x4, nil_dei, nil_deo);
    /* audio2   */ uxn_port(u, 0x5, nil_dei, nil_deo);
    /* audio3   */ uxn_port(u, 0x6, nil_dei, nil_deo);
    /* unused   */ uxn_port(u, 0x7, nil_dei, nil_deo);
    /* control  */ devctrl = uxn_port(u, 0x8, nil_dei, nil_deo);
    /* mouse    */ devmouse = uxn_port(u, 0x9, nil_dei, nil_deo);
    /* file0    */ uxn_port(u, 0xa, file_dei, file_deo);
    /* file1    */ uxn_port(u, 0xb, file_dei, file_deo);
    /* datetime */ uxn_port(u, 0xc, datetime_dei, nil_deo);
    /* unused   */ uxn_port(u, 0xd, nil_dei, nil_deo);
    /* unused   */ uxn_port(u, 0xe, nil_dei, nil_deo);
    /* unused   */ uxn_port(u, 0xf, nil_dei, nil_deo);
    uxn_eval(u, PAGE_PROGRAM);
}

int
main(int argc, char *argv[]) {
    if (argc <= 1) {
        // TODO: If no rom is given, embed the uxn compiler rom.
        fprintf(stderr, "error: no rom selected\n");
        exit(EXIT_FAILURE);
    }
    init_uxn(&u, argv[1]);

    // Main loop.
    Time frame_time = time_now();
    while (true) {
        poll_input();
        size_t elapsed = time_elapsed(frame_time);
        if (elapsed >= 16666666) {
            handle_input();

            // Echo input to standard output.
            uxn_eval(&u, GETVECTOR(devscreen));

            // Blit ppu.pixels to the framebuffer.
            blit_framebuffer();
            frame_time = time_now();
        }
    }

    return 0;
}
