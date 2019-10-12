//
// This code calculates and writes to files the templates
// used to generate modulation samples.

// Rick's code gererates them dynamically as program start, but
// that measns they have to be in RAM. By pregenerating and 
// compliling them they can be placed in program space
// This is necessary with the small RAM space of embedded CPUs

// This only needs to be run once to generate the source files

// Keep code in case we need to change, but don't compile

#include "ARDOPC.h"

#pragma warning(disable : 4244)		// Code does lots of int float to int


static int intAmp = 26000;	   // Selected to have some margin in calculations with 16 bit values (< 32767) this must apply to all filters as well. 

#if 0
void Generate50BaudTwoToneLeaderTemplate()
{
	int i;
	float x, y, z;
	int line = 0;

	FILE * fp1;

	char msg[80];
	int len;

	fp1 = fopen("s:\\leadercoeffs.txt", "wb");

	for (i = 0; i < 240; i++)
	{
		y = (sin(((1500.0 - 25) / 1500) * (i / 8.0 * 2 * M_PI)));
		z = (sin(((1500.0 + 25) / 1500) * (i / 8.0 * 2 * M_PI)));

		x = intAmp * 0.55 * (y - z);
		int50BaudTwoToneLeaderTemplate[i] = (short)x + 0.5;

		if ((i - line) == 9)
		{
			// print the last 10 values

			len = sprintf(msg, "\t%d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
				int50BaudTwoToneLeaderTemplate[line],
				int50BaudTwoToneLeaderTemplate[line + 1],
				int50BaudTwoToneLeaderTemplate[line + 2],
				int50BaudTwoToneLeaderTemplate[line + 3],
				int50BaudTwoToneLeaderTemplate[line + 4],
				int50BaudTwoToneLeaderTemplate[line + 5],
				int50BaudTwoToneLeaderTemplate[line + 6],
				int50BaudTwoToneLeaderTemplate[line + 7],
				int50BaudTwoToneLeaderTemplate[line + 8],
				int50BaudTwoToneLeaderTemplate[line + 9]);

			line = i + 1;

			fwrite(msg, 1, len, fp1);
		}
	}		
	fclose(fp1);
}

// Subroutine to create the FSK symbol templates

void GenerateFSKTemplates()
{
	// Generate templates of 240 samples (each symbol template = 20 ms) for each of the 19 possible carriers used in 500 Hz and 2000 Hz BW 50 Baud 4FSK

	// Total of 4 groups of 4 carriers/group (each group represents 2 bits using 4FSK modulation)
	// Groups 1, 2, 4, 5 are used for 2000 Hz 4Carrier 4FSK modes
	// Group 3 is used for 500 Hz 4FSK Data mode and 4FSK modulation of frame types


	float dblCarFreq[] = {1100, 1200, 1300, 1400, 1350, 1450, 1550, 1650, 1600, 1700, 1800, 1900};
	float dblAngle;		// Angle in radians
	float dblCarPhaseInc[20]; 
	int i, k;

	char msg[256];
	int len;
	int line = 0;
	FILE * fp1;

	// Compute the phase inc per sample

    for (i = 0; i < 12; i++) 
	{
		dblCarPhaseInc[i] = 2 * M_PI * dblCarFreq[i] / 12000;
	}
	
	// Now compute the templates: (4800 16 bit values total) 
	
	for (i = 0; i < 12; i++)			// across the 4 tones for 50 baud frequencies
	{
		dblAngle = 0;
		// 50 baud template

		line = 0;

		for (k = 0; k < 240; k++)	// for 240 samples (one 50 baud symbol)
		{
			intFSK50bdCarTemplate[i][k] = intAmp * 1.1 * sin(dblAngle);  // with no envelope control (factor 1.1 chosen emperically to keep FSK peak amplitude slightly below 2 tone peak)
		
			dblAngle += dblCarPhaseInc[i];

			if (dblAngle >= 2 * M_PI)
				dblAngle -= 2 * M_PI;

		}
	}


	fp1 = fopen("d:\\fskcoeffs100.txt", "wb");

	len = sprintf(msg, "CONST short intFSK50bdCarTemplate[12][240] = {\r\n");
	fwrite(msg, 1, len, fp1);

	len = sprintf(msg, "\t{{\r\n");
	fwrite(msg, 1, len, fp1);

	for (i = 0; i < 12; i++)		// across 9 tones
	{
			line = 0;

			for (k = 0; k <= 239; k++) // for 120 samples (one 100 baud symbol, 200 baud modes will just use half of the data)
			{
				if ((k - line) == 9)
				{
					// print 10 to line

					len = sprintf(msg, "\t%d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
					intFSK50bdCarTemplate[i][line],
					intFSK50bdCarTemplate[i][line + 1],
					intFSK50bdCarTemplate[i][line + 2],
					intFSK50bdCarTemplate[i][line + 3],
					intFSK50bdCarTemplate[i][line + 4],
					intFSK50bdCarTemplate[i][line + 5],
					intFSK50bdCarTemplate[i][line + 6],
					intFSK50bdCarTemplate[i][line + 7],
					intFSK50bdCarTemplate[i][line + 8],
					intFSK50bdCarTemplate[i][line + 9]);


					line = k + 1;

					if (k == 239)
					{
						len += sprintf(&msg[len-2], "},\r\n\r\n\t{");
						len -=2;
					}
					fwrite(msg, 1, len, fp1);
				}
	
		}
	}

	len = sprintf(msg, "\t}};\r\n");
	fwrite(msg, 1, len, fp1);

	fclose(fp1);


}

