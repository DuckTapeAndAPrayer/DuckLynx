#include "rhspUART.h"

#include "hardware/sysctl.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/register.h"
#include "hardware/dma.h"
#include "debugUART.h"
#include "rhsp.h"

const uint32_t REGISTER_NVIC_BASE = 0xE000E000;

volatile uint32_t * const REGISTER_NVIC_EN0 = (uint32_t *)(REGISTER_NVIC_BASE + 0x100); 
const uint32_t REGISTER_NVIC_EN0_UART0 = 1 << 5;

const uint32_t REGISTER_SYSCTL_PERIPHCTL_UART_0_INSTANCEMASK = 0b1 << 0;
const uint32_t REGISTER_SYSCTL_PERIPHCTL_GPIO_A_INSTANCEMASK = 0b1 << 0;

const uint32_t REGISTER_GPIO_A_BASE = REGISTER_GPIO_BASE + 0x54000;

const uint8_t rhspUARTPins = REGISTER_GPIO_PIN_0 | REGISTER_GPIO_PIN_1;

volatile uint32_t * const REGISTER_UART_UART0_UARTDR = (uint32_t *)(REGISTER_UART_0_BASE + REGISTER_UART_UARTDR_OFFSET);

const uint32_t REGISTER_UART_UARTDMACTL_OFFSET = 0x048;
const uint32_t REGISTER_UART_UARTDMACTL_RXDMAE = 0b1 << 0;
const uint32_t REGISTER_UART_UARTDMACTL_TXDMAE = 0b1 << 1;

volatile uint32_t * const REGISTER_UART_UART0_UARTDMACTL = (uint32_t *)(REGISTER_UART_0_BASE + REGISTER_UART_UARTDMACTL_OFFSET);

const uint32_t REGISTER_DMA_DMACHCTL_XFERMODE_SHIFT = 0;
const uint32_t REGISTER_DMA_DMACHCTL_XFERMODE_MASK = 0b111 << REGISTER_DMA_DMACHCTL_XFERMODE_SHIFT;
const uint32_t REGISTER_DMA_DMACHCTL_XFERMODE_BASIC = 0x1 << REGISTER_DMA_DMACHCTL_XFERMODE_SHIFT;

const uint32_t REGISTER_DMA_DMACHCTL_XFERSIZE_SHIFT = 4;
const uint32_t REGISTER_DMA_DMACHCTL_XFERSIZE_MASK = 0b1111111111 << REGISTER_DMA_DMACHCTL_XFERSIZE_SHIFT;

const uint32_t REGISTER_DMA_DMACHCTL_ARBSIZE_SHIFT = 14;
const uint32_t REGISTER_DMA_DMACHCTL_ARBSIZE_MASK = 0b1111 << REGISTER_DMA_DMACHCTL_ARBSIZE_SHIFT;
const uint32_t REGISTER_DMA_DMACHCTL_ARBSIZE_FOUR = 0x2 << REGISTER_DMA_DMACHCTL_ARBSIZE_SHIFT;
const uint32_t REGISTER_DMA_DMACHCTL_ARBSIZE_EIGHT = 0x3 << REGISTER_DMA_DMACHCTL_ARBSIZE_SHIFT;

const uint32_t REGISTER_DMA_DMACHCTL_SRCSIZE_SHIFT = 24;
const uint32_t REGISTER_DMA_DMACHCTL_SRCSIZE_MASK = 0b11 << REGISTER_DMA_DMACHCTL_SRCSIZE_SHIFT;
const uint32_t REGISTER_DMA_DMACHCTL_SRCSIZE_BYTE = 0b0 << REGISTER_DMA_DMACHCTL_SRCSIZE_SHIFT;

const uint32_t REGISTER_DMA_DMACHCTL_SRCINC_SHIFT = 26;
const uint32_t REGISTER_DMA_DMACHCTL_SRCINC_MASK = 0b11 << REGISTER_DMA_DMACHCTL_SRCINC_SHIFT;
const uint32_t REGISTER_DMA_DMACHCTL_SRCINC_BYTE = 0x0 << REGISTER_DMA_DMACHCTL_SRCINC_SHIFT;
const uint32_t REGISTER_DMA_DMACHCTL_SRCINC_NONE = 0x3 << REGISTER_DMA_DMACHCTL_SRCINC_SHIFT;

