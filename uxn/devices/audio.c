
#include "audio.h"

#include "../uxn11.h"
#include "pico/audio_i2s.h"
#include "string.h"
/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#define NOTE_PERIOD (SAMPLE_FREQUENCY * 0x4000 / 11025)
#define ADSR_STEP (SAMPLE_FREQUENCY / (0xf * 4)) // pi pico audio buffer and sdl audio buffer is a factor 4 difference

extern Uint8 ram[BANKS_CAP];
extern Uint8 dev[0x100];
static int audio_vector = 0;

typedef struct
{
	Uint8 *addr;
	Uint32 count, advance, period, age, a, d, s, r;
	Uint16 i, len;
	Sint8 volume[2];
	Uint8 pitch, repeat;
} UxnAudio;

/* clang-format off */

static Uint32 advances[12] = {
	0x80000, 0x879c8, 0x8facd, 0x9837f, 0xa1451, 0xaadc1,
	0xb504f, 0xbfc88, 0xcb2ff, 0xd7450, 0xe411f, 0xf1a1c
};


static UxnAudio uxn_audio[POLYPHONY];
static bool audioRunning = false;

struct audio_buffer_pool *ap = 0;

/* clang-format on */

static Sint32 envelope(UxnAudio *c, Uint32 age)
{
	if (!c->r)
		return 0x0888;
	if (age < c->a)
		return 0x0888 * age / c->a;
	if (age < c->d)
		return 0x0444 * (2 * c->d - c->a - age) / (c->d - c->a);
	if (age < c->s)
		return 0x0444;
	if (age < c->r)
		return 0x0444 * (c->r - age) / (c->r - c->s);
	c->advance = 0;
	return 0x0000;
}

void audio_init()
{
	static audio_format_t audio_format = {
		.format = AUDIO_BUFFER_FORMAT_PCM_S16,
		.sample_freq = SAMPLE_FREQUENCY,
		.channel_count = 2,
	};

	static struct audio_buffer_format producer_format = {
		.format = &audio_format,
		.sample_stride = 4};

	ap = audio_new_producer_pool(&producer_format, 2, SAMPLES_PER_BUFFER);
	const struct audio_format *output_format;

	struct audio_i2s_config config = {
		.data_pin = 30,
		.clock_pin_base = 32,
		.dma_channel = 8,
		.pio_sm = 1,
	};

	output_format = audio_i2s_setup(&audio_format, &config);
	if (!output_format)
	{
		panic("PicoAudio: Unable to open audio device.\n");
	}

	audio_i2s_connect(ap);
	audio_i2s_set_enabled(true);
}

void audio_start(int instance)
{
	UxnAudio *c = &uxn_audio[instance];
	Uint16 addr, adsr;
	Uint8 pitch;
	adsr = peek2(&dev[0x38]);
	c->len = peek2(&dev[0x3a]);
	addr = peek2(&dev[0x3c]);
	if (c->len > 0x10000 - addr)
		c->len = 0x10000 - addr;
	c->addr = &ram[addr];
	c->volume[0] = dev[0x3e] >> 4;
	c->volume[1] = dev[0x3e] & 0xf;
	c->repeat = !(dev[0x3f] & 0x80);
	pitch = dev[0x3f] & 0x7f;
	if (pitch < 108 && c->len)
		c->advance = advances[pitch % 12] >> (8 - pitch / 12);
	else
	{
		c->advance = 0;
		return;
	}
	c->a = ADSR_STEP * (adsr >> 12);
	c->d = ADSR_STEP * (adsr >> 8 & 0xf) + c->a;
	c->s = ADSR_STEP * (adsr >> 4 & 0xf) + c->d;
	c->r = ADSR_STEP * (adsr >> 0 & 0xf) + c->s;
	c->age = 0;
	c->i = 0;
	if (c->len <= 0x100) /* single cycle mode */
		c->period = NOTE_PERIOD * 337 / 2 / c->len;
	else /* sample repeat mode */
		c->period = NOTE_PERIOD;
}