//	 Subroutine to initialize valid frame types 


//	Subroutine to create the PSK symbol templates for 8 tones and 8 phases at 200 baud
float round(float x);

VOID Generate16QAMTemplates()
{

	// Generate templates of 60 samples (each template = 20 ms) for each of the 10 possible carriers used in 16QAM modulation in a 12,4 circle. 
	// Used to speed up computation of PSK frames and reduce use of Sin functions.
	// Amplitude values will have to be scaled based on the number of Active Carriers (2 or 8) initial values should be OK for 1 carrier
	// Tone values 

	// the carrier frequencies in Hz
	
	float dblCarFreq[] = {600, 800, 1000, 1200, 1400, 1500, 1600, 1800, 2000, 2200, 2400};

	// for 1 carrier mode (200 Hz) use index 5 (1500 Hz)
	// for 2 carrier (500 Hz) modes use indexes 4, 6(1400 and 1600 Hz)
	// for 10 carrier modes use index 0-4, 6-10  (600, 800, 1000, 1200, 1400, 1600, 1800, 2000, 2200, 2400 Hz) 
 
	float dblCarPhaseInc[11];	// the phase inc per sample based on frequency
	float dblAngle;				// Angle in radians
	int i, j, k;


	char msg[256];
	int len;
	int line = 0;
	FILE * fp1;

	
	// Compute the phase inc per sample
	
	for (i = 0; i < 11; i++)
	{
		dblCarPhaseInc[i] = 2 * M_PI * dblCarFreq[i] / 12000.0f;
	}
	
	//Now compute the templates: ( 32 bit values total)  
	
	for (i = 0; i < 11; i++) // across 11 tones
	{
		for (j = 0; j < 4; j++) //  using only half the phase values (0, 45, 90, 135)  (use sign compliment for the opposit phase) 
		{
			dblAngle = j * M_PI / 4;
			
			for (k = 0; k < 120; k++) // for 120 samples (one 100 baud symbol also used to generate 50 baud using mod 120) 
			{
				intQAM50bdCarTemplate[i][j][k] = intAmp * sinf(dblAngle);  // with no envelope control
				
				dblAngle += dblCarPhaseInc[i];
				if (dblAngle >= 2 * M_PI)
					dblAngle -= 2 * M_PI;
			}
		}
	}

	
	fp1 = fopen("d:\\PSKcoeffs.txt", "wb");

	len = sprintf(msg, "\tCONST short intQAM50bdCarTemplate[11][4][120]; = \r\n");
	fwrite(msg, 1, len, fp1);

	len = sprintf(msg, "\t{{{\r\n");
	fwrite(msg, 1, len, fp1);

	for (i = 0; i <= 10; i++)		// across 11 tones
	{
		for (j = 0; j <= 3; j++)
		{
			line = 0;
		
			len = sprintf(msg, "\r\n// Carrier %d Phase %d\r\n", i, j);

			fwrite(msg, 1, len, fp1);
		

			for (k = 0; k <= 119; k++) // for 120 samples (one 100 baud symbol, 200 baud modes will just use half of the data)
			{
				if ((k - line) == 9)
				{
					// print 10 to line

					len = sprintf(msg, "\t%d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
					intQAM50bdCarTemplate[i][j][line],
					intQAM50bdCarTemplate[i][j][line + 1],
					intQAM50bdCarTemplate[i][j][line + 2],
					intQAM50bdCarTemplate[i][j][line + 3],
					intQAM50bdCarTemplate[i][j][line + 4],
					intQAM50bdCarTemplate[i][j][line + 5],
					intQAM50bdCarTemplate[i][j][line + 6],
					intQAM50bdCarTemplate[i][j][line + 7],
					intQAM50bdCarTemplate[i][j][line + 8],
					intQAM50bdCarTemplate[i][j][line + 9]);

					line = k + 1;

					if (k == 119)
					{
						len += sprintf(&msg[len-2], "},\r\n\t{");
						len -=2;
					}
					fwrite(msg, 1, len, fp1);
				}
			}
//			len = sprintf(msg, "\t}{\r\n");
//			fwrite(msg, 1, len, fp1);
		}
		len = sprintf(msg, "\t}}{{\r\n");
		fwrite(msg, 1, len, fp1);
	}

	len = sprintf(msg, "\t}}};\r\n");
	fwrite(msg, 1, len, fp1);

	fclose(fp1);

}  

