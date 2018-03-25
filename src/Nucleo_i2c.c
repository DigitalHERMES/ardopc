#include <stm32f4xx.h>
#include <stm32f4xx_i2c.h>
#include "stm32f4xx_gpio.h"
#include <stm32f4xx_rcc.h>

#include "ARDOPC.h"

	/* setup SCL and SDA pins
	 * You can connect I2C1 to two different
	 * pairs of pins:
	 * 1. SCL on PB6 and SDA on PB7
	 * 2. SCL on PB8 and SDA on PB9
	 */

void init_I2C1(void){

	GPIO_InitTypeDef GPIO_InitStruct;
	I2C_InitTypeDef I2C_InitStruct;

	// enable APB1 peripheral clock for I2C1
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
	// enable clock for SCL and SDA pins
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

	GPIO_StructInit(&GPIO_InitStruct);
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9; // PB8 and PB9
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;			// set pins to alternate function
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;		// set GPIO speed
	GPIO_InitStruct.GPIO_OType = GPIO_OType_OD;			// set output to open drain --> the line has to be only pulled low, not driven high |
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;			// enable pull up resistors  <---------------------<-------------------------------<|
	GPIO_Init(GPIOB, &GPIO_InitStruct);					// init GPIOB

	// Connect I2C1 pins to AF
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource8, GPIO_AF_I2C1);	// SCL
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource9, GPIO_AF_I2C1); // SDA

	// configure I2C1
	I2C_StructInit(&I2C_InitStruct);
	I2C_InitStruct.I2C_ClockSpeed = 100000; 		// 100kHz(standard) vs 400
	I2C_InitStruct.I2C_Mode = I2C_Mode_I2C;			// I2C mode
	I2C_InitStruct.I2C_DutyCycle = I2C_DutyCycle_2;	// 50% duty cycle --> standard
	I2C_InitStruct.I2C_OwnAddress1 = 0x00;			// own address, not relevant in master mode
	I2C_InitStruct.I2C_Ack = I2C_Ack_Disable;		// disable acknowledge when reading (can be changed later on)
	I2C_InitStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit; // set address length to 7 bit addresses
	I2C_Init(I2C1, &I2C_InitStruct);				// init I2C1

	// enable I2C1
	I2C_Cmd(I2C1, ENABLE); //sets PE bit in CR1, at end`
}
/* This function issues a start condition and2
 * transmits the slave address + R/W bit
 *
 * Parameters:
 * 		I2Cx --> the I2C peripheral e.g. I2C1
 * 		address --> the 7 bit slave address
 * 		direction --> the tranmission direction can be:
 * 						I2C_Direction_Tranmitter for Master transmitter mode
 * 						I2C_Direction_Receiver for Master receiver
 */
void I2C_start(I2C_TypeDef* I2Cx, uint8_t address, uint8_t direction){
	// wait until I2C1 is not busy anymore

	while(I2C_GetFlagStatus(I2Cx, I2C_FLAG_BUSY));

	// Send I2C1 START condition
	I2C_GenerateSTART(I2Cx, ENABLE);

	// wait for I2C1 EV5 --> Slave has acknowledged start condition
	while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT));

	// Send slave Address for write
	I2C_Send7bitAddress(I2Cx, address, direction);

	/* wait for I2C1 EV6, check if
	 * either Slave has acknowledged Master transmitter or
	 * Master receiver mode, depending on the transmission
	 * direction
	 */
	if(direction == I2C_Direction_Transmitter){
		while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));
	}
	else if(direction == I2C_Direction_Receiver){
		while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));
	}
}

/* This function transmits one byte to the slave device
 * Parameters:
 *		I2Cx --> the I2C peripheral e.g. I2C1
 *		data --> the data byte to be transmitted
 */
void I2C_write(I2C_TypeDef* I2Cx, uint8_t data){
	I2C_SendData(I2Cx, data);
	// wait for I2C1 EV8_2 --> byte has been transmitted
	while(!I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
}

/* This function reads one byte from the slave device
 * and acknowledges the byte (requests another byte)
 */
uint8_t I2C_read_ack(I2C_TypeDef* I2Cx){
	// enable acknowledge of recieved data
	I2C_AcknowledgeConfig(I2Cx, ENABLE);
	// wait until one byte has been received
	while( !I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_RECEIVED) );
	// read data from I2C data register and return data byte
	uint8_t data = I2C_ReceiveData(I2Cx);
	return data;
}

/* This function reads one byte from the slave device
 * and doesn't acknowledge the recieved data
 */
uint8_t I2C_read_nack(I2C_TypeDef* I2Cx){
	// disabe acknowledge of received data
	I2C_AcknowledgeConfig(I2Cx, DISABLE);
	// wait until one byte has been received
	while( !I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_RECEIVED) );
	// read data from I2C data register and return data byte
	uint8_t data = I2C_ReceiveData(I2Cx);
	return data;
}

/* This funtion issues a stop condition and therefore
 * releases the bus
 */
void I2C_stop(I2C_TypeDef* I2Cx){
	// Send I2C1 STOP Condition
	I2C_GenerateSTOP(I2Cx, ENABLE);
}


