/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/
#ifndef UXN_DEVICES_AUDIO_H
#define UXN_DEVICES_AUDIO_H

typedef signed int Sint32;
typedef unsigned int Uint32;

#include "../uxntypes.h"

#define SAMPLE_FREQUENCY 44100
#define SAMPLES_PER_BUFFER 512
#define POLYPHONY 1

void audio_init();
Uint8 audio_get_vu(int instance);
Uint16 audio_get_position(int instance);
int audio_render(int instance, Sint16 *sample, Sint16 *end);
void audio_start(int instance);
void audio_callback();
void audio_finished_handler(int instance);

Uint8 audio_dei_positionl(int instance);
Uint8 audio_dei_positionh(int instance);
Uint8 audio_dei_output(int instance);

void audio_deo_vector(int instance);
void audio_deo_pitch(int instance);

Uint8 audio0_dei_positionl(void);
Uint8 audio0_dei_positionh(void);
Uint8 audio0_dei_output(void);
Uint8 audio1_dei_positionl(void);
Uint8 audio1_dei_positionh(void);
Uint8 audio1_dei_output(void);
Uint8 audio2_dei_positionl(void);
Uint8 audio2_dei_positionh(void);
Uint8 audio2_dei_output(void);
Uint8 audio3_dei_positionl(void);
Uint8 audio3_dei_positionh(void);
Uint8 audio3_dei_output(void);

void audio0_deo_vector(void);
void audio0_deo_pitch(void);
void audio1_deo_vector(void);
void audio1_deo_pitch(void);
void audio2_deo_vector(void);
void audio2_deo_pitch(void);
void audio3_deo_vector(void);
void audio3_deo_pitch(void);

#endif