/**
 * @file msc-demo-cli.c
 * @brief these functions implement the CLI for the MSC demo
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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include "ff.h"
#include "rp2040_rtc.h"
#include "msc-demo-cli.h"
#include "pico/stdlib.h"
static EmbeddedCli *cli;
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
    (void)cli;
    (void)args;
    (void)context;
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
    else if (argc == 1) {
        strncpy(temp_cwd, embeddedCliGetToken(args, 1), sizeof(temp_cwd)-1);
        temp_cwd[sizeof(temp_cwd)-1] = '\0';
        res = f_chdir(temp_cwd);
    }
    else {
        printf("usage: cd <new path>\r\n");
        return;
    }
    if (res != FR_OK) {
        printf("error %u setting cwd to %s\r\n", res, temp_cwd);
    }
    res = f_getcwd(temp_cwd, sizeof(temp_cwd));
    if (res == FR_OK)
        printf("cwd=\"%s\"\r\n", temp_cwd);
    else
        printf("error %u getting cwd\r\n", res);
}

static void on_chdrive(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)context;
    uint16_t argc = embeddedCliGetTokenCount(args);
    FRESULT res;
    char dstr[] = "0:";
    if (argc != 1) {
        printf("usage chdrive drive_number(0-3)");
    }
    else {
        int drive = atoi(embeddedCliGetToken(args, 1));
        if (drive >=0 && drive <= 3) {
            dstr[0] += drive;
            res = f_chdrive(dstr);
            if (res != FR_OK) {
                printf("error %u setting drive to %d\r\n", res, drive);
            }
        }
        else {
            printf("usage chdrive drive_number(0-3)");
        }
    }
}


static void print_fat_date(WORD wdate)
{
    uint16_t year = 1980 + ((wdate >> 9) & 0x7f);
    uint16_t month = (wdate >> 5) & 0xf;
    uint16_t day = wdate & 0x1f;
    printf("%02u/%02u/%04u\t", month, day, year);
}

static void print_fat_time(WORD wtime)
{
    uint8_t hour = ((wtime >> 11) & 0x1f);
    uint8_t min = ((wtime >> 5) & 0x3F);
    uint8_t sec = ((wtime &0x1f)*2);
    printf("%02u:%02u:%02u\t", hour, min, sec);
}

static FRESULT scan_files(const char* path)
{
    FRESULT res;
    DIR dir;
    //UINT i;
    static FILINFO fno;

    res = f_opendir(&dir, path);                       /* Open the directory */
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);                   /* Read a directory item */
            if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
            #if 0 // Do not do recursive scan.
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
            printf("%lu\t",fno.fsize);
            print_fat_date(fno.fdate);
            print_fat_time(fno.ftime);
            printf("%s%c\r\n",fno.fname, (fno.fattrib & AM_DIR) ? '/' : ' ');
        }
        f_closedir(&dir);
    }

    return res;
}

static void on_ls(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)args;
    (void)context;
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
        printf("usage: cat filename\r\n");
    }
}

static void on_mkdir(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)context;
    if (embeddedCliGetTokenCount(args) == 1) {
        char dirname[256];
        strncpy(dirname, embeddedCliGetToken(args, 1), sizeof(dirname)-1);
        dirname[sizeof(dirname)-1] = '\0';
        FRESULT res = f_mkdir(dirname);
        if (res != FR_OK) {
            printf("error %u creating directory %s\r\n", res, dirname);
        }
    }
    else {
        printf("usage: mkdir directory-name\r\n");
    }
}

static void on_pwd(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)args;
    (void)context;
    char cwd[256];
    FRESULT res = f_getcwd(cwd, sizeof(cwd));
    if (res != FR_OK) {
        printf("error %u reading the current working directory\r\n", res);
    }
    else {
        printf("cwd=%s\r\n", cwd);
    }
}

static void on_rm(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)context;
    if (embeddedCliGetTokenCount(args) == 1) {
        char fn[256];
        strncpy(fn, embeddedCliGetToken(args, 1), sizeof(fn)-1);
        fn[sizeof(fn)-1] = '\0';
        FRESULT res = f_unlink(fn);
        if (res != FR_OK) {
            printf("error %u deleting %s\r\n", res, fn);
        }
        else {
            printf("%s deleted\r\n", fn);
        }
    }
    else {
        printf("usage: rm filename\r\n");
    }
}

static void on_mv(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)context;
    if (embeddedCliGetTokenCount(args) == 2) {
        char fn1[256];
        char fn2[256];
        strncpy(fn1, embeddedCliGetToken(args, 1), sizeof(fn1)-1);
        fn1[sizeof(fn1)-1] = '\0';
        strncpy(fn2, embeddedCliGetToken(args, 2), sizeof(fn2)-1);
        fn2[sizeof(fn2)-1] = '\0';

        FRESULT res = f_rename(fn1, fn2);
        if (res != FR_OK) {
            printf("error %u renaming %s to %s\r\n", res, fn1, fn2);
        }
        else {
            printf("%s renamed to %s\r\n", fn1, fn2);
        }
    }
    else {
        printf("usage: mv old new\r\n");
    }
}

