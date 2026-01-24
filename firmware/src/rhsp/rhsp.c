#include "rhsp.h"

#include <stdint.h>

#include "hardware/encoder.h"
#include "hardware/motor.h"
#include "hardware/register.h"
#include "debugUART.h"
#include "other.h"
#include "rhspUART.h"
#include "hardware/led.h"
#include "hardware/eeprom.h"
#include "hardware/adc.h"

//Ideally this file should be platform agnostic so it can be tested without a lynx board

const uint16_t RHSP_MAGIC_NUMBER = 0x4B44;
const uint8_t RHSP_MAGIC_NUMBER_BYTE_ONE = RHSP_MAGIC_NUMBER & 0xFF;
const uint8_t RHSP_MAGIC_NUMBER_BYTE_TWO = RHSP_MAGIC_NUMBER >> 8;
const uint8_t RHSP_BROADCAST_ADDRESS = 0xFF;
const uint8_t RHSP_CONTROLLER_ADDRESS = 0x00;

typedef enum : uint16_t {
    RHSP_COMMAND_INTERFACE_START_DEKA = 4096
} RHSP_COMMAND_INTERFACE_START;

typedef enum : uint16_t {
    //Common RHSP commands
    RHSP_COMMAND_ACK = 0x7f01,
    RHSP_COMMAND_NACK = 0x7f02,
    RHSP_COMMAND_GET_MODULE_STATUS = 0x7f03,
    RHSP_COMMAND_KEEP_ALIVE = 0x7f04,
    RHSP_COMMAND_FAIL_SAFE = 0x7f05,
    RHSP_COMMAND_SET_NEW_MODULE_ADDRESS = 0x7f06,
    RHSP_COMMAND_QUERY_INTERFACE = 0x7f07,
    RHSP_COMMAND_START_DOWNLOAD = 0x7f08,
    RHSP_COMMAND_DOWNLOAD_CHUNK = 0x7f09,
    RHSP_COMMAND_SET_MODULE_LED_COLOR = 0x7f0a,
    RHSP_COMMAND_GET_MODULE_LED_COLOR = 0x7f0b,
    RHSP_COMMAND_SET_MODULE_LED_PATTERN = 0x7f0c,
    RHSP_COMMAND_GET_MODULE_LED_PATTERN = 0x7f0d,
    RHSP_COMMAND_DEBUG_LOG_LEVEL = 0x7f0e,
    RHSP_COMMAND_DISCOVERY = 0x7f0f,

    //DEKA interface
    RHSP_COMMAND_DEKA_GET_BULK_INPUT_DATA = RHSP_COMMAND_INTERFACE_START_DEKA + 0x00,
    RHSP_COMMAND_DEKA_SET_SINGLE_DIO_OUTPUT = RHSP_COMMAND_INTERFACE_START_DEKA + 0x01,
    RHSP_COMMAND_DEKA_SET_ALL_DIO_OUTPUTS = RHSP_COMMAND_INTERFACE_START_DEKA + 0x02,
    RHSP_COMMAND_DEKA_SET_DIO_DIRECTION = RHSP_COMMAND_INTERFACE_START_DEKA + 0x03,
    RHSP_COMMAND_DEKA_GET_DIO_DIRECTION = RHSP_COMMAND_INTERFACE_START_DEKA + 0x04,
    RHSP_COMMAND_DEKA_GET_SINGLE_DIO_INPUT = RHSP_COMMAND_INTERFACE_START_DEKA + 0x05,
    RHSP_COMMAND_DEKA_GET_ALL_DIO_INPUTS = RHSP_COMMAND_INTERFACE_START_DEKA + 0x06,
    RHSP_COMMAND_DEKA_GET_ADC = RHSP_COMMAND_INTERFACE_START_DEKA + 0x07,
    RHSP_COMMAND_DEKA_SET_MOTOR_CHANNEL_MODE = RHSP_COMMAND_INTERFACE_START_DEKA + 0x08,
    RHSP_COMMAND_DEKA_GET_MOTOR_CHANNEL_MODE = RHSP_COMMAND_INTERFACE_START_DEKA + 0x09,
    RHSP_COMMAND_DEKA_SET_MOTOR_CHANNEL_ENABLE = RHSP_COMMAND_INTERFACE_START_DEKA + 0x0A,
    RHSP_COMMAND_DEKA_GET_MOTOR_CHANNEL_ENABLE = RHSP_COMMAND_INTERFACE_START_DEKA + 0x0B,
    RHSP_COMMAND_DEKA_SET_MOTOR_CHANNEL_CURRENT_ALERT_LEVEL = RHSP_COMMAND_INTERFACE_START_DEKA + 0x0C,
    RHSP_COMMAND_DEKA_GET_MOTOR_CHANNEL_CURRENT_ALERT_LEVEL = RHSP_COMMAND_INTERFACE_START_DEKA + 0x0D,
    RHSP_COMMAND_DEKA_RESET_MOTOR_ENCODER = RHSP_COMMAND_INTERFACE_START_DEKA + 0x0E,
    RHSP_COMMAND_DEKA_SET_MOTOR_CONSTANT_POWER = RHSP_COMMAND_INTERFACE_START_DEKA + 0x0F,
    RHSP_COMMAND_DEKA_GET_MOTOR_CONSTANT_POWER = RHSP_COMMAND_INTERFACE_START_DEKA + 0x10,
    RHSP_COMMAND_DEKA_SET_MOTOR_TARGET_VELOCITY = RHSP_COMMAND_INTERFACE_START_DEKA + 0x11,
    RHSP_COMMAND_DEKA_GET_MOTOR_TARGET_VELOCITY = RHSP_COMMAND_INTERFACE_START_DEKA + 0x12,
    RHSP_COMMAND_DEKA_SET_MOTOR_TARGET_POSITION = RHSP_COMMAND_INTERFACE_START_DEKA + 0x13,
    RHSP_COMMAND_DEKA_GET_MOTOR_TARGET_POSITION = RHSP_COMMAND_INTERFACE_START_DEKA + 0x14,
    RHSP_COMMAND_DEKA_IS_MOTOR_AT_TARGET = RHSP_COMMAND_INTERFACE_START_DEKA + 0x15,
    RHSP_COMMAND_DEKA_GET_MOTOR_ENCODER_POSITION = RHSP_COMMAND_INTERFACE_START_DEKA + 0x16,
    RHSP_COMMAND_DEKA_SET_MOTOR_PID_CONTROL_LOOP_COEFFICIENTS = RHSP_COMMAND_INTERFACE_START_DEKA + 0x17,
    RHSP_COMMAND_DEKA_GET_MOTOR_PID_CONTROL_LOOP_COEFFICIENTS = RHSP_COMMAND_INTERFACE_START_DEKA + 0x18,
    RHSP_COMMAND_DEKA_SET_PWM_CONFIGURATION = RHSP_COMMAND_INTERFACE_START_DEKA + 0x19,
    RHSP_COMMAND_DEKA_GET_PWM_CONFIGURATION = RHSP_COMMAND_INTERFACE_START_DEKA + 0x1A,
    RHSP_COMMAND_DEKA_SET_PWM_PULSE_WIDTH = RHSP_COMMAND_INTERFACE_START_DEKA + 0x1B,
    RHSP_COMMAND_DEKA_GET_PWM_PULSE_WIDTH = RHSP_COMMAND_INTERFACE_START_DEKA + 0x1C,
    RHSP_COMMAND_DEKA_SET_PWM_ENABLE = RHSP_COMMAND_INTERFACE_START_DEKA + 0x1D,
    RHSP_COMMAND_DEKA_GET_PWM_ENABLE = RHSP_COMMAND_INTERFACE_START_DEKA + 0x1E,
    RHSP_COMMAND_DEKA_SET_SERVO_CONFIGURATION = RHSP_COMMAND_INTERFACE_START_DEKA + 0x1F,
    RHSP_COMMAND_DEKA_GET_SERVO_CONFIGURATION = RHSP_COMMAND_INTERFACE_START_DEKA + 0x20,
    RHSP_COMMAND_DEKA_SET_SERVO_PULSE_WIDTH = RHSP_COMMAND_INTERFACE_START_DEKA + 0x21,
    RHSP_COMMAND_DEKA_GET_SERVO_PULSE_WIDTH = RHSP_COMMAND_INTERFACE_START_DEKA + 0x22,
    RHSP_COMMAND_DEKA_SET_SERVO_ENABLE = RHSP_COMMAND_INTERFACE_START_DEKA + 0x23,
    RHSP_COMMAND_DEKA_GET_SERVO_ENABLE = RHSP_COMMAND_INTERFACE_START_DEKA + 0x24,
    RHSP_COMMAND_DEKA_I2C_WRITE_SINGLE_BYTE = RHSP_COMMAND_INTERFACE_START_DEKA + 0x25,
    RHSP_COMMAND_DEKA_I2C_WRITE_MULTIPLE_BYTES = RHSP_COMMAND_INTERFACE_START_DEKA + 0x26,
    RHSP_COMMAND_DEKA_I2C_READ_SINGLE_BYTE = RHSP_COMMAND_INTERFACE_START_DEKA + 0x27,
    RHSP_COMMAND_DEKA_I2C_READ_MULTIPLE_BYTES = RHSP_COMMAND_INTERFACE_START_DEKA + 0x28,
    RHSP_COMMAND_DEKA_I2C_READ_STATUS_QUERY = RHSP_COMMAND_INTERFACE_START_DEKA + 0x29,
    RHSP_COMMAND_DEKA_I2C_WRITE_STATUS_QUERY = RHSP_COMMAND_INTERFACE_START_DEKA + 0x2A,
    RHSP_COMMAND_DEKA_I2C_CONFIGURE_CHANNEL = RHSP_COMMAND_INTERFACE_START_DEKA + 0x2B,
    RHSP_COMMAND_DEKA_PHONE_CHARGE_CONTROL = RHSP_COMMAND_INTERFACE_START_DEKA + 0x2C,
    RHSP_COMMAND_DEKA_PHONE_CHARGE_QUERY = RHSP_COMMAND_INTERFACE_START_DEKA + 0x2D,
    RHSP_COMMAND_DEKA_INJECT_DATA_LOG_HINT = RHSP_COMMAND_INTERFACE_START_DEKA + 0x2E,
    RHSP_COMMAND_DEKA_I2C_CONFIGURE_QUERY = RHSP_COMMAND_INTERFACE_START_DEKA + 0x2F,
    RHSP_COMMAND_DEKA_READ_VERSION_STRING = RHSP_COMMAND_INTERFACE_START_DEKA + 0x30,
    RHSP_COMMAND_DEKA_FTDI_RESET_CONTROL = RHSP_COMMAND_INTERFACE_START_DEKA + 0x31,
    RHSP_COMMAND_DEKA_FTDI_RESET_QUERY = RHSP_COMMAND_INTERFACE_START_DEKA + 0x32,
    RHSP_COMMAND_DEKA_SET_MOTOR_PIDF_CONTROL_LOOP_COEFFICIENTS = RHSP_COMMAND_INTERFACE_START_DEKA + 0x33,
    RHSP_COMMAND_DEKA_I2C_WRITE_READ_MULTIPLE_BYTES = RHSP_COMMAND_INTERFACE_START_DEKA + 0x34,
    RHSP_COMMAND_DEKA_GET_MOTOR_PIDF_CONTROL_LOOP_COEFFICIENTS = RHSP_COMMAND_INTERFACE_START_DEKA + 0x35,
    RHSP_COMMAND_DEKA_I2C_TRANSACTION = RHSP_COMMAND_INTERFACE_START_DEKA + 0x36,
    RHSP_COMMAND_DEKA_I2C_QUERY_TRANSACTION = RHSP_COMMAND_INTERFACE_START_DEKA + 0x37,
    RHSP_COMMAND_DEKA_SET_BULK_OUTPUT_DATA = RHSP_COMMAND_INTERFACE_START_DEKA + 0x38,
    RHSP_COMMAND_DEKA_READ_VERSION = RHSP_COMMAND_INTERFACE_START_DEKA + 0x39,
} RHSP_COMMAND;

