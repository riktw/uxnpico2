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
#include "devices/audio.h"
#include "devices/mouse.h"
#include "devices/controller.h"

#include "uxn/uxn11.h"

#include "pio_usb.h"
#include "tusb.h"

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
    checkNesController();
    screen_update();
}

static void run()
{
    while (1)
    {
#ifdef ENABLE_PCBOARD
        audio_callback();
        datetime_update();
        tuh_task();
#endif
    }
}

int main()
{
    // Set system clock to 252 MHz for HSTX timing
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    set_sys_clock_khz(252000, true);
    stdio_init_all();

    printf("Hello world~\n");

    gpio_init(42);
    gpio_put(41, 1);
    sleep_ms(100);

    uint32_t irq = multicore_doorbell_irq_num(screenUpdated);
    irq_set_exclusive_handler(irq, core0_doorbell_irq);
    irq_set_priority(irq, 0xF0);
    irq_set_enabled(irq, true);

#ifdef ENABLE_PCBOARD
    pio_usb_configuration_t pio_cfg = {
        28, 2, PIO_SM_USB_TX_DEFAULT,
        6, 2, PIO_SM_USB_RX_DEFAULT,
        PIO_SM_USB_EOP_DEFAULT, NULL, PIO_USB_DEBUG_PIN_NONE,
        PIO_USB_DEBUG_PIN_NONE, false, PIO_USB_PINOUT_DPDM};

    tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

    tuh_init(1);
#endif

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

//--------------------------------------------------------------------+
// Host HID
//--------------------------------------------------------------------+
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len)
{
    (void)desc_report;
    (void)desc_len;

    // Interface protocol (hid_interface_protocol_enum_t)
    const char *protocol_str[] = {"None", "Keyboard", "Mouse"};
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    uint16_t vid, pid;
    tuh_vid_pid_get(dev_addr, &vid, &pid);

    char tempbuf[256];
    printf(tempbuf, "[%04x:%04x][%u] HID Interface%u, Protocol = %s\r\n", vid, pid, dev_addr, instance, protocol_str[itf_protocol]);

    // Receive report from boot keyboard & mouse only
    // tuh_hid_report_received_cb() will be invoked when report is available
    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD || itf_protocol == HID_ITF_PROTOCOL_MOUSE)
    {
        if (!tuh_hid_receive_report(dev_addr, instance))
        {
            printf("Error: cannot request report\r\n");
        }
    }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    printf("[%u] HID Interface%u is unmounted\r\n", dev_addr, instance);
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len)
{
    (void)len;
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    switch (itf_protocol)
    {
    case HID_ITF_PROTOCOL_KEYBOARD:
        process_kbd_report(dev_addr, (hid_keyboard_report_t const *)report);
        break;

    case HID_ITF_PROTOCOL_MOUSE:
        process_mouse_report(dev_addr, (hid_mouse_report_t const *)report);
        break;

    default:
        break;
    }

    // continue to request to receive report
    if (!tuh_hid_receive_report(dev_addr, instance))
    {
        printf("Error: cannot request report\r\n");
    }
}