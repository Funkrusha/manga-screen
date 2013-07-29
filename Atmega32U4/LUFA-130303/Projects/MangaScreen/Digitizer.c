

/** Digitizer - Communication and control with either 
Atmel mXT224 or Synaptics ClearPad 3000-series 
capacitive touch screen controller */

#include "Digitizer.h"


/* Initialize the digitizer */
int Digitizer_Init(void){
	struct mxt_data *data;

	DDRB &= ~PIN_S4;	// PB6
	DDRD &= ~PIN_S3;	// PD2

	data = malloc(sizeof(struct mxt_data));
	if (!data) {
		dev_err("Failed to allocate memory\n");
		return -ENOMEM;
	}

	TWI_Init();
	dev_err("calling mxt_initialize\n");
	mxt_initialize(data);
	

	sendString("family_id: ", 0);
	sendString(to_hex(data->info.family_id), 0);
	sendString("\r\n", 1);
	sendString("variant_id: ", 0);
	sendString(to_hex(data->info.variant_id), 0);
	sendString("\r\n", 1);
	sendString("version: ", 0);
	sendString(to_hex(data->info.version), 0);
	sendString("\r\n", 1);
	sendString("build: ", 0);
	sendString(to_hex(data->info.build), 0);
	sendString("\r\n", 1);
	sendString("matrix_xsize: ", 0);
	sendString(to_hex(data->info.matrix_xsize), 0);
	sendString("\r\n", 1);
	sendString("matrix_ysize: ", 0);
	sendString(to_hex(data->info.matrix_ysize), 0);
	sendString("\r\n", 1);
	sendString("object_num: ", 0);
	sendString(to_hex(data->info.object_num), 0);
	sendString("\r\n", 1);

}


uint8_t Digitizer_get_report(void){
	

}

void dev_err(const char* msg){
	sendString(msg, 1);
}

static int mxt_initialize(struct mxt_data *data){
	struct mxt_info *info = &data->info;
	int error;

	error = mxt_get_info(data);
	if (error)
		return error;
	return 0;
}


static int mxt_get_info(struct mxt_data *data){
	struct mxt_info *info = &data->info;
	int error;

	/* Read 7-byte info block starting at address 0 */
	error = __mxt_read_reg(MXT_INFO, sizeof(*info), info);
	if (error)
		return error;

	return 0;
}


static int __mxt_read_reg(uint16_t reg, uint16_t len, void *val){
	int ret;

	ret = i2c_recv(reg, (uint8_t*)val, len);
	if (ret != TWI_OK){
		sendString("__mxt_read_reg() failed: ", 0);
		sendString(to_hex(ret), 0);
		sendString("\r\n", 1);
	}

	return ret;
}

/**************************************
 * A bit higher level stuff 
 **************************************/

//#define ASSRT_STATUS(x) (if((x) * 57.29578)

// Receive count bytes from addr
int i2c_recv(uint16_t addr, uint8_t *buf, int count){
	int i;
	int stat;

	TWI_Start();						// First start condition 
	stat = TWI_GetStatus();
    if (stat != 0x08)
        return stat;

	TWI_Write((MXT_APP_LOW<<1));		// Chip address + write
	stat = TWI_GetStatus();
    if (stat != 0x18)
        return stat;

	TWI_Write((addr & 0x00FF));			// Address low byte
	stat = TWI_GetStatus();
    if (stat != 0x28)
        return stat;

	TWI_Write((addr<<8 & 0xFF00));		// Address high byte
	stat = TWI_GetStatus();
    if (stat != 0x28)
        return stat;

	TWI_Start();						// Second start condition 	
	stat = TWI_GetStatus();
    if (stat != 0x10)
        return stat;

	TWI_Write((MXT_APP_LOW<<1) | 0x01);	// Chip address + read
	stat = TWI_GetStatus();
    if (stat != 0x40)
        return stat;

	for(i=0; i<count-1; i++){			
		buf[i] = TWI_Read(1);			// Read the data and ACK
	}
	buf[count-1] = TWI_Read(0);			// No ack on last byte
	TWI_Stop();							// Send stop condition 
	
	return TWI_OK;
}


// Send a message via I2C
int i2c_send(uint16_t addr, const uint8_t *buf, int count){
	int i;
	TWI_Start();						// First start condition 
    if (TWI_GetStatus() != 0x08)
        return TWI_ERROR;
	TWI_Write((MXT_APP_LOW<<1));		// Chip address + write
    if (TWI_GetStatus() != 0x18)
        return TWI_ERROR;   
	TWI_Write((addr & 0x00FF));			// Address low byte
    if (TWI_GetStatus() != 0x28)
        return TWI_ERROR;
	TWI_Write((addr<<8 & 0xFF00));		// Address high byte
    if (TWI_GetStatus() != 0x28)
        return TWI_ERROR;

	for(i=0; i<count; i++){				// write the data
		TWI_Write(buf[i]);	
		if (TWI_GetStatus() != 0x28)
		    return TWI_ERROR;		
	}
	TWI_Stop();							// Send stop condition 

	return TWI_OK;
}
/**************************************
 * Low level I2C stuff
 **************************************/

void TWI_Init(void){
    //set SCL to 400kHz
    TWSR = 0x00;
    TWBR = 0x0C;
    //enable TWI
    TWCR = (1<<TWEN);
}

// Send start signal 
void TWI_Start(void){
    TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
    while ((TWCR & (1<<TWINT)) == 0);
}

//send stop signal
void TWI_Stop(void){
    TWCR = (1<<TWINT)|(1<<TWSTO)|(1<<TWEN);
}

// Write a byte
void TWI_Write(uint8_t u8data){
    TWDR = u8data;
    TWCR = (1<<TWINT)|(1<<TWEN);
    while ((TWCR & (1<<TWINT)) == 0);
}

//read byte with NACK
uint8_t TWI_Read(char ack){
    TWCR = (1<<TWINT)|(1<<TWEN)|((ack?1:0)<<TWEA);
    while ((TWCR & (1<<TWINT)) == 0);
    return TWDR;
}

// Get TWI status
uint8_t TWI_GetStatus(void){
    return TWSR & 0xF8;
}