VOID Generate16QAMTemplates()
{

	// Generate templates of 60 samples (each template = 20 ms) for each of the 10 possible carriers used in 16QAM modulation in a 12,4 circle. 
	// Used to speed up computation of PSK frames and reduce use of Sin functions.
	// Amplitude values will have to be scaled based on the number of Active Carriers (2 or 8) initial values should be OK for 1 carrier
	// Tone values 

	// the carrier frequencies in Hz
	
	float dblCarFreq[] = {600, 800, 1000, 1200, 1400, 1500, 1600, 1800, 2000, 2200, 2400};

	// for 1 carrier mode (200 Hz) use index 5 (1500 Hz)
	// for 2 carrier (500 Hz) modes use indexes 4, 6(1400 and 1600 Hz)
	// for 10 carrier modes use index 0-4, 6-10  (600, 800, 1000, 1200, 1400, 1600, 1800, 2000, 2200, 2400 Hz) 
 
	float dblCarPhaseInc[11];	// the phase inc per sample based on frequency
	float dblAngle;				// Angle in radians
	int i, j, k;


	char msg[256];
	int len;
	int line = 0;
	FILE * fp1;

	
	// Compute the phase inc per sample
	
	for (i = 0; i < 11; i++)
	{
		dblCarPhaseInc[i] = 2 * M_PI * dblCarFreq[i] / 12000.0f;
	}
	
	//Now compute the templates: ( 32 bit values total)  
	
	for (i = 0; i < 11; i++) // across 11 tones
	{
		for (j = 0; j < 4; j++) //  using only half the phase values (0, 45, 90, 135)  (use sign compliment for the opposit phase) 
		{
			dblAngle = j * M_PI / 4;
			
			for (k = 0; k < 120; k++) // for 120 samples (one 100 baud symbol also used to generate 50 baud using mod 120) 
			{
				intQAM50bdCarTemplate[i][j][k] = intAmp * sinf(dblAngle);  // with no envelope control
				
				dblAngle += dblCarPhaseInc[i];
				if (dblAngle >= 2 * M_PI)
					dblAngle -= 2 * M_PI;
			}
		}
	}

	
	fp1 = fopen("d:\\PSKcoeffs.txt", "wb");

	len = sprintf(msg, "\tCONST short intQAM50bdCarTemplate[11][4][120]; = \r\n");
	fwrite(msg, 1, len, fp1);

	len = sprintf(msg, "\t{{{\r\n");
	fwrite(msg, 1, len, fp1);

	for (i = 0; i <= 10; i++)		// across 11 tones
	{
		for (j = 0; j <= 3; j++)
		{
			line = 0;
		
			len = sprintf(msg, "\r\n// Carrier %d Phase %d\r\n", i, j);

			fwrite(msg, 1, len, fp1);
		

			for (k = 0; k <= 119; k++) // for 120 samples (one 100 baud symbol, 200 baud modes will just use half of the data)
			{
				if ((k - line) == 9)
				{
					// print 10 to line

					len = sprintf(msg, "\t%d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
					intQAM50bdCarTemplate[i][j][line],
					intQAM50bdCarTemplate[i][j][line + 1],
					intQAM50bdCarTemplate[i][j][line + 2],
					intQAM50bdCarTemplate[i][j][line + 3],
					intQAM50bdCarTemplate[i][j][line + 4],
					intQAM50bdCarTemplate[i][j][line + 5],
					intQAM50bdCarTemplate[i][j][line + 6],
					intQAM50bdCarTemplate[i][j][line + 7],
					intQAM50bdCarTemplate[i][j][line + 8],
					intQAM50bdCarTemplate[i][j][line + 9]);

					line = k + 1;

					if (k == 119)
					{
						len += sprintf(&msg[len-2], "},\r\n\t{");
						len -=2;
					}
					fwrite(msg, 1, len, fp1);
				}
			}
//			len = sprintf(msg, "\t}{\r\n");
//			fwrite(msg, 1, len, fp1);
		}
		len = sprintf(msg, "\t}}{{\r\n");
		fwrite(msg, 1, len, fp1);
	}

	len = sprintf(msg, "\t}}};\r\n");
	fwrite(msg, 1, len, fp1);

	fclose(fp1);

}  
#endif