const uint32_t REGISTER_DMA_DMACHCTL_DSTSIZE_SHIFT = 28;
const uint32_t REGISTER_DMA_DMACHCTL_DSTSIZE_MASK = 0b11 << REGISTER_DMA_DMACHCTL_DSTSIZE_SHIFT;
const uint32_t REGISTER_DMA_DMACHCTL_DSTSIZE_BYTE = 0b0 << REGISTER_DMA_DMACHCTL_DSTSIZE_SHIFT;

const uint32_t REGISTER_DMA_DMACHCTL_DSTINC_SHIFT = 30;
const uint32_t REGISTER_DMA_DMACHCTL_DSTINC_MASK = 0b11 << REGISTER_DMA_DMACHCTL_DSTINC_SHIFT;
const uint32_t REGISTER_DMA_DMACHCTL_DSTINC_BYTE = 0x0 << REGISTER_DMA_DMACHCTL_DSTINC_SHIFT;
const uint32_t REGISTER_DMA_DMACHCTL_DSTINC_NONE = 0x3 << REGISTER_DMA_DMACHCTL_DSTINC_SHIFT;

volatile uint32_t * const REGISTER_DMA_DMAUSEBURSTSET = (uint32_t *)(REGISTER_DMA_BASE + 0x18);
volatile uint32_t * const REGISTER_DMA_DMAENASET = (uint32_t *)(REGISTER_DMA_BASE + 0x028);

volatile uint32_t * const REGISTER_DMA_DMACHMAP1 = (uint32_t *)(REGISTER_DMA_BASE + 0x514);
const uint32_t REGISTER_DMA_DMACHMAPn_CHANNEL_0_SHIFT = 0;
const uint32_t REGISTER_DMA_DMACHMAPn_CHANNEL_0_MASK = 0b1111 << REGISTER_DMA_DMACHMAPn_CHANNEL_0_SHIFT;
const uint32_t REGISTER_DMA_DMACHMAPn_CHANNEL_1_SHIFT = 4;
const uint32_t REGISTER_DMA_DMACHMAPn_CHANNEL_1_MASK = REGISTER_DMA_DMACHMAPn_CHANNEL_0_MASK << REGISTER_DMA_DMACHMAPn_CHANNEL_1_SHIFT;
const uint32_t REGISTER_DMA_DMACHMAP1_CHANNEL_8_UART_0_RX = 0 << REGISTER_DMA_DMACHMAPn_CHANNEL_0_SHIFT;
const uint32_t REGISTER_DMA_DMACHMAP1_CHANNEL_9_UART_0_TX = 0 << REGISTER_DMA_DMACHMAPn_CHANNEL_1_SHIFT;

uint8_t rawBuffer[RHSP_BUFFER_SIZE];
uint8_t * rawBufferEnd = rawBuffer + RHSP_BUFFER_SIZE;

uint16_t lastReceiveSize = 0;
uint8_t * lastReceiveLocation = 0;
void rhspUART_receive(uint8_t * dest, uint16_t length) {
    if(length > 1024 || length == 0) { 
        return;
    }

    if(*REGISTER_DMA_DMAENASET & REGISTER_DMA_CHANNEL_8) {
        debugUART_printString("ERROR: UART RX DMA enabled when trying to receive more data. Failing!!!!");
        return;
    }

    lastReceiveSize = length;
    lastReceiveLocation = dest;

    ChannelControlEntry * entry = &channelControlTable[8];
    length--;
    entry->DMADSTENDP = dest + length;
    entry->DMACHCTL = 
       (entry->DMACHCTL & ~REGISTER_DMA_DMACHCTL_XFERSIZE_MASK)
     | (REGISTER_DMA_DMACHCTL_XFERSIZE_MASK & (length << REGISTER_DMA_DMACHCTL_XFERSIZE_SHIFT))
     | REGISTER_DMA_DMACHCTL_XFERMODE_BASIC
    ;
    
    *REGISTER_DMA_DMAENASET |= REGISTER_DMA_CHANNEL_8;
}

