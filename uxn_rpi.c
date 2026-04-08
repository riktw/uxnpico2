#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"

#include "core1.h"
#include "ps2_keyboard.h"

#include "uxn/uxn11.h"

static void run()
{
    static int counter = 0;
    // int c = getchar_timeout_us(16667);
    // if (c >= 0)
    //     console_input(u, (Uint8)c);
    while (1)
    {
        sleep_us(16667);
        uint16_t keyboardEvent = ps2GetEvent();

        // TODO: clean up keyboard code here and in ps_keyboard.c
        while (keyboardEvent != 0 && keyboardEvent != KEYBOARD_EVENT_IDLE)
        {
            uint8_t control = keyboardEvent >> 8;
            uint8_t ascii = keyboardEvent & 0x00FF;
            uint8_t controllerCode = ps2GetController(control, ascii);
            if (CheckBit(control, RELEASE_BIT))
            {
                controller_up(controllerCode);
            }
            else if (!CheckBit(control, EXTENDED_BIT))
            {
                if (ascii)
                {
                    controller_key(ascii);
                }
                controller_down(controllerCode);
            }
            keyboardEvent = ps2GetEvent();
        }
        screen_update();
        counter++;
        if (counter >= 60)
        {
            SecondPasssed();
            counter = 0;
        }
    }
}

int main()
{
    // Set system clock to 252 MHz for HSTX timing
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    set_sys_clock_khz(252000, true);

    stdio_init_all();

    uxn_load_rom("perifs.rom");

    core1_init();

    initPS2();

    if (!uxn_eval(PAGE_PROGRAM))
    {
        printf("Init Failed\n");
    }

    run();

    // unmount();

    while (true)
    {
        sleep_ms(1000);
    }
}
