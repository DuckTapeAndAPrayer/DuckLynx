# Stock Firmware
All GPIO peripherals are enabled
Ports A-J are on the older APB
Ports K-P are on the newer AHP
I2C 4 and 5 aren't used on the original FW

## Serial numbers
Serial number is the serial number of the FTDI. Lynx devices that aren't connected by USB don't have a programmatically readable serial number.

## EEPROM
The first byte of EEPROM stores the address of the module. It seems like there is nothing else of importance in it.

## LEDS
All use general timers  
About a 3.21 nS pulse length (About 312 KHz). Seems to be chosen to allow allow the timer to range from 0-255  
0x100 is the value that it counts down from  
0x20 is transition for on  
0x0 prescaler 

### Colors
`#FF2000` - no 12V power flashing orange  
`#000020` - just powered on blue  
`#00007f` - flashing blue for rhsp timeout  
`#002000` - rhsp connected green
For flashing modes leds blink every 600 ms (300 ms on 300 ms off). 
Expansion hubs blink their address every so often, but control hubs don't. This behavior comes from a pattern sent by the RC app

## What to flash to a new MCU to use the stock firmware
Flash
EEPROM <!-- TEST: Does the stock firmware write the eeprom if its FF or 00? -->
BOOTCFG - ffff24fe
    Port B pin 1 for bootloader entry
    Gets automatically set by the stock firmware. It will boot loop after being uploaded for the first time until the device is power on reset because it takes a power on reset for the BOOTCFG register to become permanent.
User registers 1-4 - all just F and don't need to be flashed

