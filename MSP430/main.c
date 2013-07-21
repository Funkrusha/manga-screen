



#include <msp430.h>
#include <legacymsp430.h>

#define PIN_RESET 	BIT0
#define TXD 		BIT1
#define PIN_BL    	BIT2
#define PIN_NCS   	BIT4
#define PIN_SCLK  	BIT5
#define PIN_MOSI  	BIT6
#define PIN_MISO  	BIT7

#define CMP_NOP 		0x00
#define CMD_SWRESET 	0x01
#define CMD_RDRED 		0x06
#define CMD_RDGREEN 	0x07
#define CMD_RDBLUE  	0x08
#define CMD_RDDPM   	0x0A
#define CMD_RDDMADCTL 	0x0B
#define CMD_RDDCOLMOD   0x0C

#define LCD_C     0
#define LCD_D     1

#define Bit_time 104 	// 9600 Baud, SMCLK=1MHz (1MHz/9600)=104
#define Bit_time_5 52 	// Time for half a bit.

#define TO_HEX(i) (i <= 9 ? '0' + i : 'A' - 10 + i)


unsigned char BitCnt; 	// Bit count, used when transmitting byte
unsigned int TXByte; 	// Value sent over UART when Transmit() is called

// Function Definitions
void LcdInit(void);
void LcdUnInit(void);
void LcdGamma(void);
void LcdExtended(void);
void LcdTest(void);
void LcdWrite(char dnc, char data, char cs);
char LcdRead(char cs);
void bl_init();
void bl_off();
void delay_ms(unsigned int n);			// Delay milliseconds.
void Transmit(char c);
void send(char* str);
void hex(char x);
static void __inline__ _brief_pause(register unsigned int n);

void LcdInit(void){
	LcdWrite(LCD_C, 0x11, 1); // Sleep out command
	delay_ms(110);
	LcdWrite(LCD_C, 0x29, 1); // Display on command

	P1OUT &= ~PIN_NCS; 
	LcdWrite(LCD_C, 0x3A, 0); // Interface pixel format
	LcdWrite(LCD_D, 0x70, 0); // 24 bit
	P1OUT |= PIN_NCS; 

	P1OUT &= ~PIN_NCS; 
	LcdWrite(LCD_C, 0x36, 0); // Memory Access Control
	LcdWrite(LCD_D, 0x00, 0); // 24 bit
	P1OUT |= PIN_NCS; 
}

void LcdUnInit(void){
	LcdWrite(LCD_C, 0x28, 1); // Sleep out command
	delay_ms(110);
	LcdWrite(LCD_C, 0x10, 1); // Display on command
}

void LcdGamma(void){
	int i;
	P1OUT &= ~PIN_NCS; 
	LcdWrite(LCD_C, 0xC1, 0); // Memory Access Control
	LcdWrite(LCD_D, 0x00, 0); // DCG_EN = 0
	for(i=0; i<128; i++){
		LcdWrite(LCD_D, i*0x00, 0); // Gamma shit
	}
	
	/*for(i=0; i<=33; i++){
		LcdWrite(LCD_D, i*0x08, 0); // Gamma shit
	}
	for(i=34; i<=42; i++){
		LcdWrite(LCD_D, 0x00, 0); // Gamma shit
	}
	for(i=43; i<=75; i++){
		LcdWrite(LCD_D, (i-0x43)*0x08, 0); // Gamma shit
	}*/
	P1OUT |= PIN_NCS; 


}

void LcdExtended(void){
	P1OUT &= ~PIN_NCS; // Select Chip Enable low
	LcdWrite(LCD_C, 0xB9, 0); // Set extended commands
	LcdWrite(LCD_D, 0xFF, 0);
	LcdWrite(LCD_D, 0x83, 0);
	LcdWrite(LCD_D, 0x63, 0);
	P1OUT |= PIN_NCS; // Select Chip Enable High
}

