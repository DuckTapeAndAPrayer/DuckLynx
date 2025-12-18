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
    #define errorno
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
    printf("Exit Handler\n");
    if(num == SIGINT) {
        if(!stop) {
            stop = 1;
            
            if(open) {
                printf("disabling bitbang mode\n");
                ftdi_disable_bitbang(ftdi);
                ftdi_usb_close(ftdi);
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

const uint8_t AUTOBAUD_RETRY_ATTEMPS = 5;
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
    };
};

struct  __attribute__((packed)) Packet {
    uint8_t size;
    uint8_t checksum;    
    union {
        uint8_t payload[PACKET_MAX_PAYLOAD_LENGTH];
        struct CommandPayload commandPayload;
        uint32_t goAddr;
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
        fprintf(stderr,"Writing message failed with error %d (%s)\n", f, ftdi_get_error_string(ftdi));
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
int serialRead(uint8_t * location, uint8_t count) {
    printf("SerialRead for %u\n", count);
    uint8_t left = count;
    while(1) {
        // printf("Trying to read in %u\n", left);
        int f = ftdi_read_data(ftdi, inBuffer.buffer + (count - left), left);
        if (f < 0) {
            fprintf(stderr, "Reading in failed with error %d (%s)\n", f, ftdi_get_error_string(ftdi));
            return 1;
        }

        left -= f;
        if(left == 0) {
            break;
        }

        sleepMS(10);
    }

    puts("");

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
        fprintf(stderr, "Nacked on %s command\n", location);
        return 1;
    } else if(inBuffer.responseType1 != RESPONSE_ACK) {
        fprintf(stderr, "Unknown response to %s command: 0x%x\n", location, inBuffer.responseType1);
        return 1;
    } else {
        printf("Good type 1 response at \"%s\"\n", location);
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
        fprintf(stderr, "Sending NCK to get status when doing \"%s\" command.\n", location);
        return 1;  
    }

    serialWrite(inBuffer.buffer, RESPONSE_TYPE_1_LENGTH);

    if(inBuffer.responseType2.code != RESPONSE_TYPE_2_SUCCESS) {
        fprintf(stderr, "Got non-success status code %x which means \"%s\" while doing \"%s\" command.\n", inBuffer.responseType2.code, RESPONSE_TYPE_2_CODE_toString(inBuffer.responseType2.code), location);
        return 1;
    } else {
        printf("Success at \"%s\"\n", location);
    }

    return 0;
}

void printUsage(char * argV0) {
fputs("firmware_updater. An updater for Lynx firmware and a replacement to LMFlashProgrammer from TI\n\nUsage: ", stdout);
fputs(argV0, stdout);
    puts(
" [action] <required argument> <optional argument>\n\
Actions:\n\
    reset - Reset the MCU using the reset line connected to the FTDI CBUS\n\
    reset_soft - Reset MCU in software with bootloader\n\
    bootloader - Reset the MCU, enter the bootloader, and perform autobaud negotiation\n\
    write - Write the provided file to the MCU. The required argument is filename\n\
    go - Start execution at the provided address. The required argument is address\n\
    flash - enter booloader, write, and reset_soft - The required argument is the filename to write\n\
\n\
Write, Flash, and Go options:\n\
    -a=<address> --address=<address>. \
    Provided addresses should be in the format 0x<addr> from 0x00000000 to 0xFFFFFFFF. If they are outside the allowable range a NAK will be received from the MCU and reported.\n"
    );
}

enum ACTION : uint8_t{
    ACTION_RESET      = 0b000001,
    ACTION_RESET_SOFT = 0b000010,
    ACTION_BOOTLOADER = 0b000100,
    ACTION_WRITE      = 0b001000,
    ACTION_GO         = 0b010000,
    ACTION_FLASH      = ACTION_BOOTLOADER | ACTION_WRITE | ACTION_RESET_SOFT,
};

uint8_t shouldRunAction(enum ACTION targetAction, enum ACTION realAction) {
    return (realAction & targetAction) > 0;
}

int checkSysFunc(int retVal, const char * location) {
    if (__unix) {
        printf("Error #%i meaning \"%s\" at \"%s\"\n", errno, strerror(errno), location);
    } else {
        printf("Error at \"%s\"\n", location);
    }
    
    return retVal;
}

int main(int argc, char **argv) {
    puts("firmware updater Copyright (C) 2025 Bryn 'bakl' Hakl");
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
    if(strcmp(actionStr, "reset")) {
        action = ACTION_RESET;

    } else if (strcmp(actionStr, "reset_soft")) {
        action = ACTION_RESET_SOFT;
        
    } else if (strcmp(actionStr, "bootloader")) {
        action = ACTION_BOOTLOADER;
        
    } else if (strcmp(actionStr, "write")) {
        action = ACTION_WRITE;
        
    } else if (strcmp(actionStr, "go")) {
        action = ACTION_GO;
        
    } else if (strcmp(actionStr, "flash")) {
        action = ACTION_FLASH;
        
    } else {
        printf("ERROR: Unknown action \"%s\"\n", actionStr);
        printUsage(argv[0]);
        return 1;
    }

    uint32_t addr = 0;
    if(shouldRunAction(ACTION_GO | ACTION_WRITE, action)) {
        if(argc == 3) {
            const char * opt = argv[3];
            const char shortArg[] = "-a=0x";
            const char longArg[] = "--address=0x";
            const char * hexString = 0;
            if(strncmp(shortArg, opt, sizeof(shortArg)) == 0) {
                hexString = opt + sizeof(shortArg) - 1;
            } else if (strncmp(longArg, opt, sizeof(longArg)) == 0) {
                hexString = opt + sizeof(longArg) - 1;
            } else {
                printf("Invalid argument \"%s\"\n", opt);
                return 1;
            }

            if(strlen(hexString) > 8) {
                printf("Address \"%s\" too long. Max 8 hex characters\n", hexString);
                return 1;
            }

            addr = strtoul(hexString, nullptr, 16);
        } else {
            printf("ERROR: Too many arguments. Max is three for action \"%s\"\n", actionStr);
        }

    } else if(argc > 2) {
        printf("ERROR: Too many arguments. Max is two for action \"%s\"\n", actionStr);
    }

    printf("Running action \"%s\"\n", actionStr);

    //We never use fclose because the file will be closed on exit
    FILE *file = 0;
    uint32_t fileSize = 0;

    if(shouldRunAction(ACTION_WRITE, action)) {
        const char * fileName = argv[2];
        file = fopen(fileName, "r");
        if(file == NULL) {
            checkSysFunc(-1, "opening file");
            return 1;
        }

        if(!checkSysFunc(fseek(file, 0L, SEEK_END), "seeking file to end")) {
            return 1;
        }
        
        long fileSizeLong = ftell(file);

        if(!checkSysFunc(fileSizeLong, "finding file length")) {
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
        fprintf(stderr, "ftdi_new failed\n");
        return 1;
    }

    ftdi->module_detach_mode = AUTO_DETACH_REATACH_SIO_MODULE;

    //ADD: Support for opening other FTDI devices not just the 230X with this VID and PID
    f = ftdi_usb_open(ftdi, 0x0403, 0x6015);
    if (f < 0) {
        fprintf(stderr, "Unable to open FTDI device: %d (%s)\n", f, ftdi_get_error_string(ftdi));
        exitHandler(SIGINT);
        return 1;
    }

    open = 1;
    puts("FTDI open succeeded\n");

    if(shouldRunAction(ACTION_RESET, action)) {
        puts("Resetting MCU");

        f = ftdi_set_bitmode(ftdi, RST_MASK, BITMODE_CBUS);
        if (f < 0) {
            fprintf(stderr,"Setting bitmode to RST_MASK | BOOTLOADER_MASK failed with error %d (%s)\n", f, ftdi_get_error_string(ftdi));
            exitHandler(SIGINT);
            return 1;
        }

        sleepMS(50);

        f = ftdi_set_bitmode(ftdi, 0, BITMODE_CBUS);
        if (f < 0) {
            fprintf(stderr,"Setting bitmode to BOOTLOADER_MASK failed with error %d (%s)\n", f, ftdi_get_error_string(ftdi));
            exitHandler(SIGINT);
            return 1;
        }

        puts("Reset complete");

    } else {
        // puts("Setting baudrate");
        //Baud rate is divided by four because if bitbang mode is on libftdi1 multiplies the given baud rate by four.
        // For the Lynx MCU the max baud rate is 500000 because the max baud rate is the clock / 32 and the bootloader 
        // clock of 16 MHz / 32 = 500000. We run at 115200 to be on the safer side for testing
        //ADD: Option to set baudrate
        f = ftdi_set_baudrate(ftdi, 115200 / 4);
        if (f < 0) {
            fprintf(stderr,"Setting baudrate failed with error %d (%s)\n", f, ftdi_get_error_string(ftdi));
            exitHandler(SIGINT);
            return 1;
        }

        // puts("Setting line properties");
        f = ftdi_set_line_property(ftdi, 8, STOP_BIT_1, NONE);
        if (f < 0) {
            fprintf(stderr,"Setting line properties with error %d (%s)\n", f, ftdi_get_error_string(ftdi));
            exitHandler(SIGINT);
            return 1;
        }

        if(shouldRunAction(ACTION_BOOTLOADER, action)) {
            puts("Entering bootloader");
            f = ftdi_set_bitmode(ftdi, RST_MASK | BOOTLOADER_MASK, BITMODE_CBUS);
            if (f < 0) {
                fprintf(stderr,"Setting bitmode to RST_MASK | BOOTLOADER_MASK failed with error %d (%s)\n", f, ftdi_get_error_string(ftdi));
                exitHandler(SIGINT);
                return 1;
            }

            sleepMS(50);

            f = ftdi_set_bitmode(ftdi, BOOTLOADER_MASK, BITMODE_CBUS);
            if (f < 0) {
                fprintf(stderr,"Setting bitmode to BOOTLOADER_MASK failed with error %d (%s)\n", f, ftdi_get_error_string(ftdi));
                exitHandler(SIGINT);
                return 1;
            }

            sleepMS(50);

            f = ftdi_set_bitmode(ftdi, 0, BITMODE_CBUS);
            if (f < 0) {
                fprintf(stderr,"Setting bitmode to 0 failed with error %d (%s)\n", f, ftdi_get_error_string(ftdi));
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
                if(bytesRead != COMMAND_MAX_PAYLOAD_LENGTH && !checkSysFunc(ferror(file), "read file data")) {
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

        const char * location = 0;
        if(shouldRunAction(ACTION_GO, action)) {
            packet->goAddr = __bswap_constant_32(addr);
            packet->size = 2;
            location = "Go";
        } else if(shouldRunAction(ACTION_RESET_SOFT, action)) {
            packet->commandPayload.command = COMMAND_RESET;
            packet->size = 1;
            location = "Software reset";
        }
        
        if(shouldRunAction(ACTION_GO | ACTION_RESET_SOFT, action)) {
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
        }
    }

    puts("\nfirmware updater successful!");

    exitHandler(SIGINT);
    return 0;
}
