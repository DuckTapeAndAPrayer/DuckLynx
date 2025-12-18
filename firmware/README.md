# DuckLynx Firmware

## Coding conventions
 - A simple write to one register should not be put into a function. The raw register should be written.
 - A function can be created when more logic than the above is needed
 - See register.h for how register names should work

## Building
This project uses CMake as its build system. You will need a ARM C compiler to compile the project. bakl uses `arm-none-eabi-gcc`. The toolchain is currently setup for building on linux. Modifications may be needed to build on Windows or MacOS. To build on linux do the following from the root of the project:
 1. `cd firmware`
 2. `mkdir build`
 3. `cmake ..`
 4. `make`

## Uploading
The easiest way to upload is using by using the Robot Controller Console. See instructions on [REV's website](https://docs.revrobotics.com/duo-control/managing-the-control-system/updating-firmware#using-the-robot-controller-console), but instead of uploading the bundled firmware update or the one from REV's site upload DuckLynx instead. Firmware can also be updated by [using the Driver Station or Robot Controller Apps](https://ftc-docs.firstinspires.org/en/latest/ftc_sdk/updating/hub_firmware/Updating-Hub-Firmware.html) and the using the DuckLynx file, not the REV one.

Firmware can also be uploaded via JTAG which also allows for debugging. See 'Debugging' section.

## Reverting to the stock firmware
Simply follow the uploading instructions again but upload [the file from REV](https://docs.revrobotics.com/duo-control/managing-the-control-system/updating-firmware#download-the-latest-rev-hub-firmware-version-1.8.1). DuckLynx does not modify the firmware upload process, so even if DuckLynx has a problem the stock firmware can always be easily restored without opening the Lynx.

## Debugging
The JTAG interface on the Lynx HIB is fully enabled from the factory. To use it open the Lynx and wire up the JTAG port to your JTAG probe. See the schematic for the pinout and part number of the connector. bakl uses an FTDI FT2232H with OpenOCD. "FTDI FT2232H Mini Modules" can be found cheaply online. Some work better than others. bakl recommends buying a genuine one from FTDI. The OpenOCD config file can be found in `openocd\ configs/ftdi_lynx.cfg` and ran with `openocd -f openocd\ configs/ftdi_lynx.cfg`. To upload connect to OpenOCD over telnet with `telnet localhost 4444` then run `program <path to file name> reset` in the telnet prompt. To read a memory location run `mrw <address>`. To exit run `shutdown`.

## Features Compared to Stock firmware
<!-- ADD: Features compared to stock firmware -->