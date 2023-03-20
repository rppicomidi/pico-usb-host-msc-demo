# pico-usb-host-msc-demo
A CLI-driven demo of a Raspberry Pi Pico operating as a USB Mass Storage Class Host

This demo supports up to 4 USB flash drives connected through a
hub to the RP2040 USB port. The demo uses a terminal-based command
line interpreter to access the drives. It supports only one
logical unit per drive. Drives are identified by drive number,
0-3.

This project uses the pico-sdk, the very latest tinyusb library,
and the elm-chan fatfs file system to implement the USB Host MSC.
The command line interpreter is built on the embedded-cli project.

If you prefer to use the examples that come with the [tinyusb stack](https://github.com/hathach/tinyusb),
please look at the USB Host [msc_file_explorer](https://github.com/hathach/tinyusb/tree/master/examples/host/msc_file_explorer)
example. It appears to be a more generic adaptation of this code.

# Build Instructions
As of this writing, USB host bulk transfers that the Mass Storage
Class requires are not supported for RP2040 in the version of
tinyusb that ships with the pico-sdk. You need the very latest
version of tinyusb to get that. This assumes `${PICO_SDK_PATH}`
points to your `pico-sdk` directory.

```
cd ${PICO_SDK_PATH}/lib/tinyusb
git checkout master
git pull
```

Get this project's source code assuming the parent directory is called
`${PROJECTS}`.

```
cd ${PROJECTS}
git clone https://github.com/rppicomidi/pico-usb-host-msc-demo.git
cd pico-usb-host-msc-demo
git submodule update --recursive --init
```

Build the software. The build will not work correctly unless
you have already set the `PICO_SDK_PATH` environment variable
to point to your `pico-sdk` directory

If you use a Pico W board, you must set the `PICO_BOARD` environment
variable to `pico_w`.

```
cd ${PROJECTS}/pico-usb-host-msc-demo
mkdir build
cd build
cmake ..
make
```
If you are using a Pico W board, you may see some unused parameter warnings
in the cyw43_arch_threadsafe_background.c file. You can safely ignore these.
The demo code is only using the `cyw43_arch` API to blink the LED.

# Hardware hookup
You will need a UART terminal connected to pins 1 and 2 of the Pico
board so you can use the command line interpreter. You will need a
microUSB to USB A female adapter to interface with most USB flash
drives. You will also need to provide 5V to the Vbus pin so the Pico
board will boot and to power the USB flash drive.

I use a Picoprobe board hooked to a computer to provide the UART
terminal interface and to provide the 5V to VBus (I just hook the
Vbus pins together). I also use it to download code to the board.

# Usage
Hook up the hardware and start a terminal program. Get the code into
the target Pico board. Make sure the board boots. The terminal should
display a message similar to this one. The initial date and time is
the last build date and time.

```
Pico USB Host Mass Storage Class Demo
date=11/05/2022 time=07:15:32
Please use the CLI to set the date and time for file
timestamps before accessing the filesystem
Type help for more information
```

Set the time and date so any changes to the file system have the
correct date and time stamps. Attach a USB flash drive to the USB A
female adapter hooked to the Pico's microUSB connector. The terminal
should display a message similar to this:

```
A MassStorage device is mounted
General  USB Flash Disk   rev 1100
Disk Size: 15384 MB
Block Count = 31506432, Block Size: 512   
```

Type the command `help` at any time to see the list of CLI commands.

Enjoy.
