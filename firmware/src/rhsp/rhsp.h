#ifndef RHSP_H
#define RHSP_H

#include <stdint.h>
#include "hardware/timer.h"

//Use an enum because for some reason consts aren't actually read only and this needs to size an array, but enums are so ...
enum : uint16_t {
    RHSP_PAYLOAD_MAX_SIZE = 512,
    RHSP_HEADER_SIZE = 10,
    RHSP_CHECKSUM_SIZE = 1,    
    RHSP_PACKET_MIN_SIZE = RHSP_HEADER_SIZE + RHSP_CHECKSUM_SIZE,
    RHSP_PACKET_MAX_SIZE = RHSP_PAYLOAD_MAX_SIZE + RHSP_PACKET_MIN_SIZE,
    //The buffer size has an extra RHSP_PACKET_MIN_SIZE - 1 tacked onto the end so that if the last 
    // byte of waiting-for-packet RHSP_PACKET_MIN_SIZE byte dma transfer is the start of a packet 
    // and it is the max length that we support there will be enough room in the buffer for it
    RHSP_BUFFER_SIZE = RHSP_PACKET_MAX_SIZE + RHSP_PACKET_MIN_SIZE - 1
};

typedef enum : uint8_t {
    RHSP_MODULE_STATUS_KEEP_ALIVE_TIMEOUT    = 0b000001,
    RHSP_MODULE_STATUS_DEVICE_RESET          = 0b000010,
    RHSP_MODULE_STATUS_FAIL_SAFE             = 0b000100,
    //ADD: Check for controller over temp
    RHSP_MODULE_STATUS_CONTROLLER_OVER_TEMP  = 0b001000,
    RHSP_MODULE_STATUS_BATTERY_LOW           = 0b010000,
    RHSP_MODULE_STATUS_HIB_FAULT             = 0b100000,
} RHSP_MODULE_STATUS;
volatile extern RHSP_MODULE_STATUS rhsp_moduleStatus;

typedef enum : uint8_t {
    // 0-9 are for params
    RHSP_NACK_PARAM_0_WRONG = 0,
    RHSP_NACK_PARAM_1_WRONG = 1,
    RHSP_NACK_PARAM_2_WRONG = 2,
    RHSP_NACK_PARAM_3_WRONG = 3,
    RHSP_NACK_PARAM_4_WRONG = 4,
    RHSP_NACK_PARAM_5_WRONG = 5,
    RHSP_NACK_PARAM_6_WRONG = 6,
    RHSP_NACK_PARAM_7_WRONG = 7,
    RHSP_NACK_PARAM_8_WRONG = 8,
    RHSP_NACK_PARAM_9_WRONG = 9,

    // 10-19 are for GPIO output
    RHSP_NACK_GPIO_0_NOT_OUTPUT = 10,
    RHSP_NACK_GPIO_1_NOT_OUTPUT = 11,
    RHSP_NACK_GPIO_2_NOT_OUTPUT = 12,
    RHSP_NACK_GPIO_3_NOT_OUTPUT = 13,
    RHSP_NACK_GPIO_4_NOT_OUTPUT = 14,
    RHSP_NACK_GPIO_5_NOT_OUTPUT = 15,
    RHSP_NACK_GPIO_6_NOT_OUTPUT = 16,
    RHSP_NACK_GPIO_7_NOT_OUTPUT = 17,
    RHSP_NACK_GPIO_ALL_NOT_OUTPUT = 18,
    // NACK 19 is reserved

    // 20-29 are for GPIO input
    RHSP_NACK_GPIO_0_NOT_INPUT = 20,
    RHSP_NACK_GPIO_1_NOT_INPUT = 21,
    RHSP_NACK_GPIO_2_NOT_INPUT = 22,
    RHSP_NACK_GPIO_3_NOT_INPUT = 23,
    RHSP_NACK_GPIO_4_NOT_INPUT = 24,
    RHSP_NACK_GPIO_5_NOT_INPUT = 25,
    RHSP_NACK_GPIO_6_NOT_INPUT = 26,
    RHSP_NACK_GPIO_7_NOT_INPUT = 27,
    RHSP_NACK_GPIO_ALL_NOT_INPUT = 28,
    // NACK 29 is reserved

    // 30-39 are for servos
    RHSP_NACK_SERVO_NOT_CONFIGURED_BEFORE_ENABLE = 30,
    RHSP_NACK_SERVO_CANT_RUN_BATTERY_TOO_LOW = 31,
    // NACKs 32-39 are reserved

    // 40-49 are for i2c
    RHSP_NACK_I2C_MASTER_BUSY = 40,
    RHSP_NACK_I2C_PENDING = 41,
    RHSP_NACK_I2C_NOTHING_PENDING = 42,
    RHSP_NACK_I2C_QUERY_MISMATCH = 43,
    RHSP_NACK_I2C_TIMEOUT_SDA_STUCK = 44,
    RHSP_NACK_I2C_TIMEOUT_SCL_STUCK = 45,
    RHSP_NACK_I2C_TIMEOUT = 46,
    // NACKs 47-49 are reserved

    // 50-59 are for motors
    RHSP_NACK_MOTOR_NOT_CONFIGURED_BEFORE_ENABLE = 50,
    RHSP_NACK_MOTOR_INVALID_COMMAND_FOR_MODE = 51,
    RHSP_NACK_MOTOR_CANT_RUN_BATTERY_TOO_LOW = 52,
    // NACKs 53-59 are reserved

    // Never send this!!!. Used ot indicate success internally when RHSP_NACK is used as a return code
    RHSP_NACK_NO_NACK = 250,

    //There are a few codes at the end for debugging
    RHSP_NACK_COMMAND_IMPLEMENTATION_PENDING = 253,
    RHSP_NACK_COMMAND_ROUTING_ERROR = 254,
    RHSP_NACK_UNKNOWN_COMMAND_ID = 255
} RHSP_NACK;

static const uint32_t RHSP_TIMEOUT = TIMER_TICKS_PER_100MS * 25;

int8_t rhsp_init(void);
void rhsp_tick(uint8_t * location, uint16_t count);

volatile extern uint32_t rhsp_lastCommandTime;

#endif