typedef struct {
    const char * name;
    RHSP_COMMAND_INTERFACE_START interfaceStart;
    uint16_t commandCount;
} Interface;

const Interface interfaces[] = {
    {"DEKA", RHSP_COMMAND_INTERFACE_START_DEKA, 58}
};

const uint16_t interfaceCount = sizeof(interfaces) / sizeof(Interface);

union packetType {
    //Size one just to make the compiler happy
    uint8_t buffer[1];

    struct {
        uint16_t magicNumber;
        uint16_t packetSize;
        uint8_t destAddress;
        //TEST: Is the src address always zero or does the parent hub repeating packets change its address?
        uint8_t srcAddress;
        uint8_t messageNumber;
        uint8_t referenceNumber;
        RHSP_COMMAND command;
        uint8_t payload[];
        //Payload
        //CRC
    } decoded __attribute__((packed));
} * packet;

// void clearBuffer(void) {
//     for(int i = 0; i < RHSP_BUFFER_SIZE; i++) {
//         packet->buffer[i] = 0;
//     }
// }

uint8_t calcChecksum(void) {
    uint8_t sum = 0;
    for (uint32_t i = 0; i < packet->decoded.packetSize - 1; i++) {
        sum += packet->buffer[i];
    }
    return sum;
}

uint8_t getPacketChecksum(void) {
    return packet->buffer[packet->decoded.packetSize-1];
}

