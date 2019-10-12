#include "ARDOPC.h"

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


#define LCD_EN 0x04	// Enable bit
#define LCD_RW 0x02	// Read/Write bit
#define LCD_RS 0x01	// Register select bit



void clear(int i2cfile);
void home(int i2cfile);
void locate(int i2cfile, int row, int col);
void print(int i2cfile, const char *text);
int initialize(const char *i2c_device, int addr);
void finalize(int i2cfile);

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>


#define I2C_SLAVE	0x0703	// Change slave address			


//#include <sys/ioctl.h>

int i2cfile = 0;

void expanderWrite(char value)
{
	char buffer = value | LCD_BACKLIGHT;

	if (i2cfile)
	{
		if (write(i2cfile, &buffer, 1) != 1) 
		{
			printf("i2c write failed - disabling i2c display\r\n");
			i2cfile = 0;
		}
	}
}

void pulseEnable(int i2cfile, char value)
{
	expanderWrite(value | LCD_EN);
	usleep(1);

	expanderWrite(value & ~LCD_EN);
	usleep(50);
}

void write4bits(int i2cfile, char value)
{
	expanderWrite(value);
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
	int i2cfile = 0;
	if ((i2cfile = open(i2c_device, O_RDWR)) < 0) {
		printf("Can't open i2c: %s\r\n", i2c_device);
		return -1;
	}

	if (ioctl(i2cfile, I2C_SLAVE, addr) != 0) {
		printf("Can't set I2C Slave Address\r\n");
		return -1;
	}

	usleep(50000);
	expanderWrite(LCD_BACKLIGHT);
	usleep(100000);

	// Se comienza en modo 4 bit, intentamos poner en modo 4 bit

	write4bits(i2cfile, 0x03 << 4);
	usleep(4500);
	write4bits(i2cfile, 0x30);
	usleep(4500);
	write4bits(i2cfile, 0x30);
	usleep(150);

	// Finalmente se pone el interface en 4 bit

	write4bits(i2cfile, 0x20);

	// Sert 2 Lines

	command(i2cfile, LCD_FUNCTIONSET | LCD_2LINE);
	command(i2cfile, LCD_DISPLAYCONTROL | LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF);
	clear(i2cfile);

	// Se inicializa la dirección del texto por defecto
	command(i2cfile, LCD_ENTRYMODESET | LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT);

	// Cursor al inicio
	home(i2cfile);

	return i2cfile;
}

void finalize(int i2cfile)
{
	close(i2cfile);
}

void clear(int i2cfile)
{
	command(i2cfile, LCD_CLEARDISPLAY);
	usleep(2000);
}

void home(int i2cfile)
{
	command(i2cfile, LCD_RETURNHOME);
	usleep(2000);
}

void locate(int i2cfile, int row, int col)
{
	static int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
	command(i2cfile, LCD_SETDDRAMADDR | ((col % 16) + row_offsets[row % 2])); 
}

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

//	Signal Level uses a Half block defined as char 1

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


void displayState(const char * State)
{
	if (i2cfile)
	{
		locate(i2cfile, 1, 0);
		print(i2cfile, "        ");
		locate(i2cfile, 1, 0);
		print(i2cfile, State);
	}
}


void displayLevel(int max)
{
	if (i2cfile)
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
}

void displayCall(int dirn, char * Call)
{
	if (i2cfile)
	{
		char paddedcall[12] = "           ";

		paddedcall[0] = dirn;
		memcpy(paddedcall+1, Call, strlen(Call));

		locate(i2cfile, 0, 0);
		print(i2cfile, paddedcall);
	}
}

int initdisplay()
{
#ifdef I2CDISPLAY

  	i2cfile = initialize("/dev/i2c-1", 0X27);

	//	Set font for half block for sig level display

	locateCG(i2cfile, 0);
	print(i2cfile, "\x7\x7\x7\x7\x7\x7\x7\x7");
	print(i2cfile, "\x1c\x1c\x1c\x1c\x1c\x1c\x1c\x1c");

#endif                                     
}

