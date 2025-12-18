#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <libftdi1/ftdi.h>
#include <string.h>
#include <byteswap.h>

void sleepMS(unsigned int msecs);

#ifdef _WIN32
//  For Windows (32- and 64-bit)
#   include <windows.h>
    void sleepMS(unsigned int msecs) {
        Sleep(msecs);
    }

    #define __unix 0
    // #define errno 0
    #define strerror(x) ""
#elif __unix
//  For linux, OSX, and other unixes
#   include <time.h>
    void sleepMS(unsigned int msecs) {
        struct timespec ts;
        ts.tv_sec = msecs / 1000;
        ts.tv_nsec = (msecs % 1000) * 1000000;
        nanosleep(&ts, NULL); 
    }
    #include <errno.h>
#else
#   error "Unknown system"
#endif

uint8_t stop = 0;
struct ftdi_context *ftdi = 0;
uint8_t open = 0;

void exitHandler(int num) {
    // printf("Exit Handler\n");
    if(num == SIGINT) {
        if(!stop) {
            stop = 1;
            
            if(open) {
                // printf("disabling bitbang mode\n");
                int f = 0;

                if(ftdi->bitbang_enabled) {
                    f = ftdi_disable_bitbang(ftdi);
                    if (f < 0) {
                        printf("Disabling bitbang failed with error %d (%s)\n", f, ftdi_get_error_string(ftdi));
                    }
                }

                f = ftdi_usb_close(ftdi);

                if (f < 0) {
                    printf("Closing FTDI failed with error %d (%s)\n", f, ftdi_get_error_string(ftdi));
                }
            }

            if(ftdi != 0) {
                ftdi_free(ftdi);
            }

            exit(0);
        }
    }
}
 
// Upper nibble is mask, lower is on and off. The pins are active low
//RST is on CBUS0.
const uint8_t RST_MASK = 0b00010000;
//BOOTLOADER is on CBUS0
const uint8_t BOOTLOADER_MASK = 0b00100000;
const uint8_t AUTOBAUD_PATTERN[] = {0x55, 0x55};

enum : uint8_t {
    PACKET_HEADER_LENGTH = 2,
    PACKET_MAX_LENGTH = UINT8_MAX,
    PACKET_MAX_PAYLOAD_LENGTH = PACKET_MAX_LENGTH - PACKET_HEADER_LENGTH,
    PACKET_MIN_PAYLOAD_LENGTH = 1,
    PACKET_MIN_SIZE = PACKET_HEADER_LENGTH + PACKET_MIN_PAYLOAD_LENGTH,

    COMMAND_MAX_PAYLOAD_LENGTH = PACKET_MAX_PAYLOAD_LENGTH - 1
};

uint8_t outBuffer[PACKET_MAX_LENGTH];

enum COMMAND : uint8_t {
    COMMAND_PING = 0x20,
    COMMAND_DOWNLOAD = 0x21,
    COMMAND_RUN = 0x22,
    COMMAND_GET_STATUS = 0x23,
    COMMAND_SEND_DATA = 0x24,
    COMMAND_RESET = 0x25,
};

struct  __attribute__((packed)) CommandPayload {
    enum COMMAND command;
    union {
        uint8_t payload[COMMAND_MAX_PAYLOAD_LENGTH];
        struct  __attribute__((packed)) {
            uint32_t addr;
            uint32_t size;
        } downloadPayload;
        uint32_t runAddr;
    };
};

struct  __attribute__((packed)) Packet {
    uint8_t size;
    uint8_t checksum;    
    union {
        uint8_t payload[PACKET_MAX_PAYLOAD_LENGTH];
        struct CommandPayload commandPayload;
    };
    
};

struct Packet * const packet = (struct Packet *) &outBuffer;

int serialWrite(uint8_t * buffer, size_t length) {
    // printf("Serial write %zu bytes: ", length);
    // for(int i = 0; i < length; i++)  {
    //     printf("%x ", buffer[i]);
    // }
    // puts("\n");
    
    int f = ftdi_write_data(ftdi, buffer, length);
    if (f < 0) {
        printf("Writing message failed with error %d (%s)\n", f, ftdi_get_error_string(ftdi));
        return 1;
    }

    return 0;
}

