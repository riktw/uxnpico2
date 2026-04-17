/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#ifndef UXN_DEVICES_DATETIME_H
#define UXN_DEVICES_DATETIME_H

#include "../uxntypes.h"

void SecondPasssed();
Uint8 datetime_dei_yhb(void);
Uint8 datetime_dei_ylb(void);
Uint8 datetime_dei_mon(void);
Uint8 datetime_dei_day(void);
Uint8 datetime_dei_hou(void);
Uint8 datetime_dei_min(void);
Uint8 datetime_dei_sec(void);
Uint8 datetime_dei_wday(void);
Uint8 datetime_dei_ydayhb(void);
Uint8 datetime_dei_ydaylb(void);
Uint8 datetime_dei_dst(void);

#endif