void GenerateOFDMTemplates()
{

	// Generate templates of 216 samples (each template = 20 ms) for each of the 43 possible carriers used in OFDM modulation
	// Amplitude values will have to be scaled based on the number of Active Carriers initial values should be OK for 1 carrier
	// Tone values 

	// Centre Freq 1500 carrier spacing 55.555 (10000 /180);

	// the carrier frequencies in Hz

	float dblCarFreq[MAXCAR] = {
		1055.555542f, 1111.111084f, 1166.666748f, 1222.222290f,
		1277.777832f, 1333.333374f, 1388.888916f, 1444.444458f,
		1500.000000f, 1555.555664f, 1611.111206f, 1666.666748f,
		1722.222290f,  1777.777832f, 1833.333374f, 1888.888916f, 1944.444458f};

	float dblCarPhaseInc[MAXCAR];	// the phase inc per sample based on frequency
	float dblAngle;				// Angle in radians

	float spacing = 10000.0f / 180;

	int i, j, k;


	char msg[256];
	int len;
	int line = 0;
	FILE * fp1;

	for (i = 0; i < MAXCAR; i++)
	{
		dblCarFreq[i] = 1500.0f + (spacing * (i - 21));
	}



	// Compute the phase inc per sample

	for (i = 0; i < MAXCAR; i++)
	{
		dblCarPhaseInc[i] = 2 * M_PI * dblCarFreq[i] / 12000.0f;
	}

	//Now compute the templates: ( 32 bit values total)  

	for (i = 0; i < MAXCAR; i++) // across all tones
	{
		for (j = 0; j < 8; j++) //  using only half the phase values (0, 45, 90, 135)  (use sign compliment for the opposit phase) 
		{
			dblAngle = j * M_PI / 8;

			for (k = 0; k < 216; k++) // for 120 samples (one 100 baud symbol also used to generate 50 baud using mod 120) 
			{
				int x =  intAmp * sinf(dblAngle);  // with no envelope control

				if (intOFDMTemplate[i][j][k] != x)
					x = x; 
				
//				intOFDMTemplate[i][j][k] = x;

				dblAngle += dblCarPhaseInc[i];
				if (dblAngle >= 2 * M_PI)
					dblAngle -= 2 * M_PI;
			}
		}
	}

	
	fp1 = fopen("d:\\OFDMcoeffs.txt", "wb");

	len = sprintf(msg, "\tCONST short intOFDMTemplate[MAXCAR][8][216] = \r\n");
	fwrite(msg, 1, len, fp1);

	len = sprintf(msg, "\t{{{\r\n");
	fwrite(msg, 1, len, fp1);

	for (i = 0; i < MAXCAR; i++)		// across 43 tones
	{
		for (j = 0; j <= 7; j++)
		{
			line = 0;

			len = sprintf(msg, "\r\n// Carrier %d Phase %d\r\n", i, j);

			fwrite(msg, 1, len, fp1);


			for (k = 0; k <= 216; k++) // for 216 samples (one 55 baud symbol)
			{
				if ((k - line) == 7)
				{
					// print 8 to line

					len = sprintf(msg, "\t%d, %d, %d, %d, %d, %d, %d, %d,\n",
						intOFDMTemplate[i][j][line],
						intOFDMTemplate[i][j][line + 1],
						intOFDMTemplate[i][j][line + 2],
						intOFDMTemplate[i][j][line + 3],
						intOFDMTemplate[i][j][line + 4],
						intOFDMTemplate[i][j][line + 5],
						intOFDMTemplate[i][j][line + 6],
						intOFDMTemplate[i][j][line + 7]);

					line = k + 1;

					if (k == 215)
					{
						len += sprintf(&msg[len - 2], "},\r\n\t{");
						len -= 2;
					}
					fwrite(msg, 1, len, fp1);
				}
			}
			//			len = sprintf(msg, "\t}{\r\n");
			//			fwrite(msg, 1, len, fp1);
		}
		len = sprintf(msg, "\t},{{\r\n");
		fwrite(msg, 1, len, fp1);
	}

	len = sprintf(msg, "\t}}};\r\n");
	fwrite(msg, 1, len, fp1);

	fclose(fp1);

}





