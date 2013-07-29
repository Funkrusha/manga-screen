/*
             LUFA Library
     Copyright (C) Dean Camera, 2013.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2013  Dean Camera (dean [at] fourwalledcubicle [dot] com)	

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Main source file for the VirtualSerialMouse demo. This file contains the main tasks of
 *  the demo and is responsible for the initial application hardware configuration.
 */

#include "MangaScreen.h"

/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
	{
		.Config =
			{
				.ControlInterfaceNumber         = 0,
				.DataINEndpoint                 =
					{
						.Address                = CDC_TX_EPADDR,
						.Size                   = CDC_TXRX_EPSIZE,
						.Banks                  = 1,
					},
				.DataOUTEndpoint                =
					{
						.Address                = CDC_RX_EPADDR,
						.Size                   = CDC_TXRX_EPSIZE,
						.Banks                  = 1,
					},
				.NotificationEndpoint           =
					{
						.Address                = CDC_NOTIFICATION_EPADDR,
						.Size                   = CDC_NOTIFICATION_EPSIZE,
						.Banks                  = 1,
					},
			},
	};

/** Buffer to hold the previously generated Digitizer HID report, for comparison purposes inside the HID class driver. */
static uint8_t PrevDigitizerHIDReportBuffer[sizeof(USB_DigitizerReport_Data_t)];

/** Circular buffer to hold data from the host before it is sent to the LCD */
static RingBuffer_t FromHost_Buffer;

/** Underlying data buffer for \ref FromHost_Buffer, where the stored bytes are located. */
static uint8_t  FromHost_Buffer_Data[128];

char cmd[100];
int cmd_cnt;

/** LUFA HID Class driver interface configuration and state information. This structure is
 *  passed to all HID Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_HID_Device_t Digitizer_HID_Interface =
{
	.Config =
		{
			.InterfaceNumber                = 2,
			.ReportINEndpoint               =
				{
					.Address                = DIGITIZER_EPADDR,
					.Size                   = DIGITIZER_EPSIZE,
					.Banks                  = 1,
				},
			.PrevReportINBuffer             = PrevDigitizerHIDReportBuffer,
			.PrevReportINBufferSize         = sizeof(PrevDigitizerHIDReportBuffer),
		},
};


/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void){

	SetupHardware();

	RingBuffer_InitBuffer(&FromHost_Buffer, FromHost_Buffer_Data, sizeof(FromHost_Buffer_Data));

	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
	GlobalInterruptEnable();

	for (;;){
		HandleSerial();		
		HandleDigitizer();

		CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
		HID_Device_USBTask(&Digitizer_HID_Interface);
		USB_USBTask();
	}
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void){
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);

	/* Hardware Initialization */
	LEDs_Init();
	USB_Init();
	EEPROM_Init();
	LCD_Init();
	//Digitizer_Init();
	BL_on(128);		
		
	DDRC  |= PIN_PD;  // Powerdown 
	PORTC |= PIN_PD;  // High for normal operation
	DDRB  |= PIN_PDO; // Powerdown outputs
	PORTB |= PIN_PDO; // High for normal operation
}


/** Checks for changes in the position of the board joystick, sending strings to the host upon each change. */
void HandleSerial(void){
	int err;

	/* Only try to read in bytes from the CDC interface if the transmit buffer is not full */
	if (!(RingBuffer_IsFull(&FromHost_Buffer))){
		int16_t ReceivedByte = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);
		/* Read bytes from the USB OUT endpoint into the USART transmit buffer */
		if (!(ReceivedByte < 0)){
		  	RingBuffer_Insert(&FromHost_Buffer, ReceivedByte);
			CDC_Device_SendByte(&VirtualSerial_CDC_Interface, ReceivedByte);
		}
	}

	while (RingBuffer_GetCount(&FromHost_Buffer) > 0){
		int16_t c = RingBuffer_Remove(&FromHost_Buffer);
		
		if(c == '\n' || c == '\r'){
			if(cmd_cnt > 0 && (cmd[cmd_cnt-1] == '\n'))
				cmd_cnt--;
			cmd[cmd_cnt] = 0;			
			err = execute_command();
			cmd_cnt = 0;			
		}
		else{
			cmd[cmd_cnt++] = c;			
		}
	}
}