static void on_cp(EmbeddedCli *cli, char *args, void *context)
{
    (void)cli;
    (void)context;
    if (embeddedCliGetTokenCount(args) == 2) {
        char fn1[256];
        char fn2[256];
        strncpy(fn1, embeddedCliGetToken(args, 1), sizeof(fn1)-1);
        fn1[sizeof(fn1)-1] = '\0';
        strncpy(fn2, embeddedCliGetToken(args, 2), sizeof(fn2)-1);
        fn2[sizeof(fn2)-1] = '\0';
        FIL src, dest;
        FRESULT res = f_open(&src, fn1, FA_READ);
        if (res == FR_OK) {
            res = f_open(&dest, fn2, FA_WRITE | FA_CREATE_NEW);
            if (res == FR_OK) {
                uint8_t buffer[512];
                UINT nread = sizeof(buffer);
                while (res == FR_OK && nread == sizeof(buffer)) {
                    res = f_read(&src, buffer, sizeof(buffer), &nread);
                    if (res == FR_OK) {
                        UINT nwritten;
                        res = f_write(&dest, buffer, nread, &nwritten);
                    }
                }
                if (res == FR_OK) {
                    printf("%s copied to %s\r\n", fn1, fn2);
                }
                else {
                    printf("error %u copying %s to %s\r\n", res, fn1, fn2);
                }
                f_close(&src);
                f_close(&dest);
            }
            else {
                f_close(&src);
                printf("error %u opening %s for writing\r\n", res, fn2);
            }
        }
        else {
            printf("error %u opening %s for reading\r\n", res, fn1);
        }
    }
    else {
        printf("usage: cp from-file to-file\r\n");
    }
}

void msc_demo_cli_init()
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
    EmbeddedCliConfig demo_config;

    demo_config.rxBufferSize = 64;
    demo_config.cmdBufferSize = 64;
    demo_config.historyBufferSize = 128;
    demo_config.cliBuffer = NULL;
    demo_config.cliBufferSize = 0;
    demo_config.maxBindingCount = 16;
    demo_config.enableAutoComplete = true;
    demo_config.invitation = "> ";

    cli = embeddedCliNew(&demo_config);
    cli->onCommand = onCommandFn;
    cli->writeChar = writeCharFn;
    bool result = embeddedCliAddBinding(cli, {
            "cat",
            "print the specified file; usage cat filename",
            true,
            NULL,
            on_cat
    });
    assert(result);
    result = embeddedCliAddBinding(cli, {
            "cd",
            "change the current working directory; usage cd <new path>",
            true,
            NULL,
            on_cd
    });
    assert(result);
    result = embeddedCliAddBinding(cli, {
            "chdrive",
            "change the current drive number; usage chdrive drive_number(0-3)",
            true,
            NULL,
            on_chdrive
    });
    assert(result);
    result = embeddedCliAddBinding(cli, {
            "cp",
            "copy an unopened file to a different unopened file; usage cp old_file new_file",
            true,
            NULL,
            on_cp
    });
    assert(result);
    result = embeddedCliAddBinding(cli, {
            "get-date",
            "get the date for file timestamps; usage get-date",
            false,
            NULL,
            on_get_date
    });
    assert(result);
    result = embeddedCliAddBinding(cli, {
            "get-fattime",
            "get the date and time for file timestamps; usage get-fattime",
            false,
            NULL,
            on_get_fat_time
    });
    assert(result);
    result = embeddedCliAddBinding(cli, {
            "get-free",
            "get drive free space",
            false,
            NULL,
            on_get_free
    });
    assert(result);
    result = embeddedCliAddBinding(cli, {
            "get-time",
            "get the time of day for file timestamps; usage get-time",
            false,
            NULL,
            on_get_time
    });
    assert(result);
    result = embeddedCliAddBinding(cli, {
            "ls",
            "list current directory; usage ls",
            false,
            NULL,
            on_ls
    });
    assert(result);
    result = embeddedCliAddBinding(cli, {
            "mkdir",
            "create a new directory; usage mkdir new_directory_name",
            true,
            NULL,
            on_mkdir
    });
    assert(result);
    result = embeddedCliAddBinding(cli, {
            "mv",
            "rename an unopened file or an unopened, empty directory; usage mv old_name new_name",
            true,
            NULL,
            on_mv
    });
    assert(result);
    result = embeddedCliAddBinding(cli, {
            "pwd",
            "print the current working directory; usage pwd",
            false,
            NULL,
            on_pwd
    });
    assert(result);
    result = embeddedCliAddBinding(cli, {
            "rm",
            "delete an unopened file or an unopened, empty directory; usage rm name",
            true,
            NULL,
            on_rm
    });
    assert(result);
    result = embeddedCliAddBinding(cli, {
            "set-date",
            "change the date for file timestamps; usage set-date year(2022-9999) month(1-12) day(1-31)",
            true,
            NULL,
            on_set_date
    });
    assert(result);
    result = embeddedCliAddBinding(cli, {
            "set-time",
            "change the time of day for file timestamps; usage set-time hour(0-23) minute(0-59) second(0-59)",
            true,
            NULL,
            on_set_time
    });
    assert(result);
    (void)result;
    // flush out junk from the keyboard buffer
    int c;
    do {
        c = getchar_timeout_us(0);
    } while(c != PICO_ERROR_TIMEOUT);

    embeddedCliProcess(cli);
}

void msc_demo_cli_task()
{
    int c = getchar_timeout_us(0);
    if (c != PICO_ERROR_TIMEOUT) {
        embeddedCliReceiveChar(cli, c);
        embeddedCliProcess(cli);
    }
}
