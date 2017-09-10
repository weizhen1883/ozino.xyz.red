#include <msp430g2553.h>
#include <stdint.h>

#define MAIN_MEMORY_START_ADDRESS  0xC000
#define MAIN_MEMORY_END_ADDRESS  0xFBFF

// Vector table handlers
typedef void vector_handler(void);
vector_handler *APP = (vector_handler *)0xC000;
vector_handler *PORT1_VECTOR_HANDLER = (vector_handler *)0x0202;
vector_handler *PORT2_VECTOR_HANDLER = (vector_handler *)0x0203;
vector_handler *ADC10_VECTOR_HANDLER = (vector_handler *)0x0205;
vector_handler *USCIAB0TX_VECTOR_HANDLER = (vector_handler *)0x0206;
vector_handler *USCIAB0RX_VECTOR_HANDLER = (vector_handler *)0x0207;
vector_handler *TIMER0_A1_VECTOR_HANDLER = (vector_handler *)0x0208;
vector_handler *TIMER0_A0_VECTOR_HANDLER = (vector_handler *)0x0209;
vector_handler *WDT_VECTOR_HANDLER = (vector_handler *)0x020A;
vector_handler *COMPARATORA_VECTOR_HANDLER = (vector_handler *)0x020B;
vector_handler *TIMER1_A1_VECTOR_HANDLER = (vector_handler *)0x020C;
vector_handler *TIMER1_A0_VECTOR_HANDLER = (vector_handler *)0x020D;
vector_handler *NMI_VECTOR_HANDLER = (vector_handler *)0x020E;

__attribute__((interrupt(PORT1_VECTOR))) void PORT1_ISR(void);
__attribute__((interrupt(PORT2_VECTOR))) void PORT2_ISR(void);
__attribute__((interrupt(ADC10_VECTOR))) void ADC10_ISR(void);
__attribute__((interrupt(USCIAB0TX_VECTOR))) void USCIAB0TX_ISR(void);
__attribute__((interrupt(USCIAB0RX_VECTOR))) void USCIAB0RX_ISR(void);
__attribute__((interrupt(TIMER0_A1_VECTOR))) void TIMER0_A1_ISR(void);
__attribute__((interrupt(TIMER0_A0_VECTOR))) void TIMER0_A0_ISR(void);
__attribute__((interrupt(WDT_VECTOR))) void WDT_ISR(void);
__attribute__((interrupt(COMPARATORA_VECTOR))) void COMPARATORA_ISR(void);
__attribute__((interrupt(TIMER1_A1_VECTOR))) void TIMER1_A1_ISR(void);
__attribute__((interrupt(TIMER1_A0_VECTOR))) void TIMER1_A0_ISR(void);
__attribute__((interrupt(NMI_VECTOR))) void NMI_ISR(void);

void PORT1_ISR(void) { PORT1_VECTOR_HANDLER(); }
void PORT2_ISR(void) { PORT2_VECTOR_HANDLER(); }
void ADC10_ISR(void) { ADC10_VECTOR_HANDLER(); }
void USCIAB0TX_ISR(void) { USCIAB0TX_VECTOR_HANDLER(); }
void USCIAB0RX_ISR(void) { USCIAB0RX_VECTOR_HANDLER(); }
void TIMER0_A1_ISR(void) { TIMER0_A1_VECTOR_HANDLER(); }
void TIMER0_A0_ISR(void) { TIMER0_A0_VECTOR_HANDLER(); }
void WDT_ISR(void) { WDT_VECTOR_HANDLER(); }
void COMPARATORA_ISR(void) { COMPARATORA_VECTOR_HANDLER(); }
void TIMER1_A1_ISR(void) { TIMER1_A1_VECTOR_HANDLER(); }
void TIMER1_A0_ISR(void) { TIMER1_A0_VECTOR_HANDLER(); }
void NMI_ISR(void) { NMI_VECTOR_HANDLER(); }

// variables
uint8_t bootloader_mode = 0;
uint8_t programmable = 0;
uint8_t data[25];
uint8_t data_index = 0;

// flash memory controller function
void write_byte_to_memory(uint16_t address, uint8_t val) {
    uint8_t *ptr;
    ptr = (uint8_t *) address;
    FCTL1 = FWKEY + WRT;
    FCTL3 = FWKEY;
    *ptr = val;
    FCTL1 = FWKEY;
    FCTL3 = FWKEY + LOCK;
}

