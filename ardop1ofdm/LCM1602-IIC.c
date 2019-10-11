#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "LCM1602-IIC.h"

void expanderWrite(int file, char value)
{
	char buffer = value | LCD_BACKLIGHT;
	//printf("EW = %x\r\n", buffer);
	if (write(file, &buffer, 1) != 1) 
		printf("Error escribiendo en el dispositivo.\r\n");
}

void pulseEnable(int file, char value)
{
	expanderWrite(file, value | LCD_EN);
	usleep(1);

	expanderWrite(file, value & ~LCD_EN);
	usleep(50);
}

void write4bits(int file, char value)
{
	//printf("\r\n");
	//printf("W4B\r\n");
	expanderWrite(file, value);
	pulseEnable(file, value);
	//printf("\r\n");
}

void send(int file, char value, char mode)
{
	//printf("\r\nSEND\r\n");
	char h = value & 0xf0;
	char l = (value << 4) & 0xf0;
	write4bits(file, h | mode);
	write4bits(file, l | mode);
}

void command(int file, char value)
{
	send(file, value, 0);
}

int initialize(const char *i2c_device, int addr)
{
	// Se abre el fichero del dispositivo
	int file = 0;
	if ((file = open(i2c_device, O_RDWR)) < 0) {
		printf("No se pudo abrir el dispositivo i2c: %s\r\n", i2c_device);
		return -1;
	}

	if (ioctl(file, I2C_SLAVE, addr) != 0) {
		printf("No se ha podido seleccionar la dirección del esclavo\r\n");
		return -1;
	}

	usleep(50000);
	expanderWrite(file, LCD_BACKLIGHT);
	usleep(1000000);

	// Se comienza en modo 4 bit, intentamos poner en modo 4 bit
	write4bits(file, 0x03 << 4);
	usleep(4500);
	write4bits(file, 0x30);
	usleep(4500);
	write4bits(file, 0x30);
	usleep(150);

	// Finalmente se pone el interface en 4 bit
	write4bits(file, 0x20);

	// Se configura el número de líneas
	command(file, LCD_FUNCTIONSET | LCD_2LINE);
	command(file, LCD_DISPLAYCONTROL | LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF);
	clear(file);

	// Se inicializa la dirección del texto por defecto
	command(file, LCD_ENTRYMODESET | LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT);

	// Cursor al inicio
	home(file);

	return file;
}

void finalize(int file)
{
	close(file);
}

void clear(int file)
{
	command(file, LCD_CLEARDISPLAY);
	usleep(2000);
}

void home(int file)
{
	command(file, LCD_RETURNHOME);
	usleep(2000);
}

void locate(int file, int row, int col)
{
	static int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
	command(file, LCD_SETDDRAMADDR | ((col % 16) + row_offsets[row % 2])); 
}

void locateCG(int file, int n)
{
	command(file, LCD_SETCGRAMADDR + n);
}
                

void print(int file, const char *text)
{
	int i = 0;
	int tlen = strlen(text);
	for (i = 0; i < tlen; i++) 
		send(file, text[i], LCD_RS);
}