int sendPacket() {
    if(packet->size < PACKET_MIN_PAYLOAD_LENGTH) {
        return 1;
    }

    packet->checksum = 0;
    for(uint8_t i = 0; i < packet->size; i++) {
        packet->checksum += packet->payload[i];
    }

    // printf("Packet payload %u bytes: ", packet->size);
    // for(int i = 0; i < packet->size; i++)  {
    //     printf("%x ", packet->payload[i]);
    // }
    // puts("\n");

    packet->size += PACKET_HEADER_LENGTH;

    if(serialWrite((uint8_t *) packet, packet->size)) {
        return 1;
    }

    packet->size -= PACKET_HEADER_LENGTH;

    return 0;
}

enum RESPONSE_TYPE_1 : uint8_t {
    RESPONSE_ACK = 0xCC,
    RESPONSE_NCK = 0x33,
};

enum RESPONSE_TYPE_2_CODE : uint8_t {
    RESPONSE_TYPE_2_SUCCESS = 0x40,
    RESPONSE_TYPE_2_UNKNOWN_COMMAND = 0x41,
    RESPONSE_TYPE_2_INVALID_COMMAND = 0x42,
    RESPONSE_TYPE_2_INVALID_ADDRESS = 0x43,
    RESPONSE_TYPE_2_FLASH_FAIL = 0x44,
    RESPONSE_TYPE_2_CRC_FAIL = 0x45,
};

const char * const RESPONSE_TYPE_2_CODE_toString(enum RESPONSE_TYPE_2_CODE code) {
    switch (code) {
    case RESPONSE_TYPE_2_SUCCESS:
        return "success";
    case RESPONSE_TYPE_2_UNKNOWN_COMMAND:
        return "unknown command";
    case RESPONSE_TYPE_2_INVALID_COMMAND:
        return "invalid command";
    case RESPONSE_TYPE_2_INVALID_ADDRESS:
        return "invalid address";
    case RESPONSE_TYPE_2_FLASH_FAIL:
        return "flash fail";
    case RESPONSE_TYPE_2_CRC_FAIL:
        return "CRC fail";
      break;
    }

    return "";
}

struct __attribute__((packed)) ResponseType2 {
    uint8_t size;
    uint8_t checksum;    
    enum RESPONSE_TYPE_2_CODE code;
};

union {
    uint8_t buffer[sizeof(struct ResponseType2)];
    enum RESPONSE_TYPE_1 responseType1;
    struct ResponseType2 responseType2;
} inBuffer;

enum : uint8_t {
    RESPONSE_TYPE_1_LENGTH = sizeof(enum RESPONSE_TYPE_1),
    RESPONSE_TYPE_2_LENGTH = sizeof(struct ResponseType2),
};

// This blocks for data
//ADD: Some sort of timeout
int serialRead(uint8_t * location, uint8_t count) {
    // printf("SerialRead for %u\n", count);
    uint8_t left = count;
    while(1) {
        // printf("Trying to read in %u\n", left);
        int f = ftdi_read_data(ftdi, inBuffer.buffer + (count - left), left);
        if (f < 0) {
            printf("Reading in failed with error %d (%s)\n", f, ftdi_get_error_string(ftdi));
            return 1;
        }

        left -= f;
        if(left == 0) {
            break;
        }

        sleepMS(10);
    }

    // puts("");

    return 0;
}

int dataRead(uint8_t amount) {
    if(amount > PACKET_MIN_SIZE) {
        return 1;
    }
    
    if(serialRead(inBuffer.buffer, amount)) {
        return 1;
    }

    //Deal with the potential of random leading zeros
    //Count the zeros
    uint8_t zeros = 0;
    for(; zeros < amount; zeros++) {
        if(inBuffer.buffer[zeros] != 0) {
            break;
        }
    }

    //We have zeros
    if(zeros != 0) {
        //If there are any move the data over in the buffer
        for(uint8_t i = 0; i < (amount - zeros); i++) {
            inBuffer.buffer[i] = inBuffer.buffer[i + zeros];
        }

        // printf("Zeroread: %u\n", zeros);
        if(serialRead(inBuffer.buffer, zeros)) {
            return 1;
        }
    }
    
    return 0;
}