int8_t rhsp_init() {
    // clearBuffer();
    return 0;
}

//A negative code means nothing was sent. Positive (and zero) mean a packet was sent
typedef enum : int8_t {
    RHSP_PARSE_RESULT_SUCCESS = 0,
    RHSP_PARSE_RESULT_NOT_RHSP = -1,
    RHSP_PARSE_RESULT_INCORRECT_DEST = -2,
    RHSP_PARSE_RESULT_INVALID_CHECKSUM = -3,
    RHSP_PARSE_RESULT_INVALID_COMMAND = -4,
    RHSP_PARSE_RESULT_INVALID_PAYLOAD = 1,
} RHSP_PARSE_RESULT;

const char * toString_PARSE_RESULT(RHSP_PARSE_RESULT result) {
    switch (result) {
        case RHSP_PARSE_RESULT_SUCCESS:
            return "Packet parseing successful.";
        case RHSP_PARSE_RESULT_NOT_RHSP:
            return "Packet not RHSP (no magic number)";
        case RHSP_PARSE_RESULT_INCORRECT_DEST:
            return "Packet not bound for this hub";
        case RHSP_PARSE_RESULT_INVALID_CHECKSUM:
            return "Packet checksum incorrect";
        case RHSP_PARSE_RESULT_INVALID_COMMAND:
            return "Command be handled due to lack of handler or because command is unexpected in this circumstance";
        case RHSP_PARSE_RESULT_INVALID_PAYLOAD:
            return "Payload data is incorrect";
        };
    return "toString_PARSE_RESULT incomplete";
}

