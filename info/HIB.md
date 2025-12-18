# Lynx HIB (Hardware Interface Board)
## Components
A large chunk of the resistors are 0603
Aux Shunt resistors are 20 milli ohm in a 2512 package

### ICs
Main MCU [TM4C123GH6PGEI7](https://www.ti.com/product/TM4C123GH6PGE/part-details/TM4C123GH6PGEI7) [docs](https://www.ti.com/product/TM4C123GH6PGE#tech-docs)
Motor controller VNH5050AE full bridge motor controller
USB to UART FT230XQ
RS485 transceiver ST3485EB
Bus transceiver SN74LVC8T245
Op Amp for shunts K176 SOT 23-5 ST LMV321RILT
5V Buck converter driver tps54527
3.3V Buck converter driver TPS562209
701 IMU Bosch BNO055
AAXXX U17 Adjustable shunt voltage regulator TL431ASA
High side current monitor U25 ZXCT1010E5TA
USB OTG ESD Diodes U19 PUSBM5V5X4-TL

### Diodes
Big zener diode for motor drives D4 1SMB5931BT3G
Tiny ESD diode D5 PESD15VS1UL
ESD Protection E5U Chip CPDV5-5V0U
ESD Protection E3V3 Chip CPDV5-3V3UP
Status LED D1 LTST-G683GEBW

### Transistors
Reverse current protection BIG mosfet FDD9409-F085
NPN Transistor 43 DTC143XKA
N channel MOSFET WJ3 BSH103
P channel mosfet KFH 5V servo power enabe SSM3J328R
P channel mosfet phone charging X73L AO3407A
N channel MOSFET K38 BSS138W
N channel MOSFET N32 GN (Last few characters may be different) Q13 and others AO3434A

### Connectors
XT30 Male Amass [XT30UPB-M](https://www.tme.com/ux/en-us/details/xt30upb-m/dc-power-connectors/amass/)
XT30 Female Amass [XT30UPB-F](https://www.tme.com/ux/en-us/details/xt30upb-f/dc-power-connectors/amass/)
Mini USB B [MUSBS5FBM1RA](https://leoco.com.tw/product/mini-usb-connector/)
 - That part number exists nowhere on the internet. 
 - It also is Leoco product series 0850 and P/N 0850BFBD111. The F could be J or K as the gold plating thickness is unknown
 - TE Connectivity P/N 1734035-1 seems to have the same pad dimensions, so it may work
JTAG Connector Female Molex [53398-0671](https://www.molex.com/en-us/products/part-detail/533980671)
JTAG Connector Male Cable Molex [15134-0605](https://www.molex.com/en-us/products/part-detail/151340605)
All of shrouded external connectors are JST PH
All the motor connectors are JST VH

### Fuses
All from Bel Power? Bel Fuse?
They are green
b2 Fuse - 2A hold current - 0ZCJ0200FF2C
bS Fuse - 1.5 A hold current - 0ZCH0150FF2E
b1 Fuse - 1A hold current - 0ZCJ0100FF2E
bM Fuse - 0.5A hold current - 0ZCK0050FF2E

### Mystery Components
D2
D3 Maybe ESD Protection Diode LittelFuse SMF17A
 
## What to flash to a new chip
Flash
EEPROM <!-- TEST: Does the stock firmware write the eeprom if its FF or 00? -->
BOOTCFG - ffff24fe
    Port B pin 1 for bootloader entry
    Gets automatically set by the stock firmware. It will boot loop after being uploaded for the first time until the device is power on reset because it takes a power on reset for the BOOTCFG register to become permanent.
User registers 1-4 - all just F and don't need to be flashed

## Stock firmware
All GPIO peripherals are enabled
Ports A-J are on the older APB
Ports K-P are on the newer AHP
I2C 4 and 5 aren't used on the original FW

### Serial numbers
Serial number is the serial number of the FTDI

### EEPROM
The first byte of EEPROM stores the address of the module. It seems like there is nothing else of importance in it.

## Connector Pinouts
### Internal UART (J16)
Seems to go to the Android Header. May go other places
Pins:
```
1 - Rx
2 - Tx
3 - GND
```

### JTAG Connector
Pinout:
Pin 1 has a dot next to it on the PCB
```
1 - 3V3
2 - TCK
3 - TMS
4 - TDI
5 - TDO
6 - GND
```

### Android Header (J??)
The header that connects to a Android board (if equipped)
Pin numbering:
```
 1 - - 40
 2 - - 39
 3 - - 38
 :     :
 :     :
20 - - 21
```
 5 - R67
19 - +12V
20 - GND
21 - GND
23 - +1.8V to bus transceivers
40 - GND

## Bus Transceivers
A is Android Header - 1.8V  
B is System - 3.3V
Eight channels

HIB to Android Header (B to A) (Dir is low)
- U21 
- CH 1
    - B U0Tx
    - A Android Header 37
- CH 2
    - B FX230X Pin 4 (CTS)
    - A Android Header 39
- CH 3
    - B Internal J16 UART Rx (Pin 1)
    - A Android Header 34
- CH 4
    - B PB0 ?? Is this just a GPIO?
    - A Android Header 27
- CH 5-8 are unused and connected to GND through a 10 kΩ resistor

Android Header to HIB (A to B) (Dir is high)
- U22
- CH 1
    - A Android Header 38
    - B U0Rx
- CH 2
    - A Android Header 36
    - B FT230X Pin 16 (RTS)
- CH 3
    - A Android Header 35
    - B Internal J16 UART Tx (Pin 2) 
- CH 4
    - A Android Header 4
    - B PC7
- CH 5
    - A Android Header 29
    - B MCU PB1 and TP3 (NPRG), and FTDI CBUS1
- CH 6
    - A Android Header 28
    - B MCU RST, TP2 (NRST) and FTDI CBUS0
- CH 7-8 are unused and connected to GND through a 10 kΩ resistor

## MCU Pin Connections
Overall temp sensor - in MCU
3.3V voltage sense - doesn't exist

### FT230XQ
PB1 (Bootconfig enter bootloader) FT230X Pin 11 (CBUS1) and TP3 (NPRG)
RST FT230X Pin 12 (CBUS0) and TP2 (NRST)
PJ7 FT230X Pin 5 (CBUS2)
PK5 FT230X Pin 16 (RTS) and Android Header 36

### UART
U0Tx (PA1) FT230X Rx  
U0Rx (PA0) FT230X Tx
PK5 FT230X CTS and Android Header 39
PK4 FT230X RTS and Android Header 36

U1Tx (PC5) RS485 Tx  
U1Rx (PC4) RS485 Rx  
PF1 RS485 Read enable
PF0 RS485 Output enable  

U2Tx (PD7) External UART upper - seems dead on the test hub  
U2Rx (PD6) External UART upper

U4Tx (PJ1) External UART lower  
U4Rx (PJ0) External UART lower

### LEDS
PM3 Blue Led - Pin 86 (WT5CCP1)
PM6 Red led - Pin 83 (WT0CCP0)
PM7 Green Led - Pin 82 (WT0CCP1) 
All used general timers
About a 3.21 nS pulse length (About 312 KHz)
0x100 is the value that it counts down from
0x20 is transition for on
0x0 prescaler

Colors:
#ff2000 - no power flashing orange
#000020 - just powered on blue
#00007f - flashing blue for rhsp timeout
#002000 - rhsp connected green
For flashing modes leds blink every 600 ms (300 ms on 300 ms off). 
Expansion hubs blink their address every so often, but control hubs don't. This behavior comes from a pattern sent by the RC app
Expansion hubs blink their address every so often, but control hubs don't. This behavior comes from a pattern sent by the RC app

### Button
PB4 Button by RS458  

### I2C
Current sense opamp out AIN7 (PD4)

I2C0SDA (PB3) External I2C 0 SDA  
I2C0SCL (PB2) External I2C 0 SCL

I2C1SDA (PA7) External I2C 1 SDA
I2C1SCL (PA6) External I2C 1 SCL

I2C2SDA (PE5) External I2C 2 SDA
I2C2SCL (PE4) External I2C 2 SCL

I2C3SDA (PD1) External I2C 3 SDA
I2C3SCL (PD0) External I2C 3 SCL

### Digital GPIO
Current sense opamp out AIN6 (PD5)
GPIOs are set as open drain pins and have pull up resistors
The GPIO has a pull up resistor
The upper resistor goes to the gate of a N channel mosfet with an unknown part number that will pull the GPIO down.
The lower one comes from the pin and goes through a resistor to a pin on the MCU for input
Output and input are from the perspective of the lynx

GPIO 0:
PN4 R38 Left (output)
PL0 R39 Left (input)

GPIO 1:
PN5 R36 Left (output)
PL1 R37 Left (input)

GPIO 2:
PN6 R34 Left (output)
PL2 R35 Left (input)

GPIO 3:
PN7 R32 Left (output)
PL3 R33 Left (input)

GPIO 4:
PJ4 R30 Left (output)
PL4 R31 Left (input)

GPIO 5:
PJ5 R28 Left (output)
PL5 R29 Left (input)

GPIO 6:
PN2 R26 Left (output)
PM0 R27 Left (input)

GPIO 7:
PP2 R24 Left (output)
PM1 R25 Left (input)

### Analog GPIO
VCCA and GNDA are just connected to VCC and GND
VREFA- is connected to GND
VREFA+ is connected to a circuit that makes 2.994V. This is derived from the TL431A datasheet by using the formula Vout = Vref * (1 + (R1/R2)) which in the case of the HIB is 2.994 = 2.495 * (1 + (2000 / 10000))

AGPIO 0 AIN0 (PE3)
AGPIO 1 AIN1 (PE2)
AGPIO 2 AIN2 (PE1)
AGPIO 3 AIN3 (PE0)

### 5V rail
Buck converter enable Pin 1 PM2
Current Sense opamp out AIN12 (PD3)
Voltage monitoring AIN23 (PP0) - Through resistor divider with two 10K resistors to divide voltage by 2. 

### Servo
Servo 5 PWM M0PWM7 (PH7)
Servo 4 PWM M0PWM6 (PH6)
Servo 3 PWM M0PWM3 (PH3)
Servo 2 PWM M0PWM2 (PH2)
Servo 1 PWM M1PWM1 (PG3)
Servo 0 PWM M0PWM0 (PG2)
Servo 5V enable PC6 - Has pull down

### Motors
M0:
 - CS AIN16 (PK0)
 - EN A/B PN1
 - INA PK6
 - INB PF2
 - PWM M0PWM0 (PH0)

M1:
 - CS AIN17 (PK1)
 - EN A/B PN0
 - INA PK7
 - INB PF3
 - PWM M0PWM1 (PH1)

M2:
 - CS AIN20 (PE7)
 - EN A/B PM4
 - INA PG6
 - INB PK2
 - PWM M1PWM2 (PG4)

M3:
 - CS AIN21 (PE6)
 - EN A/B PM5
 - INA PG7
 - INB PK3
 - PWM M1PWM3 (PG5)

### Encoder
CH0
 - A PhA0 (PH4)
 - B PhB0 (PH5)

CH1
 - A PF6
 - B PF7

CH2
 - A PF4
 - B PF5

CH3
 - A PhA1 (PG0)
 - B PhB1 (PG1)

### Battery
Voltage - connected to AIN22 (PP1) via a 1/6 resistor divider with 10K and 2K resistors
Current - Uses a high side current monitor IC with a 0.003 ohm shunt and 3K Rout resistor into AIN13 (PD2)

### External oscillator
Called main oscillator by the datasheet
Connected to pins 92 and 93 which are OSC0 and OSC1
16 MHz

### SSI 0
Goes to J19 SPI
J19 SPI:
```
Pin 1 - VCC
Pin 2 - CLK - SSI0Clk (PA2)
Pin 3 - CS - SSI0Fss (PA3)
Pin 4 - MISO - SSI0Rx (PA4)
Pin 5 - MOSI - SSI0Tx (PA5)
Pin 6 - GND
```