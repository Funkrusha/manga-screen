/******************************************************************************
* LCD controller - This uC is on the PCB that controls an LCD HTC Desire HD
* uC: MSP430G2231, the one that comes with the Launchpad from TI.
*
* Copyright (C) 2011  Intelligent Agent AS <software@iagent.no>
* You may use all or parts of this code any way you want. You can even remove
* the copyright notice and these conditions. There is NO WARRANTY, to the
* extent permitted by law.
*
* 05.06.2013 - Elias Bakken <elias@iagent.no>
*******************************************************************************/

#include <io.h>
#include <signal.h>

#define PIN_RESET BIT0
#define TXD		  BIT1
#define PIN_BL    BIT2
#define PIN_NCS   BIT4
#define PIN_SCLK  BIT5
#define PIN_MOSI  BIT6
#define PIN_MISO  BIT7


#define LCD_C     0
#define LCD_D     1

#define Bit_time 104 	// 9600 Baud, SMCLK=1MHz (1MHz/9600)=104


void LcdInitialise(void);
void LcdWrite(char dnc, char data, char cs);
char LcdRead(char cs);
void bl_init();
void delay_ms(unsigned int n);			// Delay milliseconds.
static void __inline__ _brief_pause(register unsigned int n);
void Transmit(void);

unsigned int TXByte; 	// Value sent over UART when Transmit() is called
unsigned char BitCnt; 	// Bit count, used when transmitting byte


// Function Transmits Character from TXByte
void Transmit(){
	CCTL0 = OUT; // TXD Idle as Mark
	TACTL = TASSEL_2 + MC_2; // SMCLK, continuous mode

	BitCnt = 0xA; // Load Bit counter, 8 bits + ST/SP
	CCR0 = TAR; // Initialize compare register

	CCR0 += Bit_time; // Set time till first bit
	TXByte |= 0x100; // Add stop bit to TXByte (which is logical 1)
	TXByte = TXByte << 1; // Add start bit (which is logical 0)

	CCTL0 = CCIS0 + OUTMOD0 + CCIE; // Set signal, intial value, enable interrupts
	while ( CCTL0 & CCIE ); // Wait for previous TX completion
}


void LcdInitialise(void){
	int data;

	LcdWrite(LCD_C, 0x11, 1); // Sleep out command
	delay_ms(100);

	LcdWrite(LCD_C, 0xB9, 0); // Set extended commands
	TXByte = LcdRead(0);
	Transmit();
	TXByte = LcdRead(0);
	Transmit();
	TXByte = LcdRead(1);
	Transmit();
	LcdWrite(LCD_D, 0xDB, 0);
	TXByte = LcdRead(1);
	Transmit();
	LcdWrite(LCD_C, 0x29, 1); // Display on command
}

// Write data to the display
void LcdWrite(char dnc, char data, char cs){
	int i;

	P1OUT &= ~PIN_NCS; // Select Chip Enable low

	// Output the control bit DNC
	if(dnc)
		P1OUT |= PIN_MOSI;
	else
		P1OUT &= ~PIN_MOSI;
	P1OUT &= ~PIN_SCLK; // Clock pin low
	P1OUT |= PIN_SCLK; // Clock pin high

	// Output the data byte
	for(i=0; i<8; i++){
		if(data & (0x01<<(7-i)))
			P1OUT |= PIN_MOSI;
		else
			P1OUT &= ~PIN_MOSI;
		P1OUT &= ~PIN_SCLK; // Clock pin low
		P1OUT |= PIN_SCLK; // Clock pin high
	}

	if(cs)
		P1OUT |= PIN_NCS; // Select Chip Enable High

}

char LcdRead(char cs){
	int i;
	char data = 0;

	P1OUT &= ~PIN_SCLK; // Clock pin low
	P1OUT |= PIN_SCLK; // Clock pin high

	// Get the data byte
	for(i=0; i<8; i++){
		if(P1IN & PIN_MOSI){
			data |= (0x01<<(7-i));
		}
		P1OUT &= ~PIN_SCLK; // Clock pin low
		P1OUT |= PIN_SCLK; // Clock pin high
	}
	if(cs)
		P1OUT |= PIN_NCS; // Select Chip Enable High

	return data;
}

void bl_init(){
	CCR0 = 10000-1;             // PWM Period
	CCTL1 = OUTMOD_7;          // CCR1 reset/set
	CCR1 = 5000;                // CCR1 PWM duty cycle
	TACTL = TASSEL_2 + MC_1;   // SMCLK, up mode
}

// Main method.
int main(void){
	int index;
	int i = 0;
	WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

	BCSCTL1 = CALBC1_1MHZ; // Set range
	DCOCTL = CALDCO_1MHZ; // SMCLK = DCO = 1MHz

	P1DIR |= PIN_RESET | TXD | PIN_BL;
	P1SEL |= TXD | PIN_BL;
	P1DIR |= PIN_NCS;
	P1DIR |= PIN_MOSI;
	P1DIR |= PIN_SCLK;

	P1OUT |= PIN_RESET;
	delay_ms(1000); // Wait for power to become stable.

	P1OUT &= ~PIN_RESET;
	delay_ms(1); // Page 13 in manual for LCD
	P1OUT |= PIN_RESET;
	delay_ms(120);

	bl_init();

	TXByte = 'O';
	Transmit();
	TXByte = 'K';
	Transmit();


	LcdInitialise();

	__bis_SR_register(GIE); 	// interrupts enabled

	while(1){
		delay_ms(1000);
		TXByte = 0xAA;
		Transmit();
	}
}


static void __inline__ _brief_pause(register unsigned int n)
{
    __asm__ __volatile__ (
                "1: \n"
                " dec      %[n] \n"
                " jne      1b \n"
        : [n] "+r"(n));

}
// Delay n milliseconds
void delay_ms(unsigned int n){
	do{
		_brief_pause(333);
	}while(n--);
}


interrupt(TIMERA0_VECTOR) ta1_isr (void){
	CCR0 += Bit_time; 					// Add Offset to CCR0
	if ( BitCnt == 0){ 					// If all bits TXed
		TACTL = TASSEL_2; 				// SMCLK, timer off (for power consumption)
		CCTL0 &= ~ CCIE ; 				// Disable interrupt
	}
	else{
		CCTL0 |= OUTMOD2; // Set TX bit to 0
		if (TXByte & 0x01)
			CCTL0 &= ~ OUTMOD2; // If it should be 1, set it to 1
		TXByte = TXByte >> 1;
		BitCnt --;
	}
}