uint8_t streq(const char * s1, const char * s2) {
    for (uint16_t i = 0; 1; i++) {
        char c1 = s1[i];
        char c2 = s2[i];

        if(c1 != c2) {
            return 0;
        }

        if(c1 == 0 || c2 == 0) {
            break;
        }
    }

    return 1;
}

void printBuffer(void) {
    for(int i = 0; i < packet->decoded.packetSize; i++) {
        debugUART_printU8Hex(packet->buffer[i]);
        debugUART_printChar(' ');
    }
    debugUART_printChar('\n');
}

void sendPacket(void) {
    packet->decoded.packetSize += RHSP_PACKET_MIN_SIZE;
    packet->decoded.destAddress = packet->decoded.srcAddress;
    packet->decoded.srcAddress = rhspAddress;
    packet->decoded.referenceNumber = packet->decoded.messageNumber;
    packet->buffer[packet->decoded.packetSize-1] = calcChecksum();
    // debugUART_printString("Sending response: ");
    // printBuffer();
    // debugUART_printChar('\n');
    
    rhspUART_send(packet->buffer, packet->decoded.packetSize);
}

void sendReadPacket(void) {
    packet->decoded.command = packet->decoded.command | 0x8000;
    sendPacket();
}

void sendNACK(RHSP_NACK nackCode) {
    packet->decoded.command = RHSP_COMMAND_NACK;
    packet->decoded.packetSize = 1;
    packet->decoded.payload[0] = nackCode;

    packet->decoded.packetSize += RHSP_PACKET_MIN_SIZE;

    debugUART_printString("Sent nack: ");
    printBuffer();
    debugUART_printChar('\n');

    packet->decoded.packetSize -= RHSP_PACKET_MIN_SIZE;

    sendPacket();
}

volatile RHSP_MODULE_STATUS rhsp_moduleStatus = RHSP_MODULE_STATUS_DEVICE_RESET;
uint8_t motorAlerts = 0;

void sendACK(void) {
    packet->decoded.command = RHSP_COMMAND_ACK;
    packet->decoded.packetSize = 1;
    packet->decoded.payload[0] = rhsp_moduleStatus != 0 || motorAlerts != 0;

    sendPacket();
}

const char versionString[] = "HW: 20, Maj: 1, Min: 8, Eng: 2";
//Subtract 1 to skip null byte because it messes up the RC
const uint8_t versionStringLength = sizeof(versionString) / sizeof(char) - 1;

//The timer value can never be UINT32_MAX as it starts at one less than that, so if rhsp_lastCommandTime equals UINT32_MAX, then it must be timed out from reset
volatile uint32_t rhsp_lastCommandTime = UINT32_MAX;

uint8_t phoneChargingEnabled = 0;

