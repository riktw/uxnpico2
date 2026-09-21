#ifndef UXN_UXNTYPES_H
#define UXN_UXNTYPES_H

#define CLAMP(v, a, b)   \
    {                    \
        if (v < a)       \
            v = a;       \
        else if (v >= b) \
            v = b;       \
    }
#define TWOS(v) (v & 0x8000 ? (int)v - 0x10000 : (int)v)

typedef signed char Sint8;
typedef unsigned char Uint8;
typedef signed short Sint16;
typedef unsigned short Uint16;
typedef void (*deo_handler)(void);
typedef Uint8 (*dei_handler)(void);

static inline Uint16 peek2(const Uint8 *d) { return ((Uint16)d[0] << 8) | d[1]; }
static inline void poke2(Uint8 *d, Uint16 v) { d[0] = v >> 8, d[1] = v; }

#endif