int8_t rhspUART_init(void) {
    //Setup rhsp UART
    sysctl_enablePeripheral(REGISTER_SYSCTL_PERIPHCTL_UART_OFFSET, REGISTER_SYSCTL_PERIPHCTL_UART_0_INSTANCEMASK);
    sysctl_enablePeripheral(REGISTER_SYSCTL_PERIPHCTL_GPIO_OFFSET, REGISTER_SYSCTL_PERIPHCTL_GPIO_A_INSTANCEMASK);
    
    gpio_enableAltPinFunc(REGISTER_GPIO_A_BASE, rhspUARTPins, REGISTER_GPIO_GPIOPCTL_UART);

    //For 460800 baud and a system clock of 80 MHz the integer divisor is 10 and the fractional component is 54. 
    // To compute see datasheet page 926.
    uart_configure(REGISTER_UART_0_BASE, 10, 54);

    //Enable UART0 interrupts
    *REGISTER_NVIC_EN0 |= REGISTER_NVIC_EN0_UART0;

    //Setup DMA
    *REGISTER_DMA_DMACHMAP1 = 
           (*REGISTER_DMA_DMACHMAP1 & (~REGISTER_DMA_DMACHMAPn_CHANNEL_1_MASK))
         | REGISTER_DMA_DMACHMAP1_CHANNEL_8_UART_0_RX
         | REGISTER_DMA_DMACHMAP1_CHANNEL_9_UART_0_TX
    ;
     
    ChannelControlEntry * entry = &channelControlTable[8];
    entry->DMASRCENDP = REGISTER_UART_UART0_UARTDR;
    entry->DMACHCTL =
        REGISTER_DMA_DMACHCTL_ARBSIZE_EIGHT |
        REGISTER_DMA_DMACHCTL_SRCSIZE_BYTE |
        REGISTER_DMA_DMACHCTL_SRCINC_NONE |
        REGISTER_DMA_DMACHCTL_DSTSIZE_BYTE |
        REGISTER_DMA_DMACHCTL_DSTINC_BYTE
    ;

    entry = &channelControlTable[9];
    entry->DMADSTENDP = REGISTER_UART_UART0_UARTDR;
    entry->DMACHCTL =
        REGISTER_DMA_DMACHCTL_ARBSIZE_EIGHT |
        REGISTER_DMA_DMACHCTL_SRCSIZE_BYTE |
        REGISTER_DMA_DMACHCTL_SRCINC_BYTE |
        REGISTER_DMA_DMACHCTL_DSTSIZE_BYTE |
        REGISTER_DMA_DMACHCTL_DSTINC_NONE
    ;

    *REGISTER_UART_UART0_UARTDMACTL |= REGISTER_UART_UARTDMACTL_TXDMAE | REGISTER_UART_UARTDMACTL_RXDMAE;

    // debugUART_printString("Address of buffer: ");
    // debugUART_printWordHex((uint32_t) rawBuffer);
    // debugUART_printChar('\n');

    rhspUART_receive(rawBuffer, RHSP_PACKET_MIN_SIZE);

    return 0;
}

void rhspUART_send(uint8_t * buffer, uint16_t bufferLength) {
    if(bufferLength > 1024 || bufferLength == 0) { 
        return;
    }

    while(*REGISTER_DMA_DMAENASET & REGISTER_DMA_CHANNEL_9) {
        //Wait for transfer to finish
    }

    bufferLength--;
    ChannelControlEntry * entry = &channelControlTable[9];
    entry->DMASRCENDP = buffer + bufferLength;
    entry->DMACHCTL = 
       (entry->DMACHCTL & ~REGISTER_DMA_DMACHCTL_XFERSIZE_MASK)
     | (REGISTER_DMA_DMACHCTL_XFERSIZE_MASK & ((bufferLength) << REGISTER_DMA_DMACHCTL_XFERSIZE_SHIFT))
     | REGISTER_DMA_DMACHCTL_XFERMODE_BASIC
    ;
    
    //This doesn't need to be set because UART supports Wait-on-Request which does the same thing
    // *REGISTER_DMA_DMAUSEBURSTSET |= REGISTER_DMA_CHANNEL_9;
    *REGISTER_DMA_DMAENASET |= REGISTER_DMA_CHANNEL_9;
}

//ADD: Monitor for if the RX FIFO looses data
void rhspUART_interruptHandler(void) {
    if((*REGISTER_DMA_DMACHIS) & REGISTER_DMA_CHANNEL_9) {
        //Even if this bit isn't cleared in this handler the interrupt won't be triggered again,
        // but the next time the interrupt is triggered and this handler is called the bit will 
        // still be set which could confuse things.
        *REGISTER_DMA_DMACHIS |= REGISTER_DMA_CHANNEL_9;
        // debugUART_printString("Interrupt from TX DMA finishing!\n\n");
        rhspUART_receive(rawBuffer, RHSP_PACKET_MIN_SIZE);
    }

    if((*REGISTER_DMA_DMACHIS) & REGISTER_DMA_CHANNEL_8) {
        *REGISTER_DMA_DMACHIS |= REGISTER_DMA_CHANNEL_8;
        // debugUART_printString("Interrupt from RX DMA finishing!\n");
        rhsp_tick(lastReceiveLocation, lastReceiveSize);
    }
}