RHSP_PARSE_RESULT handlePacket(void) {
    debugUART_printString("Packet received: ");
    printBuffer();

    //This probably isn't needed
    if(packet->decoded.magicNumber != RHSP_MAGIC_NUMBER) {
        return RHSP_PARSE_RESULT_NOT_RHSP;
    }

    if(getPacketChecksum() != calcChecksum()) {
        return RHSP_PARSE_RESULT_INVALID_CHECKSUM;
    }

    if(packet->decoded.destAddress != rhspAddress && packet->decoded.destAddress != RHSP_BROADCAST_ADDRESS) {
        //ADD: Pass along packets not bound for this hub
        return RHSP_PARSE_RESULT_INCORRECT_DEST;
    }

    rhsp_lastCommandTime = *timer_value;
    if(led_mode == LED_MODE_RESET || led_mode == LED_MODE_TIMEOUT) {
        led_setupPattern(LED_MODE_CONNECTED, &led_connectedPattern);
    }

    packet->decoded.packetSize -= RHSP_PACKET_MIN_SIZE;

    //FIX: Replace this massive switch statement with an array of commands. In the array have the index be the index into the command interface. Have a function pointer and an expected length of payload
    switch(packet->decoded.command) {
    //
    // Common RHSP Commands
    //

    case RHSP_COMMAND_GET_MODULE_STATUS:
        uint8_t resetStatus = 0;
        if(packet->decoded.packetSize != 1 || (resetStatus = packet->decoded.payload[0]) < 0 || resetStatus > 1) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            return RHSP_PARSE_RESULT_INVALID_PAYLOAD;
        }
         
        packet->decoded.packetSize = 2;
        packet->decoded.payload[0] = rhsp_moduleStatus;
        packet->decoded.payload[1] = motorAlerts;
        
        sendReadPacket();

        if(resetStatus) {
            //Don't allow resetStatus to reset battery low or controller over temperature
            rhsp_moduleStatus &= (RHSP_MODULE_STATUS_BATTERY_LOW | RHSP_MODULE_STATUS_CONTROLLER_OVER_TEMP);
            motorAlerts = 0;
        }

        break;

    case RHSP_COMMAND_KEEP_ALIVE:
        sendACK();
        break;

    case RHSP_COMMAND_SET_MODULE_LED_COLOR:
        if(packet->decoded.packetSize != 3) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            return RHSP_PARSE_RESULT_INVALID_PAYLOAD;
        }

        //We load the color into a pattern here because it saves the color for if a low battery state occurs
        led_userPattern.totalSteps = 1;
        led_userPattern.steps[0].tenths = 0;
        led_userPattern.steps[0].red = packet->decoded.payload[0];
        led_userPattern.steps[0].green = packet->decoded.payload[1];
        led_userPattern.steps[0].blue = packet->decoded.payload[2];
        led_setupPattern(LED_MODE_USER, &led_userPattern);
        led_dumpPattern();

        sendACK();
        break;

    case RHSP_COMMAND_GET_MODULE_LED_COLOR:
        if(packet->decoded.packetSize != 0) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            return RHSP_PARSE_RESULT_INVALID_PAYLOAD;
        }

        packet->decoded.packetSize = 3;
        led_getColor(&packet->decoded.payload[0], &packet->decoded.payload[1], &packet->decoded.payload[2]);
        sendReadPacket();
        break;

    case RHSP_COMMAND_SET_MODULE_LED_PATTERN:
        if(packet->decoded.packetSize % sizeof(led_PatternStep) != 0 & packet->decoded.packetSize < (sizeof(led_PatternStep) * LED_MAX_PATTERN_STEPS)) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
        }

        uint8_t i = 0;;
        for(; i < LED_MAX_PATTERN_STEPS; i++) {
            //I loves types in C. For some reason I can't cast a uint32_t to a led_PatternStep, but I can cast it when
            // they are both pointers, so we get this lovely mess.
            uint32_t * step = ((uint32_t *) (packet->decoded.payload)) + i;
            if(*step == 0) {
                break;
            }

            led_userPattern.steps[i] = *((led_PatternStep *) step);
        }

        led_userPattern.totalSteps = i;
        led_setupPattern(LED_MODE_USER, &led_userPattern);
        // led_dumpPattern();

        sendACK();
        break;

    case RHSP_COMMAND_GET_MODULE_LED_PATTERN:
        sendNACK(RHSP_NACK_COMMAND_IMPLEMENTATION_PENDING);
        break;

    case RHSP_COMMAND_FAIL_SAFE:
        if(packet->decoded.packetSize != 0) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            return RHSP_PARSE_RESULT_INVALID_PAYLOAD;
        }

        failsafe();
        sendACK();
        break;

    case RHSP_COMMAND_QUERY_INTERFACE:
        //Check that the last byte of the buffer is actually zero so that string functions won't go forever
        if(packet->decoded.payload[packet->decoded.packetSize - 1] != 0) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            break;
        }

        for(uint16_t i = 0; i < interfaceCount; i++) {
            if(streq((char *) packet->decoded.payload, interfaces[i].name)) {
                packet->decoded.packetSize = 4;
                *((uint16_t *)(packet->decoded.payload)) = interfaces[i].interfaceStart;
                *((uint16_t *)(packet->decoded.payload + 2)) = interfaces[i].commandCount;
            
                sendReadPacket();                

                goto success;
            }
        }

        sendNACK(RHSP_NACK_PARAM_0_WRONG);

        success:
        break;
    
    case RHSP_COMMAND_DISCOVERY:
        if(packet->decoded.packetSize != 0) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            return RHSP_PARSE_RESULT_INVALID_PAYLOAD;
        }
        
        packet->decoded.packetSize = 1;
        packet->decoded.payload[0] = 1;

        //ADD: Make the discovery command look for RS485 Children

        sendReadPacket();
        break;

    case RHSP_COMMAND_SET_NEW_MODULE_ADDRESS:
        if(packet->decoded.packetSize != 1) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            return RHSP_PARSE_RESULT_INVALID_PAYLOAD;
        }

        uint8_t newAddress = packet->decoded.payload[0];
        if(newAddress == RHSP_BROADCAST_ADDRESS || newAddress == RHSP_CONTROLLER_ADDRESS) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            return RHSP_PARSE_RESULT_INVALID_PAYLOAD;
        }

        if(rhspAddress != newAddress) {
            rhspAddress = newAddress;
            eeprom_setAddress(newAddress);
        }

        sendACK();
        break;

    case RHSP_COMMAND_NACK:
    case RHSP_COMMAND_ACK:
        //Never should be here because these are only responses, but this is what the stock firmware does
        sendACK();
        break;

    //These will never be implemented
    case RHSP_COMMAND_START_DOWNLOAD:
    case RHSP_COMMAND_DOWNLOAD_CHUNK:
    case RHSP_COMMAND_DEBUG_LOG_LEVEL:
        sendNACK(RHSP_NACK_COMMAND_IMPLEMENTATION_PENDING);
        break;

    //
    // DEKA Interface Commands
    //

    //ADD: Implement bulk read
    case RHSP_COMMAND_DEKA_GET_BULK_INPUT_DATA:
    case RHSP_COMMAND_DEKA_SET_BULK_OUTPUT_DATA:
        sendNACK(RHSP_NACK_COMMAND_IMPLEMENTATION_PENDING);
        break;

    case RHSP_COMMAND_DEKA_READ_VERSION_STRING:
    //FIX: Read version string gets naked
        if(packet->decoded.packetSize != 3) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            return RHSP_PARSE_RESULT_INVALID_PAYLOAD;
        }

        packet->decoded.packetSize = versionStringLength + 1;
        packet->decoded.payload[0] = versionStringLength;

        for (uint8_t i = 0; i < versionStringLength; i++) {
            packet->decoded.payload[i+1] = versionString[i];
        }

        sendReadPacket();
        break;
    
    case RHSP_COMMAND_DEKA_GET_ADC:
        if(packet->decoded.packetSize == 0) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            break;
        }

        uint8_t rawMode = packet->decoded.payload[1];
        if(packet->decoded.packetSize != 2 || (rawMode != 1 && rawMode != 0)) {
            sendNACK(RHSP_NACK_PARAM_1_WRONG);
            break;
        }

        ADC_CHANNEL adcChannel = packet->decoded.payload[0];
        if(adc_readMapped(adcChannel, ((int16_t *) packet->decoded.payload), rawMode)) {
            sendNACK(RHSP_NACK_COMMAND_IMPLEMENTATION_PENDING);
            break;
        }

        sendReadPacket();
        break;

    case RHSP_COMMAND_DEKA_SET_MOTOR_CHANNEL_MODE:
    case RHSP_COMMAND_DEKA_GET_MOTOR_CHANNEL_MODE:
        sendNACK(RHSP_NACK_COMMAND_IMPLEMENTATION_PENDING);
        break;

    case RHSP_COMMAND_DEKA_SET_MOTOR_CHANNEL_ENABLE:
        if(packet->decoded.packetSize != 2) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            break;
        }

        uint8_t enabled = packet->decoded.payload[1];
        //Make sure enabled is either 0 or 1. We don't have to check that enabled is less than 0 because it is unsigned.
        if(enabled > 1) {
            sendNACK(RHSP_NACK_PARAM_1_WRONG);
            break;
        } 

        // debugUART_printString("rhsp_moduleStatus: 0x");
        // debugUART_printU8Hex(rhsp_moduleStatus);
        // debugUART_printChar('\n');

        if(rhsp_moduleStatus & RHSP_MODULE_STATUS_BATTERY_LOW) {
            sendNACK(RHSP_NACK_MOTOR_CANT_RUN_BATTERY_TOO_LOW);
            break;
        }
        
        RHSP_NACK nackCode = motor_setEnabled(packet->decoded.payload[0], enabled);

        if(nackCode != RHSP_NACK_NO_NACK) {
            sendNACK(nackCode);
            break;
        }

        sendACK();
        break;

    case RHSP_COMMAND_DEKA_GET_MOTOR_CHANNEL_ENABLE:
        if(packet->decoded.packetSize != 1) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            break;
        }

        if(motor_getEnabled(packet->decoded.payload[0], &enabled) != 0) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            break;
        }

        packet->decoded.payload[0] = enabled;
        packet->decoded.packetSize = 1;

        sendReadPacket();
        break;

    case RHSP_COMMAND_DEKA_SET_MOTOR_CONSTANT_POWER:
        if(packet->decoded.packetSize != 3) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            break;
        }

        //The pointer spaghetti for the power field gets the int16_t value at an offset of 1 into the payload.
        if(motor_setPower(packet->decoded.payload[0], *(int16_t *)(&packet->decoded.payload[1])) != 0) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            break;
        }

        sendACK();
        break;
        
    case RHSP_COMMAND_DEKA_GET_MOTOR_CONSTANT_POWER:
        if(packet->decoded.packetSize != 1) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            break;
        }

        packet->decoded.packetSize = 2;

        if(motor_getPower(packet->decoded.payload[0], (int16_t *)(&packet->decoded.payload[0])) != 0) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            break;
        }
        
        sendReadPacket();
        break;

    case RHSP_COMMAND_DEKA_PHONE_CHARGE_CONTROL:
        if(packet->decoded.packetSize != 1) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            break;
        }

        uint8_t newPhoneChargeEnabled = packet->decoded.payload[0];
        //Booleans must be zero or one. Unsigned means nothing less than 0.
        if(newPhoneChargeEnabled > 1) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            break;
        }

        phoneChargingEnabled = newPhoneChargeEnabled;
        sendACK();
        break;
        
    case RHSP_COMMAND_DEKA_PHONE_CHARGE_QUERY:
        if(packet->decoded.packetSize != 0) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            break;
        }

        packet->decoded.packetSize = 1;
        packet->decoded.payload[0] = phoneChargingEnabled;
        sendReadPacket();
        break;

    case RHSP_COMMAND_DEKA_RESET_MOTOR_ENCODER:
        if(packet->decoded.packetSize != 1) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            break;
        }

        uint8_t encoderChannelReset = packet->decoded.payload[0];
        if(encoderChannelReset > 3) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            break;
        }
            
        if(encoderChannelReset == 0) {
            *REGISTER_QEI_0_QEIPOS = ENCODER_RESET_VALUE;
        } else if(encoderChannelReset == 3) {
            *REGISTER_QEI_1_QEIPOS = ENCODER_RESET_VALUE;
        }

        sendACK();
        break;

    case RHSP_COMMAND_DEKA_GET_MOTOR_ENCODER_POSITION:
        if(packet->decoded.packetSize != 1) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            break;
        }

        uint8_t encoderChannel = packet->decoded.payload[0];
        if(encoderChannel > 3) {
            sendNACK(RHSP_NACK_PARAM_0_WRONG);
            break;
        }

        packet->decoded.packetSize = 4;

        int32_t result = 0;
        if(encoderChannel == 0) {
            result = *REGISTER_QEI_0_QEIPOS - ENCODER_RESET_VALUE;

        //send dummy data for channels one and two as we don't support software encoders
        } else if(encoderChannel == 1 || encoderChannel == 2) {
            result = 0;
        } else if(encoderChannel == 3) {
            result = *REGISTER_QEI_1_QEIPOS - ENCODER_RESET_VALUE;
        }

        *((int32_t *)(packet->decoded.payload)) = result;

        sendReadPacket();
        break;

    case RHSP_COMMAND_DEKA_SET_SERVO_CONFIGURATION:
    case RHSP_COMMAND_DEKA_GET_SERVO_CONFIGURATION:
    case RHSP_COMMAND_DEKA_SET_SERVO_PULSE_WIDTH:
    case RHSP_COMMAND_DEKA_GET_SERVO_PULSE_WIDTH:
    case RHSP_COMMAND_DEKA_SET_SERVO_ENABLE:
    case RHSP_COMMAND_DEKA_GET_SERVO_ENABLE:
        sendNACK(RHSP_NACK_COMMAND_IMPLEMENTATION_PENDING);
        break;

    //These will never be implemented
    case RHSP_COMMAND_DEKA_SET_SINGLE_DIO_OUTPUT:
    case RHSP_COMMAND_DEKA_SET_ALL_DIO_OUTPUTS:
    case RHSP_COMMAND_DEKA_SET_DIO_DIRECTION:
    case RHSP_COMMAND_DEKA_GET_DIO_DIRECTION:
    case RHSP_COMMAND_DEKA_GET_SINGLE_DIO_INPUT:
    case RHSP_COMMAND_DEKA_GET_ALL_DIO_INPUTS:

    case RHSP_COMMAND_DEKA_SET_MOTOR_TARGET_VELOCITY:
    case RHSP_COMMAND_DEKA_GET_MOTOR_TARGET_VELOCITY:
    case RHSP_COMMAND_DEKA_SET_MOTOR_TARGET_POSITION:
    case RHSP_COMMAND_DEKA_GET_MOTOR_TARGET_POSITION:
    case RHSP_COMMAND_DEKA_IS_MOTOR_AT_TARGET:
    case RHSP_COMMAND_DEKA_SET_MOTOR_CHANNEL_CURRENT_ALERT_LEVEL:
    case RHSP_COMMAND_DEKA_GET_MOTOR_CHANNEL_CURRENT_ALERT_LEVEL:
    case RHSP_COMMAND_DEKA_SET_MOTOR_PID_CONTROL_LOOP_COEFFICIENTS:
    case RHSP_COMMAND_DEKA_GET_MOTOR_PID_CONTROL_LOOP_COEFFICIENTS:
    case RHSP_COMMAND_DEKA_SET_MOTOR_PIDF_CONTROL_LOOP_COEFFICIENTS:
    case RHSP_COMMAND_DEKA_GET_MOTOR_PIDF_CONTROL_LOOP_COEFFICIENTS:

    case RHSP_COMMAND_DEKA_SET_PWM_CONFIGURATION:
    case RHSP_COMMAND_DEKA_GET_PWM_CONFIGURATION:
    case RHSP_COMMAND_DEKA_SET_PWM_PULSE_WIDTH:
    case RHSP_COMMAND_DEKA_GET_PWM_PULSE_WIDTH:
    case RHSP_COMMAND_DEKA_SET_PWM_ENABLE:
    case RHSP_COMMAND_DEKA_GET_PWM_ENABLE:

    case RHSP_COMMAND_DEKA_I2C_WRITE_SINGLE_BYTE:
    case RHSP_COMMAND_DEKA_I2C_WRITE_MULTIPLE_BYTES:
    case RHSP_COMMAND_DEKA_I2C_READ_SINGLE_BYTE:
    case RHSP_COMMAND_DEKA_I2C_READ_MULTIPLE_BYTES:
    case RHSP_COMMAND_DEKA_I2C_READ_STATUS_QUERY:
    case RHSP_COMMAND_DEKA_I2C_WRITE_STATUS_QUERY:
    case RHSP_COMMAND_DEKA_I2C_CONFIGURE_CHANNEL:
    case RHSP_COMMAND_DEKA_I2C_CONFIGURE_QUERY:
    case RHSP_COMMAND_DEKA_I2C_WRITE_READ_MULTIPLE_BYTES:
    case RHSP_COMMAND_DEKA_I2C_TRANSACTION:
    case RHSP_COMMAND_DEKA_I2C_QUERY_TRANSACTION:

    case RHSP_COMMAND_DEKA_FTDI_RESET_CONTROL:
    case RHSP_COMMAND_DEKA_FTDI_RESET_QUERY:

    case RHSP_COMMAND_DEKA_READ_VERSION:

    case RHSP_COMMAND_DEKA_INJECT_DATA_LOG_HINT:
        sendNACK(RHSP_NACK_COMMAND_IMPLEMENTATION_PENDING);
        break;

    default:
        sendNACK(RHSP_NACK_UNKNOWN_COMMAND_ID);
        return RHSP_PARSE_RESULT_INVALID_COMMAND;
        
        break;
    }

    return RHSP_PARSE_RESULT_SUCCESS;
}

