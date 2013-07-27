

/** Digitizer - Communication and control with either 
Atmel mXT224 or Synaptics ClearPad 3000-series 
capacitive touch screen controller */

#include "Digitizer.h"

/* Initialize the digitizer */
void Digitizer_Init(void){

	DDRB &= ~PIN_S4;	// PB6
	DDRD |=  PIN_SCL;	// PD0
	DDRD |=  PIN_SDA;	// PD1
	DDRD &= ~PIN_S3;	// PD2


}



