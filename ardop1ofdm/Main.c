#include <stdio.h>

#include "LCM1602-IIC.h"

char level[10][5] = {
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
        
int main(int argc, const char *argv[])
{
        int i, j;	
	if (argc != 3) {
		printf("Ejemplo: LCD /dev/i2c-0 32\r\n");
		return -1;
	}
	printf("Using device %s 0x%x\r\n", argv[1], atoi(argv[2]));

	int file = initialize(argv[1], atoi(argv[2]));

	locateCG(file, 0);
        print(file, "\x7\x7\x7\x7\x7\x7\x7\x7");
        print(file, "\x1c\x1c\x1c\x1c\x1c\x1c\x1c\x1c");
                                               
	locate(file, 0, 0);
	print(file, "\x7fGM8BPQ-10 ");
	locate(file, 1, 0);
	print(file, "ISS");
	
	while (1)
	{
 	for (i = 0; i < 10; i++)
	{
	        locate(file, 1, 11);
       	        for (j= 0; j < 5; j++)
	        {
	                 send(file, level[i][j], LCD_RS);
                }
                usleep(600000);
        }
        }
	finalize(file);
}