//How much data we have
uint16_t bytesReceived = 0;
//How far along the packet have we checked
uint8_t parseLoc = 0;

void relocatePacket() {
    for (uint16_t loc = 0; loc < bytesReceived; loc++) {
        rawBuffer[loc] = packet->buffer[loc];
    }

    packet = (union packetType *) rawBuffer;
}

//This is probably the fifth time I have tried to write this function, and this time I finally got something that
// is workable. Not great, but workable.
void rhsp_tick(uint8_t * location, uint16_t count) {
    // Increment bytesReceived by count when there is already the start of a packet
    if(parseLoc != 0) {
        bytesReceived += count;
    }

    uint16_t i = 0;

    while(1) {
        if(parseLoc == 0) {
            for (; i < count; i++) {
                packet = (union packetType *) (location + i);
                
                if(packet->buffer[0] == RHSP_MAGIC_NUMBER_BYTE_ONE) {
                    bytesReceived = count - i;
                    parseLoc = 1;
                    break;
                }
            }

            //If the for loop ends due to running out of data we are out of data and have no valid packet,
            // so receive into the start of the raw buffer
            if(parseLoc == 0) { 
                rhspUART_receive(rawBuffer, RHSP_PACKET_MIN_SIZE);
                break;
            }
        }

        if(parseLoc == 1 && bytesReceived >= 2) {
            if(packet->buffer[1] == RHSP_MAGIC_NUMBER_BYTE_TWO) {
                //Good packet, keep going 
                parseLoc = 2;

            } else {
                //ERROR. Restart parsing
                bytesReceived = 0;
                parseLoc = 0;
                i++;
                continue;
            }
        }

        if((parseLoc == 3 || parseLoc == 2) && bytesReceived >= 4) {
            if(packet->decoded.packetSize >= RHSP_PACKET_MIN_SIZE && packet->decoded.packetSize <= RHSP_PACKET_MAX_SIZE) {
                //Good packet, keep going
                parseLoc = 4;   
            } else {
                //ERROR. Restart parsing
                bytesReceived = 0;
                parseLoc = 0;
                i++;
                continue;
            }
        }

        if(parseLoc >= 4 && bytesReceived >= 4) {
            uint16_t bytesLeft = packet->decoded.packetSize - bytesReceived;
            if(bytesLeft > 0) {
                if((((uint8_t *) packet) + bytesReceived + bytesLeft) > rawBufferEnd) {
                    relocatePacket();
                }

                rhspUART_receive(((uint8_t *) packet)+bytesReceived, bytesLeft);
                break;
            } 

            RHSP_PARSE_RESULT result = handlePacket();
            if(result != RHSP_PARSE_RESULT_SUCCESS) {
                debugUART_printStringConst(toString_PARSE_RESULT(result));
                debugUART_printString("\n");
            }

            bytesReceived = 0;
            parseLoc = 0;

            if(result < 0) {
                rhspUART_receive(rawBuffer, RHSP_PACKET_MIN_SIZE);
            }

            break;
        }

        //If we get here we need more data, but we don't know exactly how much
        if(((uint8_t *) packet) > (rawBuffer + RHSP_PACKET_MIN_SIZE)) {
            relocatePacket();
        }

        rhspUART_receive(((uint8_t *) packet)+bytesReceived, RHSP_PACKET_MIN_SIZE - bytesReceived);
        break;
    }
}
