# pico-usb-host-msc-demo
A CLI-driven demo of a Raspberry Pi Pico operating as a USB Mass Storage Class Host

This demo supports up to 4 USB flash drives connected through a
hub to the RP2040 USB hardware or to a USB host port constructed
from the RP2040's PIO hardware + 2 GPIO pins. The demo can run on
a Pico or Pico W board. The demo uses a terminal-based command
line interpreter to access the drives. It supports only one
logical unit per drive. Drives are identified by drive number,
0-3.

This project uses the pico-sdk, the very latest tinyusb library,
and the elm-chan fatfs file system to implement the USB Host MSC.
The command line interpreter is built on the embedded-cli project.
If you are using PIO and GPIO to make the USB host port, it uses
the [Pico-PIO-USB](https://github.com/sekigon-gonnoc/Pico-PIO-USB)
project, which used to be installed as a git submodule in the tinyusb library
but is no longer as of March 17, 2023.

If you prefer to use the examples that come with the [tinyusb stack](https://github.com/hathach/tinyusb),
please look at the USB Host [msc_file_explorer](https://github.com/hathach/tinyusb/tree/master/examples/host/msc_file_explorer)
example. It appears to be a more generic adaptation of this code.

# Flash drive compatibility and performance
I have tested this code to work with a 32 GB USB 3.0 flash drive. Theoretically,
the Elm-Chan FatFS file system supports larger ExFAT formatted drives, and
the USB MSC class should work with any MSC device type and not just flash drives, but
I have not tested it.

The RP2040's native USB hardware transfer rate maxes out at USB Full Speed (12Mbps).
Furthermore, it is not designed to be high performance in host
mode. Expect a transfer limit of a bit less than 64Kbytes/second. The `Pico-PIO-USB`
USB host has a different design; I have not computed its performance.
If you are primarily reading and writing limited numbers of small files,
the performance is probably fine. If you plan to stream audio or video, it is probably not.

# Environment variable configurations
Please set up the following environment variables before you
start the build.

- `PICO_SDK_PATH` must point to the directory in the filesystem where
you installed the `pico-sdk`
- `PICO_BOARD` If you are using a Pico W board instead of a Pico board, you need
to set the environment variable `PICO_BOARD` to `pico_w`. Otherwise, leave it unset.
- `RPPICOMIDI_PIO_HOST` should be undefined if you are using the RP2040 native USB
hardware for the USB Host. If you are using RP2040 PIO to add a USB host port,
set this variable in your environment to 1.

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
If you have set environment variable `RPPICOMIDI_PIO_HOST` to 1 so you can
use the PIO port for the USB Host, you need to install the latest version of the
`Pico-PIO-USB` project where tinyusb can find it. Do not use my fork of
`Pico-PIO-USB` because I only created that to push the fix required to
allow this project to coexist with the Pico W driver code. It is likely
the main project has advanced beyond that pull request.

```
cd ${PICO_SDK_PATH}/lib/tinyusb/hw/mcu/raspberry_pi/
git clone https://github.com/sekigon-gonnoc/Pico-PIO-USB.git
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

```
cd ${PROJECTS}/pico-usb-host-msc-demo
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```
If you are using a Pico W board, you may see some unused parameter warnings
in the cyw43_arch_threadsafe_background.c file. You can safely ignore these.
The demo code is only using the `cyw43_arch` API to blink the LED.

I tend to do my testing in debug mode, so I show using the `Debug` build
type in the instructions. If you use some other build type and you
encounter problems during testing, please file an issue and I will try
to address it.

# Hardware hookup
## If you are using the RP2040 USB hardware for the USB Host
You will need a UART terminal connected to pins 1 and 2 of the Pico
board so you can use the command line interpreter. You will need a
microUSB to USB A female adapter to interface with most USB flash
drives. You will also need to provide 5V to the Vbus pin so the Pico
board will boot and to power the USB flash drive.

I use a Picoprobe board hooked to a computer to provide the UART
terminal interface and to provide the 5V to VBus (I just hook the
Vbus pins together). I also use it to download code to the board.

## If you are using the PIO + 2 GPIO pins for the USB Host
I used a USB A female breakout board for the USB Host connector. I
cut the D+ and D- minus traces and scraped back enough solder mask
to solder in two 0603 27 ohm 1% metal film resistors in line with
the D+ and D- signals. I wired the D+ signal post resistor to Pico
board pin 1, and I wired the D- signal post resistor to Pico board
Pin 2. I wired the +5V of the breakout board to the Pico Vbus pin 40.
I wired the GND of the breakout board to Pico GND pin 3.

There is a [photo](https://github.com/rppicomidi/pico-usb-midi-processor/blob/main/doc/PUMP_USB_A.JPG) of ugly but functional USB A breakout wiring
in the `Wiring the USB Host port` section of the `README.md` file in the
[pico-usb-midi-processor](https://github.com/rppicomidi/pico-usb-midi-processor)
project.

You need a console UART so that you can use the command line interpreter.
Normally, Pins 1 and 2 of the Pico board are used for the debug UART.
However, the `Pico-PIO-USB` library defaults to using those pins, so
I moved them in the `CMakeLists.txt` file to pins 21 (UART TX) and 22 (UART RX).

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
correct date and time stamps. Attach a USB flash drive or a USB hub
to the USB A female adapter hooked to the Pico's microUSB connector
(or for PIO USB Host, to the USB A breakout board you just wired to the Pico).
The terminal should display a message similar to this:

```
A MassStorage device is mounted
General  USB Flash Disk   rev 1100
Disk Size: 15384 MB
Block Count = 31506432, Block Size: 512   
```

Type the command `help` at any time to see the list of CLI commands.

Enjoy.
