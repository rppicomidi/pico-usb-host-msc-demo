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
#include "bsp/board.h"
#include "tusb.h"
#include "class/msc/msc_host.h"
#include "ff.h"
#include "diskio.h"
#include "rp2040_rtc.h"
#include "embedded_cli.h"

// On-board LED mapping. If no LED, set to NO_LED_GPIO
const uint NO_LED_GPIO = 255;
const uint LED_GPIO = 25;
// UART selection Pin mapping. You can move these for your design if you want to
// Make sure all these values are consistent with your choice of midi_uart
#define MIDI_UART_NUM 1
const uint MIDI_UART_TX_GPIO = 4;
const uint MIDI_UART_RX_GPIO = 5;

static void *midi_uart_instance;
static uint8_t midi_dev_addr = 0;
static EmbeddedCli *cli;

static scsi_inquiry_resp_t inquiry_resp;
static FATFS fatfs[CFG_TUH_MSC];


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
        gpio_put(LED_GPIO, led_state);
        led_state = !led_state;
        previous_timestamp = now;
    }
}

// Required functions for the CLI
static void onCommand(const char* name, char *tokens)
{
    printf("Received command: %s\r\n",name);

    for (int i = 0; i < embeddedCliGetTokenCount(tokens); ++i) {
        printf("Arg %d : %s\r\n", i, embeddedCliGetToken(tokens, i + 1));
    }
}

static void onCommandFn(EmbeddedCli *embeddedCli, CliCommand *command)
{
    (void)embeddedCli;
    embeddedCliTokenizeArgs(command->args);
    onCommand(command->name == NULL ? "" : command->name, command->args);
}

static void writeCharFn(EmbeddedCli *embeddedCli, char c)
{
    (void)embeddedCli;
    putchar(c);
}

static void on_get_free(EmbeddedCli *cli, char *args, void *context)
{
    FATFS* fs;
    DWORD fre_clust, fre_sect, tot_sect;
    FRESULT res = f_getfree("", &fre_clust, &fs);
    if (res != FR_OK) {
        printf("error %u getting free space\r\n", res);
        return;
    }
    /* Get total sectors and free sectors */
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;

    /* Print the free space (assuming 512 bytes/sector) */
    printf("%10lu KiB total drive space.\r\n%10lu KiB available.\r\n", tot_sect / 2, fre_sect / 2);
}

static void on_cd(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)context;
    uint16_t argc = embeddedCliGetTokenCount(args);
    FRESULT res;
    char temp_cwd[256] = {'/', '\0'};
    if (argc == 0) {
        res = f_chdir(temp_cwd);
    }
    else {
        strncpy(temp_cwd, embeddedCliGetToken(args, 1), sizeof(temp_cwd)-1);
        temp_cwd[sizeof(temp_cwd)-1] = '\0';
        res = f_chdir(temp_cwd);
    }
    if (res != FR_OK) {
        printf("error %u setting cwd to %s\r\n", temp_cwd);
    }
    res = f_getcwd(temp_cwd, sizeof(temp_cwd));
    if (res == FR_OK)
        printf("cwd=\"%s\"\r\n", temp_cwd);
    else
        printf("error %u getting cwd\r\n", res);
}

static FRESULT scan_files(const char* path)
{
    FRESULT res;
    DIR dir;
    UINT i;
    static FILINFO fno;

    res = f_opendir(&dir, path);                       /* Open the directory */
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);                   /* Read a directory item */
            if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
            #if 0
            if (fno.fattrib & AM_DIR) {                    /* It is a directory */
                i = strlen(path);
                sprintf(&path[i], "/%s", fno.fname);
                res = scan_files(path);                    /* Enter the directory */
                if (res != FR_OK) break;
                path[i] = 0;
            } else {                                       /* It is a file. */
                printf("%s/%s\r\n", path, fno.fname);
            }
            #endif
            printf("%s%c\r\n",fno.fname, (fno.fattrib & AM_DIR) ? '/' : ' ');
        }
        f_closedir(&dir);
    }

    return res;
}

