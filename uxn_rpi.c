#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "pico/multicore.h"

#include "core1.h"
#include "devices/screen.h"
#include "devices/datetime.h"

#include "uxn/uxn11.h"

#include "pico/audio_i2s.h"
#include <math.h>

static int screenUpdated;

// This probably happens on core1 context, look at switching to core0
void screen_vsync()
{
    multicore_doorbell_set_other_core(screenUpdated);
}

void core0_doorbell_irq()
{
    if (multicore_doorbell_is_set_current_core(screenUpdated))
    {
        multicore_doorbell_clear_current_core(screenUpdated);
    }

    screen_update();
}

#define SINE_WAVE_TABLE_LEN 4096
#define SAMPLES_PER_BUFFER 256

static int16_t sine_wave_table_right[SINE_WAVE_TABLE_LEN];
static int16_t sine_wave_table_left[SINE_WAVE_TABLE_LEN];
struct audio_buffer_pool *ap = 0;

static void run()
{
    uint32_t step = 0x200000;
    uint32_t pos = 0;
    uint32_t pos_max = 0x10000 * SINE_WAVE_TABLE_LEN;
    uint vol = 16;

    while (1)
    {
        struct audio_buffer *buffer = take_audio_buffer(ap, true);
        int16_t *samples = (int16_t *)buffer->buffer->bytes;
        for (uint i = 0; i < buffer->max_sample_count; i++)
        {
            samples[i * 2] = (vol * sine_wave_table_left[pos >> 16u]) >> 8u;
            samples[(i * 2) + 1] = (vol * sine_wave_table_right[pos >> 16u]) >> 8u;
            pos += step;
            if (pos >= pos_max)
                pos -= pos_max;
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);
        datetime_update();
    }
}

/*
    When pc board arrives:
    - Sound
    - USB for keeb/mouse
    - Game port
*/

struct audio_buffer_pool *init_audio()
{

    static audio_format_t audio_format = {
        .format = AUDIO_BUFFER_FORMAT_PCM_S16,
        .sample_freq = 48000,
        .channel_count = 2,
    };

    static struct audio_buffer_format producer_format = {
        .format = &audio_format,
        .sample_stride = 4};

    struct audio_buffer_pool *producer_pool = audio_new_producer_pool(&producer_format, 3,
                                                                      SAMPLES_PER_BUFFER); // todo correct size
    bool __unused ok;
    const struct audio_format *output_format;

    struct audio_i2s_config config = {
        .data_pin = 30,
        .clock_pin_base = 32,
        .dma_channel = 8,
        .pio_sm = 2,
    };

    output_format = audio_i2s_setup(&audio_format, &config);
    if (!output_format)
    {
        panic("PicoAudio: Unable to open audio device.\n");
    }

    ok = audio_i2s_connect(producer_pool);
    assert(ok);
    audio_i2s_set_enabled(true);

    return producer_pool;
}

int main()
{
    // Set system clock to 252 MHz for HSTX timing
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    set_sys_clock_khz(252000, true);

    for (int i = 0; i < SINE_WAVE_TABLE_LEN; i++)
    {
        sine_wave_table_right[i] = 32767 * cosf(i * 2 * (float)(M_PI / SINE_WAVE_TABLE_LEN));
        sine_wave_table_left[i] = 32767 * cosf(i * 2 * (float)(M_PI / SINE_WAVE_TABLE_LEN) * 2);
    }
    ap = init_audio();

    uint32_t irq = multicore_doorbell_irq_num(screenUpdated);
    irq_set_exclusive_handler(irq, core0_doorbell_irq);
    irq_set_priority(irq, 0xF0);
    irq_set_enabled(irq, true);

    uxn_init();

    uxn_load_rom("potato.rom");

    core1_init(screen_vsync);

    if (!uxn_eval(PAGE_PROGRAM))
    {
        printf("Init Failed\n");
    }

    run();

    while (true)
    {
        sleep_ms(1000);
    }
}