/* Send a string via virtual serial port */ 
void sendString(char* s, int flush){
	CDC_Device_SendString(&VirtualSerial_CDC_Interface, s);
	if(flush)
		CDC_Device_Flush(&VirtualSerial_CDC_Interface);
}


/** Execute a command received via virtual serial */
int execute_command(void){
	if(strcmp(cmd, "help") == 0)
		sendString("No help here, google it!\r\n", 0);
	if(strcmp(cmd, "init_tp") == 0){
		sendString("Initializing digitizer..\r\n", 0);
		Digitizer_Init();
		sendString("Done!\r\n", 0);
	}

	sendString(">", 1);

	return 0;
}

/** Get data from the touch screen digitizer */
void HandleDigitizer(void){

}

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void){
	LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void){
	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void){
	bool ConfigSuccess = true;

	ConfigSuccess &= HID_Device_ConfigureEndpoints(&Digitizer_HID_Interface);
	ConfigSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);

	USB_Device_EnableSOFEvents();

	LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void){
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
	HID_Device_ProcessControlRequest(&Digitizer_HID_Interface);
}

/** Event handler for the USB device Start Of Frame event. */
void EVENT_USB_Device_StartOfFrame(void){
	HID_Device_MillisecondElapsed(&Digitizer_HID_Interface);
}

/** HID class driver callback function for the creation of HID reports to the host.
 *
 *  \param[in]     HIDInterfaceInfo  Pointer to the HID class interface configuration structure being referenced
 *  \param[in,out] ReportID    Report ID requested by the host if non-zero, otherwise callback should set to the generated report ID
 *  \param[in]     ReportType  Type of the report to create, either HID_REPORT_ITEM_In or HID_REPORT_ITEM_Feature
 *  \param[out]    ReportData  Pointer to a buffer where the created report should be stored
 *  \param[out]    ReportSize  Number of bytes written in the report (or zero if no report is to be sent)
 *
 *  \return Boolean true to force the sending of the report, false to let the library determine if it needs to be sent
 */
bool CALLBACK_HID_Device_CreateHIDReport(USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
                                         uint8_t* const ReportID,
                                         const uint8_t ReportType,
                                         void* ReportData,
                                         uint16_t* const ReportSize)
{

	USB_DigitizerReport_Data_t* DigitizerReport = (USB_DigitizerReport_Data_t*)ReportData;

    if (1) 
    {
        DigitizerReport->X = 100; 
        DigitizerReport->Y = 150; 
        DigitizerReport->Finger =0x03; 
        DigitizerReport->Temp= 0x00; 
    } 

    *ReportSize = sizeof(USB_DigitizerReport_Data_t);
    return true;
}

/** HID class driver callback function for the processing of HID reports from the host.
 *
 *  \param[in] HIDInterfaceInfo  Pointer to the HID class interface configuration structure being referenced
 *  \param[in] ReportID    Report ID of the received report from the host
 *  \param[in] ReportType  The type of report that the host has sent, either HID_REPORT_ITEM_Out or HID_REPORT_ITEM_Feature
 *  \param[in] ReportData  Pointer to a buffer where the received report has been stored
 *  \param[in] ReportSize  Size in bytes of the received HID report
 */
void CALLBACK_HID_Device_ProcessHIDReport(USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
                                          const uint8_t ReportID,
                                          const uint8_t ReportType,
                                          const void* ReportData,
                                          const uint16_t ReportSize)
{
	// Unused (but mandatory for the HID class driver) in this demo, since there are no Host->Device reports
}