int checkType1Respose(const char * location) {
    if(dataRead(RESPONSE_TYPE_1_LENGTH)) {
        return 1;
    }

    if(inBuffer.responseType1 == RESPONSE_NCK) {
        printf("ERROR: Nacked on %s command\n", location);
        return 1;
    } else if(inBuffer.responseType1 != RESPONSE_ACK) {
        printf("ERROR: Unknown response to %s command: 0x%x\n", location, inBuffer.responseType1);
        return 1;
    } else {
        // printf("Good type 1 response at \"%s\"\n", location);
    }

    return 0;
}

int sendPacketGetResponse(const char * location) {
    // puts("Send packet");
    if(sendPacket()) {
        return 1;
        printf("Send packet error at \"%s\"\n", location);
    }  
    
    // puts("Check type 1");
    if(checkType1Respose(location)) {
        return 1;
    }

    // sleepMS(50);

    // puts("Get status");
    packet->commandPayload.command = COMMAND_GET_STATUS;
    packet->size = 1;
    if(sendPacket()) {
        exitHandler(SIGINT);
        return 1;
    }

    // puts("Check type 1 get status");
    if(checkType1Respose(location)) {
        return 1;
    }

    if(dataRead(RESPONSE_TYPE_2_LENGTH)) {
        return 1;
    }

    //Check the checksum. There is only one byte to checksum, so the checksum byte should equal that byte
    if(inBuffer.responseType2.checksum == inBuffer.responseType2.code) {  
        inBuffer.responseType1 = RESPONSE_ACK;
    } else {
        inBuffer.responseType1 = RESPONSE_NCK;
        printf("Sending NCK to get status when doing \"%s\" command.\n", location);
        return 1;  
    }

    serialWrite(inBuffer.buffer, RESPONSE_TYPE_1_LENGTH);

    if(inBuffer.responseType2.code != RESPONSE_TYPE_2_SUCCESS) {
        printf("Got non-success status code %x which means \"%s\" while doing \"%s\" command.\n", inBuffer.responseType2.code, RESPONSE_TYPE_2_CODE_toString(inBuffer.responseType2.code), location);
        return 1;
    } else {
        // printf("Success at \"%s\"\n", location);
    }

    return 0;
}

void printUsage(char * argV0) {
    //ADD: GPLv3 Message
fputs("DuckUpdate. An updater for Lynx firmware and a replacement to LMFlashProgrammer from TI\n\nUsage: ", stdout);
fputs(argV0, stdout);
    puts(
" [action] <required argument> <optional argument>\n\
Actions:\n\
    reset - Reset the MCU using the reset line connected to the FTDI CBUS\n\
    reset_soft - Reset MCU in software with bootloader. This doesn't work on the Lynx\n\
    bootloader - Reset the MCU, enter the bootloader, and perform autobaud negotiation\n\
    write - Write the provided file to the MCU. The required argument is filename\n\
    run - Start execution at the provided address. The required argument is address\n\
    flash - Enter booloader, write, and reset - The required argument is the filename to write\n\
\n\
Write and Flash options:\n\
    -a=<address> --address=<address>. Address to write to\n\
    \n\
Provided addresses should be in the format 0x<addr> from 0x00000000 to 0xFFFFFFFF. The hex character must be uppercase. If they are outside the allowable range a NAK will be received from the MCU and reported.\n"
    );
}

enum ACTION : uint8_t{
    ACTION_RESET      = 0b000001,
    ACTION_RESET_SOFT = 0b000010,
    ACTION_BOOTLOADER = 0b000100,
    ACTION_WRITE      = 0b001000,
    ACTION_RUN        = 0b010000,
    ACTION_FLASH      = ACTION_BOOTLOADER | ACTION_WRITE | ACTION_RESET,
};

uint8_t shouldRunAction(enum ACTION targetAction, enum ACTION realAction) {
    return (realAction & targetAction) > 0;
}