void audio_callback()
{
	if (audioRunning)
	{
		int running = 0;
		struct audio_buffer *buffer = take_audio_buffer(ap, true);
		int16_t *samples = (int16_t *)buffer->buffer->bytes;
		memset(samples, 0, buffer->buffer->size);

		for (int instance = 0; instance < POLYPHONY; instance++)
		{
			running += audio_render(instance, samples, samples + (SAMPLES_PER_BUFFER * 2));
		}

		buffer->sample_count = buffer->max_sample_count;
		give_audio_buffer(ap, buffer);
		if (!running)
		{
			audioRunning = false;
		}
	}
}

int audio_render(int instance, Sint16 *sample, Sint16 *end)
{
	UxnAudio *c = &uxn_audio[instance];
	Sint32 s;
	if (!c->advance || !c->period)
		return 0;
	while (sample < end)
	{
		c->count += c->advance;
		c->i += c->count / c->period;
		c->count %= c->period;
		if (c->i >= c->len)
		{
			if (!c->repeat)
			{
				c->advance = 0;
				break;
			}
			c->i %= c->len;
		}
		s = (Sint8)(c->addr[c->i] + 0x80) * envelope(c, c->age++);
		*sample++ += s * c->volume[0] / 0x180;
		*sample++ += s * c->volume[1] / 0x180;
	}
	if (!c->advance)
		audio_finished_handler(instance);
	return 1;
}

void audio_finished_handler(int instance)
{
	if (audio_vector)
		uxn_eval(audio_vector);
}

Uint8 audio_get_vu(int instance)
{
	UxnAudio *c = &uxn_audio[instance];
	int i;
	Sint32 sum[2] = {0, 0};
	if (!c->advance || !c->period)
		return 0;
	for (i = 0; i < 2; i++)
	{
		if (!c->volume[i])
			continue;
		sum[i] = 1 + envelope(c, c->age) * c->volume[i] / 0x800;
		if (sum[i] > 0xf)
			sum[i] = 0xf;
	}
	return (sum[0] << 4) | sum[1];
}

Uint16 audio_get_position(int instance)
{
	UxnAudio *c = &uxn_audio[instance];
	return c->i;
}

Uint8 audio_dei_positionl(int instance)
{
	return audio_get_position(instance) & 0xff;
}

Uint8 audio_dei_positionh(int instance)
{
	return (audio_get_position(instance) >> 8) & 0xff;
}

Uint8 audio_dei_output(int instance)
{
	return audio_get_vu(instance);
}

void audio_deo_vector(int instance)
{
	audio_vector = peek2(&dev[0x30]);
}

void audio_deo_pitch(int instance)
{
	audio_start(instance);
	audioRunning = true;
}

Uint8 audio0_dei_positionl(void) { audio_dei_positionl(0); };
Uint8 audio0_dei_positionh(void) { audio_dei_positionh(0); };
Uint8 audio0_dei_output(void) { audio_dei_output(0); };
Uint8 audio1_dei_positionl(void) { audio_dei_positionl(1); };
Uint8 audio1_dei_positionh(void) { audio_dei_positionh(1); };
Uint8 audio1_dei_output(void) { audio_dei_output(1); };
Uint8 audio2_dei_positionl(void) { audio_dei_positionl(2); };
Uint8 audio2_dei_positionh(void) { audio_dei_positionh(2); };
Uint8 audio2_dei_output(void) { audio_dei_output(2); };
Uint8 audio3_dei_positionl(void) { audio_dei_positionl(3); };
Uint8 audio3_dei_positionh(void) { audio_dei_positionh(3); };
Uint8 audio3_dei_output(void) { audio_dei_output(3); };

void audio0_deo_vector(void) { audio_deo_vector(0); };
void audio0_deo_pitch(void) { audio_deo_pitch(0); };
void audio1_deo_vector(void) { audio_deo_vector(1); };
void audio1_deo_pitch(void) { audio_deo_pitch(1); };
void audio2_deo_vector(void) { audio_deo_vector(2); };
void audio2_deo_pitch(void) { audio_deo_pitch(2); };
void audio3_deo_vector(void) { audio_deo_vector(3); };
void audio3_deo_pitch(void) { audio_deo_pitch(3); };