void LcdTest(void){
	char data;

/*	send("RDID2:");
	P1OUT &= ~PIN_NCS; 
	LcdWrite(LCD_C, 0xDB, 0); // Read the version ID, should be 0x81
	data = LcdRead(1);
	hex(data);
	send("\n");

	send("Display power mode: ");
	P1OUT &= ~PIN_NCS; // Select Chip Enable low
	LcdWrite(LCD_C, 0x0A, 0);
	data = LcdRead(1);
	hex(data);
	send("\n");
	if(data & (1<<7))
		send("\tBooster OK\n");
	else
		send("\tBooster Error\n");
	if(data & (1<<4))
		send("\tSleep out mode\n");
	else
		send("\tSleep in mode\n");
	if(data & (1<<3))
		send("\tDisplay mode normal\n");
	else
		send("\tDisplay mode not normal\n");
	if(data & (1<<2))
		send("\tDisplay is on\n");
	else
		send("\tDisplay is off\n");

	send("Display pixel format: ");
	P1OUT &= ~PIN_NCS; 
	LcdWrite(LCD_C, 0x0C, 0);
	data = LcdRead(1);
	hex(data);
	send("\n");
	if(data == 0x70)
		send("\t24 bit pixel format\n");
	else if(data == 0x50)
		send("\t16 bit pixel format\n");
	else if(data == 0x60)
		send("\t18 bit pixel format\n");
	else
		send("\tUnknown pixel format\n");

	send("Display image mode: ");
	P1OUT &= ~PIN_NCS; 
	LcdWrite(LCD_C, 0x0D, 0);
	data = LcdRead(1);
	hex(data);
	send("\n");

	send("Display signal mode: ");
	P1OUT &= ~PIN_NCS; 
	LcdWrite(LCD_C, 0x0E, 0);
	data = LcdRead(1);
	hex(data);
	send("\n");

	send("Display self diagnostic: ");
	P1OUT &= ~PIN_NCS; 
	LcdWrite(LCD_C, 0x0F, 0);
	data = LcdRead(1);
	hex(data);
	send("\n");
	if(data & (1<<7))
		send("\tOTP match\n");
	else
		send("\tOTP mismatch\n");
	if(data & (1<<6))
		send("\tBooster level and timings OK\n");
	else
		send("\tBooster shit error\n");

	send("RDRED: ");
	P1OUT &= ~PIN_NCS; 
	LcdWrite(LCD_C, CMD_RDRED, 0);
	data = LcdRead(1);
	hex(data);
	send("\n");
	send("RDBLUE: ");
	P1OUT &= ~PIN_NCS; 
	LcdWrite(LCD_C, CMD_RDBLUE, 0);
	data = LcdRead(1);
	hex(data);
	send("\n");
	send("RDGREEN: ");
	P1OUT &= ~PIN_NCS; 
	LcdWrite(LCD_C, CMD_RDGREEN, 0);
	data = LcdRead(1);
	hex(data);
	send("\n");
*/

		/*P1OUT &= ~PIN_NCS; 
	LcdWrite(LCD_C, 0xB3, 0); // Set RGB interface related register (B3h)
	LcdWrite(LCD_D, 0x01, 0); // EPL + VSPL + HSPL + DPL
	P1OUT |= PIN_NCS; 
	*/

}


// Write data to the display
void LcdWrite(char dnc, char data, char cs){
	int i;

	if(cs)
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

	// Get the data byte
	for(i=0; i<8; i++){
		P1OUT &= ~PIN_SCLK; // Clock pin low
		P1OUT |= PIN_SCLK; // Clock pin high
		if(P1IN & PIN_MISO){
			data += (0x01<<(7-i));
		}
	}
	if(cs)
		P1OUT |= PIN_NCS; // Select Chip Enable High

	return data;
}

void bl_off(){
	CCR0 = 10000-1;             // PWM Period
	CCTL1 = OUTMOD_7;          // CCR1 reset/set
	CCR1 = 10000-1;                // CCR1 PWM duty cycle
	TACTL = TASSEL_2 + MC_1;   // SMCLK, up mode
}

void bl_init(){
	CCR0 = 10000-1;             // PWM Period
	CCTL1 = OUTMOD_7;          // CCR1 reset/set
	CCR1 = 7500;                // CCR1 PWM duty cycle
	TACTL = TASSEL_2 + MC_1;   // SMCLK, up mode
}

int main(void){
	char data;
	WDTCTL = WDTPW + WDTHOLD;               // Stop WDT

	BCSCTL1 = CALBC1_1MHZ; // Set range
	DCOCTL = CALDCO_1MHZ; // SMCLK = DCO = 1MHz

	bl_off();

	P1SEL |= TXD | PIN_BL;
	P1DIR |= PIN_RESET | TXD | PIN_BL;
	P1DIR |= PIN_NCS;
	P1DIR |= PIN_MOSI;
	P1DIR |= PIN_SCLK;	

	P1OUT |= PIN_RESET;
	delay_ms(10000); // Wait for power to become stable.

	P1OUT &= ~PIN_RESET;
	delay_ms(1); // Page 13 in manual for LCD
	P1OUT |= PIN_RESET;
	delay_ms(6); // Page 13 in manual for LCD
	__bis_SR_register(GIE); 	// interrupts enabled

	//send("Start\n");
	LcdInit();
	//LcdExtended();
	//LcdGamma();
	//LcdTest();
	//send("Done\n");
	bl_init();
	

	while(1){
		delay_ms(10000);
	}
}

// Function Transmits Character from TXByte
void Transmit(char c){
	TXByte = c;

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

void send(char* str){
	while(*str != '\0')
		Transmit(*str++);
}

void hex(char x){
	Transmit('0');
	Transmit('x');
	Transmit( TO_HEX(((x & 0x00F0) >> 4)) );
	Transmit( TO_HEX(((x & 0x000F) >> 0)) );
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

// Delay n milliseconds
void delay_ms(unsigned int n){
	do{
		_brief_pause(333);
	}while(n--);
}


static void __inline__ _brief_pause(register unsigned int n){
    __asm__ __volatile__ (
                "1: \n"
                " dec      %[n] \n"
                " jne      1b \n"
        : [n] "+r"(n));

}
