#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"

#include "core1.h"
#include "devices/screen.h"

#include "uxn/uxn11.h"

volatile bool screenUpdated = false;

void screen_vsync()
{
    screenUpdated = true;
}

static void run()
{
    static int counter = 0;

    while (1)
    {
        if (screenUpdated)
        {
            screenUpdated = false;
            screen_update();
            counter++;
            if (counter >= 60)
            {
                SecondPasssed();
                counter = 0;
            }
        }
    }
}

/*
    TODO:
    - optimize some

    When pc board arrives:
    - rtc
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

    uxn_init();

    uxn_load_rom("boot.rom");

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