void erase_individual_segment(uint16_t segment_base_address) {
    uint8_t *ptr;
    ptr = (uint8_t *) segment_base_address;
    FCTL3 = FWKEY;
    FCTL1 = FWKEY + ERASE;
    *ptr = 0xFF;
    FCTL1 = FWKEY;
    FCTL3 = FWKEY + LOCK;
}

void erase_segments(uint16_t segment_start_address, uint16_t segment_end_address) {
    uint16_t segment_address = segment_start_address;
    while (segment_address < segment_end_address) {
        erase_individual_segment(segment_start_address);
        segment_address += 0x200;
    }
}

// uart functions
void uart_write_char(char c) {
    while (!(IFG2 & UCA0TXIFG));
    UCA0TXBUF = c;
}

void uart_write_string(char *buf) {
    while (*buf) uart_write_char(*buf++);
}

void USCI0RX_ISR(void) {
    uint8_t rx_char, i;
    uint16_t programming_address;
    if (IFG2 & UCA0RXIFG) {
        rx_char = UCA0RXBUF;
        if (bootloader_mode == 1) {
            if (programmable == 1) {
                data[data_index] = rx_char;
                data_index++;
                if ((data_index == (data[3] + 4)) && (data_index > 4)) {
                    if (data[0] == '+') { // '+' program data into chip
                        if ((data_index <= 4) && (data_index < (data[3] + 4))) {
                            uart_write_char('!'); // return error if formate not right
                        } else {
                            programming_address = (data[1] << 8) | (data[2]);
                            for(i = 0; i < data[3]; i++) {
                                if (data[4 + i] != 0xFF) write_byte_to_memory(programming_address + i, data[4 + i]);
                            }
                            uart_write_char('>');  // return the code programmed
                        }
                    } else if (data[0] == '-') { // '-' some error happened, remove all the code
                        erase_segments(MAIN_MEMORY_START_ADDRESS, MAIN_MEMORY_END_ADDRESS);
                        uart_write_char('!');
                    }
                    data_index = 0;
                }
            }
        } else {
            if (rx_char == 'b') bootloader_mode = 1;
        }
    }
}

int main(void) {
	WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer

	// configure clocks;
	BCSCTL1 = CALBC1_16MHZ; 		// Set oscillator to 16MHz
	DCOCTL = CALDCO_16MHZ;  		// Set oscillator to 16MHz

    // init flash memory control
    FCTL2 = FWKEY + FSSEL_2 + 0x30;

    // configure pins
    P1DIR = 0xFF;									// Set P1 to output direction
	P2DIR = 0xFF;									// Set P2 to output direction
	P3DIR = 0xFF;									// Set P3 to output direction

	P1OUT = 0x00;									// Set P1  outputs to logic 0
	P2OUT = 0x00;									// Set P2  outputs to logic 0
	P3OUT = 0x00;									// Set P3  outputs to logic 0

	// init uart
	P1SEL |= BIT1 | BIT2;
    P1SEL2 |= BIT1 | BIT2;
    UCA0CTL1 |= UCSSEL_2; // Use SMCLK = 16MHz
	UCA0BR1 = 0;
	UCA0BR0 = 138;
	UCA0MCTL = UCBRS_7 | UCBRF_0;
	UCA0CTL1 &= ~UCSWRST; // Initialize USCI state machine
    IE2 |= UCA0RXIE; // enable RX interrupt
	uart_write_string((char *)"System Booting ...\r\n");
    USCIAB0RX_VECTOR_HANDLER = USCI0RX_ISR;
    __bis_SR_register(GIE);
    __delay_cycles(16000000);					// Sleep 3 seconds and try to enter to bootloader

    if (bootloader_mode == 1) {
        // bootloader mode
        uart_write_string((char *)"Run Bootloader\r\n");
        uart_write_string((char *)"Erasing...\r\n");
        erase_segments(MAIN_MEMORY_START_ADDRESS, MAIN_MEMORY_END_ADDRESS);
        uart_write_string((char *)"Programming...\r\n");
        programmable = 1;
        data_index = 0;
    } else {
        // app mode
        uart_write_string((char *)"Start APP ...\r\n");
        __delay_cycles(16000);
        APP(); // Run the app main()
    }

	__bis_SR_register(GIE + LPM4_bits);
	return 0;
}