int checkSysFunc(int retVal, const char * location) {
    // printf("Retval: %i\n", retVal);
    if(retVal) {
        if (__unix) {
            printf("Error #%i meaning \"%s\" at \"%s\"\n", errno, strerror(errno), location);
        } else {
            printf("Error at \"%s\"\n", location);
        }
    }

    return retVal;
}

//This was easier than trying to figure out strtoul
int stringToHex(const char * s, uint32_t * value) {
    size_t len = strlen(s);
    if(len > sizeof(value) * 2) {
        printf("ERROR: Address \"0x%s\" too long. Max 8 hex characters\n", s);
        return 1;
    } else if (len == 0) {
        printf("ERROR: Address has no characters in it\n");
        return 1;
    }

    for(uint16_t i = 0; i < len; i++ ) {
        if(!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'A' && s[i] <= 'F'))) {
            puts("ERROR: Non hex character found");
            return 1;
        }
    }

    char sub;
    for(uint8_t i = 0; i < len; i++) {
        char c = s[i];
        sub = (s[i] >= '0' && s[i] <= '9') ? 0x30 : 0x37;

        *value <<= 4;
        *value |= (c - sub);
    }

    return 0;
}

int main(int argc, char **argv) {
    puts("DuckUpdate Copyright (C) 2025 Bryn 'bakl' Hakl");
    if(argc == 0) {
        // Never should happen
        puts("ERROR: argc == 0");
        return 1;
    }

    if(argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    enum ACTION action = 0;

    const char * actionStr = argv[1];
    if(strcmp(actionStr, "reset") == 0) {
        action = ACTION_RESET;

    } else if (strcmp(actionStr, "reset_soft") == 0) {
        action = ACTION_RESET_SOFT;
        
    } else if (strcmp(actionStr, "bootloader") == 0) {
        action = ACTION_BOOTLOADER;
        
    } else if (strcmp(actionStr, "write") == 0) {
        action = ACTION_WRITE;
        
    } else if (strcmp(actionStr, "Run") == 0) {
        action = ACTION_RUN;
        
    } else if (strcmp(actionStr, "flash") == 0) {
        action = ACTION_FLASH;
        
    } else {
        printUsage(argv[0]);
        printf("ERROR: Unknown action \"%s\"\n", actionStr);
        return 1;
    }

    //Option handling
    uint32_t addr = 0;
    if(shouldRunAction(ACTION_WRITE | ACTION_RUN, action)) {
        if(argc < 3) {
            printf("ERROR: Too few arguments. Need at least two for action \"%s\"\n", actionStr);
            return 1;
        }

        const char * hexString = 0;

        if(shouldRunAction(ACTION_RUN, action)) {
             if(argc > 3) {
                printf("ERROR: Too many arguments. Max is two for action \"%s\"\n", actionStr);
                return 1;
            }

            const char starter[] = "0x";
            const char * opt = argv[2];
            if(strncmp(starter, opt, sizeof(starter) - 1) != 0) {
                printf("ERROR: Invalid address \"%s\"\n", opt);
                return 1;
            }

            hexString = opt + sizeof(starter) - 1;
        } else if(shouldRunAction(ACTION_WRITE, action) && argc >= 4) {
            if(argc > 4) {
                printf("ERROR: Too many arguments. Max is three for action \"%s\"\n", actionStr);
                return 1;
            }

            const char * opt = argv[3];
            const char shortArg[] = "-a=0x";
            const char longArg[] = "--address=0x";

            if(strncmp(shortArg, opt, sizeof(shortArg) - 1) == 0) {
                hexString = opt + sizeof(shortArg) - 1;
            } else if (strncmp(longArg, opt, sizeof(longArg) - 1) == 0) {
                hexString = opt + sizeof(longArg) - 1;
            } else {
                printf("ERROR: Invalid option \"%s\"\n", opt);
                return 1;
            }
        }

        if(hexString == 0) {
            addr = 0;
        } else if(stringToHex(hexString, &addr)) {
            printf("ERROR: Invalid address \"0x%s\"\n", hexString);
            return 1;
        }

    } else if(argc > 2) {
        printf("ERROR: Too many arguments. Max is one for action \"%s\"\n", actionStr);
        return 1;
    }

    printf("Running action \"%s\"\n\n", actionStr);
    // printf("Addr: %x\n", addr);

    //We never call fclose() because the file will be closed on exit
    FILE *file = 0;
    uint32_t fileSize = 0;
    const char * fileName = 0;

    if(shouldRunAction(ACTION_WRITE, action)) {
        fileName = argv[2];
        file = fopen(fileName, "r");
        if(file == NULL) {
            printf("ERROR: Reading file \"%s\"\n", fileName);
            checkSysFunc(-1, "opening file");
            return 1;
        }

        if(checkSysFunc(fseek(file, 0L, SEEK_END), "seeking file to end")) {
            return 1;
        }
        
        long fileSizeLong = ftell(file);

        if(checkSysFunc(fileSizeLong == -1, "finding file length")) {
            return 1;
        }
        
        rewind(file);

        if(fileSizeLong > UINT32_MAX) {
            printf("File \"%s\" too long. Max 0xFFFFFFFF bytes\n", fileName);
            return 1;
        }

        fileSize = fileSizeLong;
    }

    int f;
    signal(SIGINT, exitHandler);
    
    //ADD: Support for non FTDI serial ports
    ftdi = ftdi_new();
    if (ftdi == 0) {
        printf("ftdi_new failed\n");
        return 1;
    }

    ftdi->module_detach_mode = AUTO_DETACH_REATACH_SIO_MODULE;

    //ADD: Support for opening other FTDI devices not just the 230X with this VID and PID
    f = ftdi_usb_open(ftdi, 0x0403, 0x6015);
    if (f < 0) {
        printf("Unable to open FTDI device: %d (%s)\n", f, ftdi_get_error_string(ftdi));
        exitHandler(SIGINT);
        return 1;
    }

    open = 1;
    puts("FTDI open succeeded");

    // puts("Setting baudrate");
    //Baud rate is divided by four because if bitbang mode is on libftdi1 multiplies the given baud rate by four.
    // For the Lynx MCU the max baud rate is 500000 because the max baud rate is the clock / 32 and the bootloader 
    // clock of 16 MHz / 32 = 500000. We run at 490000 to have some margin in case the clock in the MCU is a bit slow
    //ADD: Option to set baudrate
    f = ftdi_set_baudrate(ftdi, 490000);
    if (f < 0) {
        printf("Setting baudrate failed with error %d (%s)\n", f, ftdi_get_error_string(ftdi));
        exitHandler(SIGINT);
        return 1;
    }

    // puts("Setting line properties");
    f = ftdi_set_line_property(ftdi, 8, STOP_BIT_1, NONE);
    if (f < 0) {
        printf("Setting line properties with error %d (%s)\n", f, ftdi_get_error_string(ftdi));
        exitHandler(SIGINT);
        return 1;
    }

    if(shouldRunAction(ACTION_BOOTLOADER, action)) {
        puts("Entering bootloader");
        f = ftdi_set_bitmode(ftdi, RST_MASK | BOOTLOADER_MASK, BITMODE_CBUS);
        if (f < 0) {
            printf("Setting bitmode to RST_MASK | BOOTLOADER_MASK failed with error %d (%s)\n", f, ftdi_get_error_string(ftdi));
            exitHandler(SIGINT);
            return 1;
        }

        sleepMS(50);

        f = ftdi_set_bitmode(ftdi, BOOTLOADER_MASK, BITMODE_CBUS);
        if (f < 0) {
            printf("Setting bitmode to BOOTLOADER_MASK failed with error %d (%s)\n", f, ftdi_get_error_string(ftdi));
            exitHandler(SIGINT);
            return 1;
        }

        sleepMS(50);

        f = ftdi_set_bitmode(ftdi, 0, BITMODE_CBUS);
        if (f < 0) {
            printf("Setting bitmode to 0 failed with error %d (%s)\n", f, ftdi_get_error_string(ftdi));
            exitHandler(SIGINT);
            return 1;
        }

        sleepMS(50);

        //Disable bitbang mode because it messes up the baud rate and this is easier than doing math 
        // f = ftdi_set_bitmode(ftdi, 0, BITMODE_RESET);
        f = ftdi_disable_bitbang(ftdi);
        if (f < 0) {
            printf("Disabling bitbang failed with error %d (%s)\n", f, ftdi_get_error_string(ftdi));
            exitHandler(SIGINT);
            return 1;
        }

        sleepMS(50);

        //TEST: What happens when a second autobaud packet is sent?
        // puts("Autobaud");
        //Try to autobaud and establish connection
        if(serialWrite(AUTOBAUD_PATTERN, sizeof(AUTOBAUD_PATTERN))) {
            exitHandler(SIGINT);
            return 1;
        }

        if(checkType1Respose("Autobaud response")) {
            return 1;
        }
        puts("Autobaud success");
        
        packet->commandPayload.command = COMMAND_PING;
        packet->size = 1;
        if(sendPacketGetResponse("ping")) {
            exitHandler(SIGINT);
            return 1;
        }
        puts("Ping Success\nBootloader entry success");
    }

    if(shouldRunAction(ACTION_WRITE, action)) {
        printf("Writing %u bytes to 0x%X from \"%s\"\n", fileSize, addr, fileName);
        packet->commandPayload.downloadPayload.addr = __bswap_constant_32(addr);
        packet->commandPayload.downloadPayload.size = __bswap_constant_32(fileSize);
        packet->commandPayload.command = COMMAND_DOWNLOAD;
        packet->size = 9;
        if(sendPacketGetResponse("download")) {
            exitHandler(SIGINT);
            return 1;
        }
        puts("Download Packet successful\nStarting to write file");

        while (!feof(file)) {
            //ADD: File write progress indicator
            size_t bytesRead = fread(packet->commandPayload.payload, 1, COMMAND_MAX_PAYLOAD_LENGTH, file);
            if(bytesRead != COMMAND_MAX_PAYLOAD_LENGTH && checkSysFunc(ferror(file), "read file data")) {
                exitHandler(1);
                return 1;
            }

            packet->commandPayload.command = COMMAND_SEND_DATA;
            packet->size = bytesRead + 1;
            if(sendPacketGetResponse("send data")) {
                exitHandler(SIGINT);
                return 1;
            }

            if(stop) {
                puts("Stopping before the file is fully written");
                return 0;
            }
        }
        
        puts("File written successfully");
    }

    if(shouldRunAction(ACTION_RUN | ACTION_RESET_SOFT, action)) {
        const char * location = 0;
        if(shouldRunAction(ACTION_RUN, action)) {
            packet->commandPayload.command = COMMAND_RUN;
            packet->commandPayload.runAddr = __bswap_constant_32(addr);
            packet->size = 1 + sizeof(addr);
            location = "Run";
            printf("Runing at 0x%X\n", addr);
        } else if(shouldRunAction(ACTION_RESET_SOFT, action)) {
            packet->commandPayload.command = COMMAND_RESET;
            packet->size = 1;
            location = "Software reset";
        }

        printf("%s started\n", location);

        if(sendPacket()) {
            printf("ERROR: %s packet transmission\n", location);
            exitHandler(SIGINT);
            return 1;
        }  
        
        if(checkType1Respose(location)) {
            exitHandler(SIGINT);
            return 1;
        }

        printf("%s completed successfully\n", location);
        
    } else if(shouldRunAction(ACTION_RESET, action)) {
        puts("Resetting MCU");

        f = ftdi_set_bitmode(ftdi, RST_MASK, BITMODE_CBUS);
        if (f < 0) {
            printf("Setting bitmode to RST_MASK | BOOTLOADER_MASK failed with error %d (%s)\n", f, ftdi_get_error_string(ftdi));
            exitHandler(SIGINT);
            return 1;
        }

        sleepMS(50);

        f = ftdi_set_bitmode(ftdi, 0, BITMODE_CBUS);
        if (f < 0) {
            printf("Setting bitmode to BOOTLOADER_MASK failed with error %d (%s)\n", f, ftdi_get_error_string(ftdi));
            exitHandler(SIGINT);
            return 1;
        }

        puts("Reset complete");
    }

    puts("\nfirmware updater successful!");

    exitHandler(SIGINT);
    return 0;
}
