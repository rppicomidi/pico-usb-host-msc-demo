/**
 * @file Pico-USB-Host-MIDI-Adapter.c
 * @brief A USB Host to Serial Port MIDI adapter that runs on a Raspberry Pi
 * Pico board
 * 
 * MIT License

 * Copyright (c) 2022 rppicomidi

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#if CFG_TUH_RPI_PIO_USB
#include "pico/multicore.h"
#include "pico/bootrom.h"
#include "pio_usb.h"
#include "hardware/clocks.h"
#else
#include "bsp/board.h"
#endif
#include "tusb.h"
#include "class/msc/msc_host.h"
#include "ff.h"
#include "diskio.h"
#include "msc-demo-cli.h"
#ifdef RPPICOMIDI_PICO_W
#include "pico/cyw43_arch.h"
#endif

// On-board LED mapping. If no LED, set to NO_LED_GPIO
const uint NO_LED_GPIO = 255;
const uint LED_GPIO = 25;

static scsi_inquiry_resp_t inquiry_resp;
static FATFS fatfs[CFG_TUH_DEVICE_MAX];
static_assert(FF_VOLUMES == CFG_TUH_DEVICE_MAX);



static void blink_led(void)
{
    static absolute_time_t previous_timestamp = {0};

    static bool led_state = false;

    // This design has no on-board LED
    if (NO_LED_GPIO == LED_GPIO)
        return;
    absolute_time_t now = get_absolute_time();
    
    int64_t diff = absolute_time_diff_us(previous_timestamp, now);
    if (diff > 1000000) {
#ifdef RPPICOMIDI_PICO_W
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);
#else
        gpio_put(LED_GPIO, led_state);
#endif
        led_state = !led_state;
        previous_timestamp = now;
    }
}

void main_loop_task()
{
#if !defined(CFG_TUH_RPI_PIO_USB) || (CFG_TUH_RPI_PIO_USB == 0)
    tuh_task();
#endif
    msc_demo_cli_task();

    blink_led();
}

#if CFG_TUH_RPI_PIO_USB
// core1: handle host events
static volatile bool core1_booting = true;
static volatile bool core0_booting = true;
void core1_main() {
    sleep_ms(10);
    // Use tuh_configure() to pass pio configuration to the host stack
    // Note: tuh_configure() must be called before
    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = 16;
    tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);
    // To run USB SOF interrupt in core1, init host stack for pio_usb (roothub
    // port1) on core1
    tuh_init(1);
    core1_booting = false;
    while(core0_booting) {
    }
    while (true) {
        tuh_task(); // tinyusb host task
    }
}
#endif

int main()
{

    bi_decl(bi_program_description("Provide a USB host interface for FATFS formatted USB drives."));
    bi_decl(bi_1pin_with_name(LED_GPIO, "On-board LED"));

#if !defined(CFG_TUH_RPI_PIO_USB) || (CFG_TUH_RPI_PIO_USB == 0)
    stdio_init_all();

    tusb_init();
#else
    set_sys_clock_khz(120000, true);
    sleep_ms(10);

    stdio_uart_init();
    // all USB Host task run in core1
    multicore_reset_core1();
    multicore_launch_core1(core1_main);
#endif
    printf("Pico USB Host Mass Storage Class Demo\r\n");

#if defined(CFG_TUH_RPI_PIO_USB) && (CFG_TUH_RPI_PIO_USB == 1)
    // wait for core 1 to finish claiming PIO state machines and DMA
    while(core1_booting) {
    }
#endif
    // Map the pins to functions
#ifdef RPPICOMIDI_PICO_W
    if (cyw43_arch_init()) {
        printf("WiFi init failed");
        return -1;
    }
#else
    gpio_init(LED_GPIO);
    gpio_set_dir(LED_GPIO, GPIO_OUT);
#endif
    msc_fat_init();
    msc_demo_cli_init();
#if defined(CFG_TUH_RPI_PIO_USB) && (CFG_TUH_RPI_PIO_USB == 1)
    core0_booting = false;
#endif
    while (1) {
        main_loop_task();
    }
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// MSC implementation
//--------------------------------------------------------------------+
bool inquiry_complete_cb(uint8_t dev_addr, tuh_msc_complete_data_t const* cb_data)
{
    if (cb_data->csw->status != 0) {
        printf("Inquiry failed\r\n");
        return false;
    }

    // Print out Vendor ID, Product ID and Rev
    printf("%.8s %.16s rev %.4s\r\n", inquiry_resp.vendor_id, inquiry_resp.product_id, inquiry_resp.product_rev);

    // Get capacity of device
    uint32_t const block_count = tuh_msc_get_block_count(dev_addr, cb_data->cbw->lun);
    uint32_t const block_size = tuh_msc_get_block_size(dev_addr, cb_data->cbw->lun);

    printf("Disk Size: %lu MB\r\n", block_count / ((1024*1024)/block_size));
    printf("Block Count = %lu, Block Size: %lu\r\n", block_count, block_size);

    return true;
}

void tuh_msc_mount_cb(uint8_t dev_addr)
{
    uint8_t pdrv = msc_map_next_pdrv(dev_addr);

    assert(pdrv < FF_VOLUMES);
    msc_fat_plug_in(pdrv);
    uint8_t const lun = 0;
    tuh_msc_inquiry(dev_addr, lun, &inquiry_resp, inquiry_complete_cb, 0);
    char path[3] = "0:";
    path[0] += pdrv;
    if ( f_mount(&fatfs[pdrv],path, 0) != FR_OK ) {
        printf("mount drive %s failed\r\n", path);
        return;
    }
    if (f_chdrive(path) != FR_OK) {
        printf("f_chdrive(%s) failed\r\n", path);
        return;
    }
    printf("\r\nMass Storage drive %u is mounted\r\n", pdrv);
    printf("Run the set-date and set-time commands so file timestamps are correct\r\n\r\n");
}

void tuh_msc_umount_cb(uint8_t dev_addr)
{
    uint8_t pdrv = msc_unmap_pdrv(dev_addr);
    char path[3] = "0:";
    path[0] += pdrv;

    f_mount(NULL, path, 0); // unmount disk
    msc_fat_unplug(pdrv);
    printf("Mass Storage drive %u is unmounted\r\n", pdrv);
}