static void on_ls(EmbeddedCli *cli, char *args, void *context)
{
    FRESULT res = scan_files(".");
    if (res != FR_OK) {
        printf("Error %u listing files on drive\r\n", res);
    }
}


static void on_set_date(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)context;
    bool usage_ok = false;
    if (embeddedCliGetTokenCount(args) == 3) {
        int16_t year = atoi(embeddedCliGetToken(args, 1));
        int8_t month = atoi(embeddedCliGetToken(args, 2));
        int8_t day = atoi(embeddedCliGetToken(args, 3));
        usage_ok = rppicomidi::Rp2040_rtc::instance().set_date(year, month, day);
    }
    if (!usage_ok) {
        printf("usage: set-date YYYY(1980-2107) MM(1-12) DD(1-31)\r\n");
    }
}

static void on_set_time(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)context;
    bool usage_ok = false;
    if (embeddedCliGetTokenCount(args) == 3) {
        int8_t hour = atoi(embeddedCliGetToken(args, 1));
        int8_t min = atoi(embeddedCliGetToken(args, 2));
        int8_t sec = atoi(embeddedCliGetToken(args, 3));
        usage_ok = rppicomidi::Rp2040_rtc::instance().set_time(hour, min, sec);
    }
    if (!usage_ok) {
        printf("usage: set-time hour(0-23) min(0-59) sec(0-59)\r\n");
    }
}

static void on_get_date(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)args;
    (void)context;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    rppicomidi::Rp2040_rtc::instance().get_date(year, month, day);
    printf("date(MM/DD/YYYY)=%02u/%02u/%04u\r\n", month, day, year);
}

static void on_get_time(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)args;
    (void)context;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    rppicomidi::Rp2040_rtc::instance().get_time(hour, min, sec);
    printf("time=%02u:%02u:%02u\r\n",hour, min, sec);
}

static void on_get_fat_time(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)args;
    (void)context;
    DWORD f_time=get_fattime();
    int16_t year = ((f_time >> 25 ) & 0x7F) + 1980;
    int8_t month = ((f_time >> 21) & 0xf);
    int8_t day = ((f_time >> 16) & 0x1f);
    int8_t hour = ((f_time >> 11) & 0x1f);
    int8_t min = ((f_time >> 5) & 0x3F);
    int8_t sec = ((f_time &0x1f)*2);
    printf("%02d/%02d/%04d %02d:%02d:%02d\r\n", month, day, year, hour, min, sec);
}

static void on_cat(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)context;
    if (embeddedCliGetTokenCount(args) == 1) {
        char fn[256];
        FIL fil;
        strncpy(fn, embeddedCliGetToken(args, 1), sizeof(fn)-1);
        FRESULT res = f_open(&fil, fn, FA_READ);
        if (res != FR_OK) {
            printf("error %u opening file %s\r\n", res, fn);
            return;
        }
        /* Read every line and display it */
        char line[100];
        while (f_gets(line, sizeof(line), &fil)) {
            printf(line);
        }

        /* Close the file */
        f_close(&fil);
    }
    else {
        printf("usage: set-time hour(0-23) min(0-59) sec(0-59)\r\n");
    }
}

