# PIO-USB-Demo of pico-usb-msc-demo
This code does what the parent directory does except it uses the
Pico-PIO-USB code to create the USB Host port.

This project shares the same CLI and FatFs code as the parent
pico-usb-host-msc-demo code.

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
git submodule update --recursive --init
```

As of this writing, tinyusb is missing a needed delay after reset.
Without this delay, the mass storage class device will not return
its USB descriptor after reset.

```
cd ${PICO_SDK_PATH}/lib/tinyusb
git apply ${PROJECTS}/msc-usb-host-msc-demo/patches/0002-Add-missing-reset-recovery-delay.patch.patch
```

Build the software. The build will not work correctly unless
you have already set the `PICO_SDK_PATH` environment variable
to point to your `pico-sdk` directory

```
cd ${PROJECTS}/pico-usb-host-msc-demo/PIO-USB-Demo
mkdir build
cd build
cmake ..
make
```

# Hardware
This project wires the USB A port to pins 1 & 2 of the Raspberry
Pi Pico board. I am using a small USB A breakout board with 0603
27 ohm 1% resistors soldered in series with the D+ and D- pins. The
USB A ground pin is wired to Pico board Pin 3, and the VBus pin is
wired to the Pico board's Vbus pin.

You will need a UART terminal connected to pins 21 and 22 of the
Pico board so you can use the command line interpreter. You will also need to provide 5V to the Vbus pin so the Pico
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