// Display Interface

unsigned short  i2c_ADDR = 0x4E; // Display address

// COMMANDOS
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80
#define LCD_BACKLIGHT 0x08
#define LCD_NOBACKLIGHT 0x00

// FLAGS PARA EL MODO DE ENTRADA
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// FLAGS DE DISPLAY CONTROL
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// FLAGS DE FUNCTION SET
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00


#define LCD_EN 0x04 // Enable bit
#define LCD_RW 0x02 // Read/Write bit
#define LCD_RS 0x01 // Register select bit



void clear(int i2cfile);
void home(int i2cfile);
void locate(int i2cfile, int row, int col);
void print(int i2cfile, const char *text);
int initialize(const char *i2c_device, int addr);
void finalize(int i2cfile);

//#include <sys/ioctl.h>

int i2cfile;

void usleep(float usec)
{
    wait(usec/1000000);
}


void expanderWrite(int i2cfile, char value)
{
    char buffer = value | LCD_BACKLIGHT;

    I2C_start(I2C1, i2c_ADDR, I2C_Direction_Transmitter);
    I2C_write(I2C1, buffer);
    I2C_stop(I2C1);
}

void pulseEnable(int i2cfile, char value)
{
    expanderWrite(i2cfile, value | LCD_EN);
//    usleep(1);

    expanderWrite(i2cfile, value & ~LCD_EN);
//    usleep(50);
}

void write4bits(int i2cfile, char value)
{
    expanderWrite(i2cfile, value);
    pulseEnable(i2cfile, value);
}

void i2csend(int i2cfile, char value, char mode)
{
    char h = value & 0xf0;
    char l = (value << 4) & 0xf0;
    write4bits(i2cfile, h | mode);
    write4bits(i2cfile, l | mode);
}

void command(int i2cfile, char value)
{
    i2csend(i2cfile, value, 0);
}

int initialize(const char *i2c_device, int addr)
{
    expanderWrite(i2cfile, LCD_BACKLIGHT);

    write4bits(i2cfile, 0x03 << 4);

    write4bits(i2cfile, 0x30);
//    usleep(4500);
    write4bits(i2cfile, 0x30);
 //   usleep(150);

    write4bits(i2cfile, 0x20);

    // Set 2 Lines

    command(i2cfile, LCD_FUNCTIONSET | LCD_2LINE);
    command(i2cfile, LCD_DISPLAYCONTROL | LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF);
    clear(i2cfile);
    command(i2cfile, LCD_ENTRYMODESET | LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT);
    home(i2cfile);

    return i2cfile;
}

void finalize(int i2cfile)
{
}

void clear(int i2cfile)
{
    command(i2cfile, LCD_CLEARDISPLAY);
}

void home(int i2cfile)
{
    command(i2cfile, LCD_RETURNHOME);
}

void locate(int i2cfile, int row, int col)
{
    static int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
    command(i2cfile, LCD_SETDDRAMADDR | ((col % 16) + row_offsets[row % 2]));
}

//	Point to Character Generator RAM

void locateCG(int i2cfile, int n)
{
    command(i2cfile, LCD_SETCGRAMADDR + n);
}


void print(int i2cfile, const char *text)
{
    int i = 0;
    int tlen = strlen(text);
    for (i = 0; i < tlen; i++)
        i2csend(i2cfile, text[i], LCD_RS);
}

//  Signal Level uses a Half block defined as char 1
//	Use 5 chars to show 10 levels

const char level[10][5] = {
        {1,32,32,32,32},
        {255,32,32,32,32},
        {255,1,32,32,32},
        {255,255,32,32,32},
        {255,255,1,32,32},
        {255,255,255,32,32},
        {255,255,255,1,32},
        {255,255,255,255,32},
        {255,255,255,255,1},
        {255,255,255,255,255}};


void displayState()
{
//	printtick("enter displaystate");
    locate(i2cfile, 1, 0);
    print(i2cfile, "        ");
    locate(i2cfile, 1, 0);
    print(i2cfile, ARDOPStates[ProtocolState]);
//	printtick("exit displaystate");
}


void displayLevel(int max)
{
    int i, j;

    i = max/3276;

    if (i > 9)
        i = 9;

    locate(i2cfile, 1, 11);

    for (j= 0; j < 5; j++)
    {
        i2csend(i2cfile, level[i][j], LCD_RS);
    }
}

void displayCall(int dirn, char * Call)
{
	char paddedcall[12] = "           ";

	paddedcall[0] = dirn;
	memcpy(paddedcall+1, Call, strlen(Call));

    locate(i2cfile, 0, 0);
    print(i2cfile, paddedcall);
}


void initdisplay()
{
    i2cfile = initialize("/dev/i2c-1", 0X27);

    Debugprintf("Returned from Init");

    //      Set font for half bar display for sig level

    locateCG(i2cfile, 0);
    print(i2cfile, "\x7\x7\x7\x7\x7\x7\x7\x7");
    print(i2cfile, "\x1c\x1c\x1c\x1c\x1c\x1c\x1c\x1c");
}