static void initialize_cli()
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t sec;
    rppicomidi::Rp2040_rtc::instance().get_date(year, month, day);
    rppicomidi::Rp2040_rtc::instance().get_time(hour, minute, sec);
    printf("date=%02u/%02u/%04u time=%02u:%02u:%02u\r\n", month, day, year, hour, minute, sec);
    printf("Please use the CLI to set the date and time for file\r\ntimestamps before accessing the filesystem\r\n");
    printf("Type help for more information\r\n");

    cli = embeddedCliNewDefault();
    cli->onCommand = onCommandFn;
    cli->writeChar = writeCharFn;

    embeddedCliAddBinding(cli, {
            "cat",
            "print the specified file",
            true,
            NULL,
            on_cat
    });

    embeddedCliAddBinding(cli, {
            "cd",
            "change the current working directory",
            true,
            NULL,
            on_cd
    });

    embeddedCliAddBinding(cli, {
            "get-date",
            "get the date for file timestamps",
            false,
            NULL,
            on_get_date
    });

    embeddedCliAddBinding(cli, {
            "get-fattime",
            "get the date and time for file timestamps",
            false,
            NULL,
            on_get_fat_time
    });

    embeddedCliAddBinding(cli, {
            "get-time",
            "get the time of day for file timestamps",
            false,
            NULL,
            on_get_time
    });

    embeddedCliAddBinding(cli, {
            "ls",
            "list current directory",
            false,
            NULL,
            on_ls
    });

    embeddedCliAddBinding(cli, {
            "set-date",
            "change the date for file timestamps",
            true,
            NULL,
            on_set_date
    });

    embeddedCliAddBinding(cli, {
            "set-time",
            "change the time of day for file timestamps",
            true,
            NULL,
            on_set_time
    });

    embeddedCliProcess(cli);
}

extern "C" void main_loop_task()
{
    tuh_task();

    int c = getchar_timeout_us(0);
    if (c != PICO_ERROR_TIMEOUT) {
        embeddedCliReceiveChar(cli, c);
        embeddedCliProcess(cli);
    }

    blink_led();
}

int main()
{

    bi_decl(bi_program_description("Provide a USB host interface for FATFS formatted USB drives."));
    bi_decl(bi_1pin_with_name(LED_GPIO, "On-board LED"));

    board_init();
    printf("Pico USB Host Mass Storage Class Demo\r\n");
    tusb_init();

    // Map the pins to functions
    gpio_init(LED_GPIO);
    gpio_set_dir(LED_GPIO, GPIO_OUT);
    mmc_fat_init();
    initialize_cli();
    while (1) {
        main_loop_task();
    }
}

// For FatFs library
extern "C" DWORD get_fattime(void)
{
    return rppicomidi::Rp2040_rtc::instance().get_fat_date_time();
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// MSC implementation
//--------------------------------------------------------------------+
bool inquiry_complete_cb(uint8_t dev_addr, msc_cbw_t const* cbw, msc_csw_t const* csw)
{
    if (csw->status != 0) {
        printf("Inquiry failed\r\n");
        return false;
    }

    // Print out Vendor ID, Product ID and Rev
    printf("%.8s %.16s rev %.4s\r\n", inquiry_resp.vendor_id, inquiry_resp.product_id, inquiry_resp.product_rev);

    // Get capacity of device
    uint32_t const block_count = tuh_msc_get_block_count(dev_addr, cbw->lun);
    uint32_t const block_size = tuh_msc_get_block_size(dev_addr, cbw->lun);

    printf("Disk Size: %lu MB\r\n", block_count / ((1024*1024)/block_size));
    printf("Block Count = %lu, Block Size: %lu\r\n", block_count, block_size);

    return true;
}

//------------- IMPLEMENTATION -------------//
void tuh_msc_mount_cb(uint8_t dev_addr)
{
    printf("A MassStorage device is mounted\r\n");

    uint8_t pdrv = dev_addr-1;
    mmc_fat_plug_in(pdrv);
    uint8_t const lun = 0;
    tuh_msc_inquiry(dev_addr, lun, &inquiry_resp, inquiry_complete_cb);
    if ( f_mount(&fatfs[pdrv],"", 0) != FR_OK ) {
        printf("mount failed\r\n");
        return;
    }
    else {
        printf("FATFS drive %u mounted\r\n",pdrv);
    }
}

void tuh_msc_umount_cb(uint8_t dev_addr)
{
    printf("A MassStorage device is unmounted\r\n");

    uint8_t pdrv = dev_addr-1;

    f_mount(NULL, "", 0); // unmount disk
    mmc_fat_unplug(pdrv);
}

