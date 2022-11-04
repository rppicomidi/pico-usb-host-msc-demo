# pico-usb-host-msc-demo
A CLI-driven demo of a Raspberry Pi Pico operating as a USB Mass Storage Class Host 

# Build Instructions
Make sure your pico-sdk library has latest version of the tinyusb library that has
pull request 1434 merged in or else the USB host bulk transfers that the Mass
Storage Class requires won't be supported.

If the merge has not happened yet, create a temporary branch in your pico-sdk directory like this:

```
cd pico-sdk/lib/tinyusb
git fetch origin pull/1434/head:test_bulk
git checkout test_bulk
```

Next get this source code assuming the parent directory is called `${PROJECTS}`
and build

```
cd ${PROJECTS}
git clone https://github.com/rppicomidi/pico-usb-host-msc-demo.git
mkdir build
cd build
cmake ..
make
```

# Hardware hookup
You will need a UART terminal connected to pins 1 and 2 of the Pico board so
you can use the command line interpreter. You will need a microUSB to USB A
female adapter to interface with most USB flash drives. You will also need
to provide 5V to the Vbus pin so the Pico board will boot and to power the
USB flash drive.

I use a Picoprobe board hooked to a computer to provide the UART terminal interface
and to provide the 5V to VBus (I just hook the Vbus pins together). I also use
it to download code to the board.

# Usage
Hook up the hardware and start a terminal program. Get the code into the target Pico board.
Make sure the board boots. The terminal should display the message

```
Pico USB Host Mass Storage Class Demo 
```

Attach a USB flash drive to the USB A female adapter hooked to the Pico's microUSB
connector. The terminal should display a message similar to this:

```
A MassStorage device is mounted
General  USB Flash Disk   rev 1100
Disk Size: 15384 MB
Block Count = 31506432, Block Size: 512   
```

Type the command `help` at any time to see the list of CLI commands.

Enjoy.
