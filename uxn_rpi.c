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

static void run()
{
    static int counter = 0;
    static bool led = 1;

    while (1)
    {
        sleep_ms(500);
        datetime_update();
    }
}

/*
    When pc board arrives:
    - Second storage
    - Sound
    - USB for keeb/mouse
    - Game port
*/
int main()
{
    // Set system clock to 252 MHz for HSTX timing
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    set_sys_clock_khz(252000, true);

    uint32_t irq = multicore_doorbell_irq_num(screenUpdated);
    irq_set_exclusive_handler(irq, core0_doorbell_irq);
    irq_set_priority(irq, 0xF0);
    irq_set_enabled(irq, true);

    uxn_init();

    uxn_load_rom("catclock.rom");

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
