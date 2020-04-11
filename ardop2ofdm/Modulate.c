//	Sample Creation routines (encode and filter) for ARDOP Modem

#include "ARDOPC.h"

extern int intSessionBW;	// Negotiated speed

#pragma warning(disable : 4244)		// Code does lots of  float to int

FILE * fp1;

float dblQAMCarRatio = 1.0f / 1.765f;   //Optimum for 8,8 circular constellation with no phase offset: (Dmin inner to inner = Dmin inner to outer) 

#define MAX(x, y) ((x) > (y) ? (x) : (y))

// Function to generate the Two-tone leader and Frame Sync (used in all frame types) 

extern short Dummy;

int intSoftClipCnt = 0;
BOOL SendingHeader200 = 0;		// Set when sending header in 200 Hz Modes

void Flush();

// Function to soft clip combined waveforms. 
int SoftClip(int intInput)
{
	if (intInput > 30000) // soft clip above/below 30000
	{
		intInput = 30000; //min(32700, 30000 + 20 * sqrt(intInput - 30000));
		intSoftClipCnt += 1;
	}
	else if(intInput < -30000)
	{
		intInput = -30000; //max(-32700, -30000 - 20 * sqrt(-(intInput + 30000)));
		intSoftClipCnt += 1;
	}

	if (intInput == 0)
		intInput = 0;


	return intInput;
}

void GetTwoToneLeaderWithSync(int intSymLen)
{
	// Generate a 50 baud (20 ms symbol time) 2 tone leader 
    // leader tones used are 1475 and 1525 Hz.  
  
	int intSign = 1;
	int i, j;
	short intSample;

    if ((intSymLen & 1) == 1) 
		intSign = -1;

	for (i = 0; i < intSymLen; i++)   //for the number of symbols needed (two symbols less than total leader length) 
	{
		for (j = 0; j < 240; j++)	// for 240 samples per symbol (50 baud) 
		{
           if (i != (intSymLen - 1)) 
			   intSample = intSign * int50BaudTwoToneLeaderTemplate[j];
		   else
			   intSample = -intSign * int50BaudTwoToneLeaderTemplate[j];
   
		   SampleSink(intSample);
		}
		intSign = -intSign;
	}
}

void SendLeaderAndSYNC(UCHAR * bytEncodedBytes, int intLeaderLen)
{
	int intMask = 0;
	int intLeaderLenMS;
	int j, k, n;
	UCHAR bytMask;
	UCHAR bytSymToSend;
	short intSample;
	if (intLeaderLen == 0)
		intLeaderLenMS = LeaderLength;
	else
		intLeaderLenMS = intLeaderLen;

 	// Create the leader

	GetTwoToneLeaderWithSync(intLeaderLenMS / 20);
		       
	//Create the 8 symbols (16 bit) 50 baud 4FSK frame type with Implied SessionID

	for(j = 0; j < 2; j++)		 // for the 2 bytes of the frame type
	{              
		bytMask = 0x30;
		
		for(k = 0; k < 4; k++)	 // for 4 symbols per byte (3 data + 1 parity)
		{
			if (k < 3)
				bytSymToSend = (bytMask & bytEncodedBytes[j]) >> (2 * (2 - k));
			else
				bytSymToSend = ComputeTypeParity(bytEncodedBytes[j]);

			for(n = 0; n < 240; n++)
			{
//				if ( ARQBandwidth == XB2500)		// 2500 Hz
//				{
//					if (bytSymToSend < 2)
//						intSample = (0.62 * intFSK50bdCarTemplate[4 + bytSymToSend][n]) + (0.62 * intFSK50bdCarTemplate[bytSymToSend][n]); // 4 is offset to center tones 1350, 1450, 1550, 1650Hz
//					else
//						intSample = (0.62 * intFSK50bdCarTemplate[4 + bytSymToSend][n]) + (0.62 * intFSK50bdCarTemplate[8 + bytSymToSend][n]); // 4 is offset to center tones 1350, 1450, 1550, 1650Hz, 8 is offset to tones 1800 and 1900
//				}
//				else
					intSample = intFSK50bdCarTemplate[bytSymToSend + 4][n];
				
				SampleSink(intSample);	
			}
			bytMask = bytMask >> 2;
		}
	}
}

void Mod4FSKDataAndPlay(unsigned char * bytEncodedBytes, int Len, int intLeaderLen)
{
	// Function to Modulate data encoded for 4FSK, create
	// the 16 bit samples and send to sound interface

	// Function works for 1, 2 or 4 simultaneous carriers 

	int intNumCar, intBaud, intDataLen, intRSLen, intDataPtr, intSampPerSym, intDataBytesPerCar;
	BOOL blnOdd;

	int intSample;

    char strType[18] = "";
    char strMod[16] = "";

	UCHAR bytSymToSend, bytMask, bytMinQualThresh;

	float dblCarScalingFactor;
	int intMask = 0;
	int intLeaderLenMS;
	int k, m, n;
	UCHAR Type = bytEncodedBytes[0];

	if (!FrameInfo(Type, &blnOdd, &intNumCar, strMod, &intBaud, &intDataLen, &intRSLen, &bytMinQualThresh, strType))
		return;

	if (strcmp(strMod, "4FSK") != 0)
		return;

	WriteDebugLog(LOGDEBUG, "Sending Frame Type %s", strType);
	DrawTXFrame(strType);

	if (bytEncodedBytes[0] == PktFrameHeader)
	{
		// Leader is 4FSK which needs 500 filter

		if (pktBW[pktMode] < 1000)
			initFilter(500,1500);
		else
			initFilter(2500,1500);
	}
	else
	{
		 if (ARQBandwidth == XB200)
			initFilter(200,1500);
		 else if (intNumCar == 1)
			initFilter(500,1500);
		 else
			initFilter(2500,1500);
	}

	if (intLeaderLen == 0)
		intLeaderLenMS = LeaderLength;
	else
		intLeaderLenMS = intLeaderLen;

    switch(intBaud)
	{		
	case 50:
		
		intSampPerSym = 240;
		break;
                
	case 100:
		
		intSampPerSym = 120;
	}
		
	intDataBytesPerCar = (Len - 2) / intNumCar;		// We queue the samples here, so dont copy below
    
	SendLeaderAndSYNC(bytEncodedBytes, intLeaderLen);

	intDataPtr = 2;

Reenter:

	switch(intNumCar)
	{
	case 1:			 // use carriers 0-3
		
		dblCarScalingFactor = 1.0; //  (scaling factors determined emperically to minimize crest factor) 

		for (m = 0; m < intDataBytesPerCar; m++)  // For each byte of input data
		{
			bytMask = 0xC0;		 // Initialize mask each new data byte
			
			for (k = 0; k < 4; k++)		// for 4 symbol values per byte of data
			{
				bytSymToSend = (bytMask & bytEncodedBytes[intDataPtr]) >> (2 * (3 - k)); // Values 0-3

				for (n = 0; n < intSampPerSym; n++)	 // Sum for all the samples of a symbols 
				{
					if (intBaud == 50)
					{
						if (intSessionBW == 200 && (bytSymToSend == 0 || bytSymToSend == 3)) 
							// This scales down the two outer tones in 200 Hz mode to restrict bandwidth slightly
							intSample = 0.7f * intFSK50bdCarTemplate[4 + bytSymToSend][n]; //4 is offset to center tones 1350, 1450, 1550, 1650Hz
						else        
							intSample = intFSK50bdCarTemplate[4 + bytSymToSend][n]; //4 is offset to center tones 1350, 1450, 1550, 1650Hz
					}
					else if (intBaud == 100)		// Used for OFDMACK
					{
						intSample = intFSK100bdCarTemplate[bytSymToSend][n];
					}
					SampleSink(intSample);
				}

				bytMask = bytMask >> 2;
			}
			intDataPtr += 1;
		}
		
		if (Type == PktFrameHeader)
		{
		
			// just sent packet header. Send rest in current mode
			// Assumes we are using 4FSK for Packet Header

			Type = 0;			// Prevent reentry
	
			strcpy(strMod, &pktMod[pktMode][0]);
			intDataBytesPerCar = pktDataLen + pktRSLen + 3;
			intDataPtr = 11;		// Over Header
			intNumCar = pktCarriers[pktMode];

			// This assumes Packet Data is sent as PSK/QAM

			switch(intNumCar)
			{		
			case 1:
		//		intCarStartIndex = 4;
				dblCarScalingFactor = 1.0f; // Starting at 1500 Hz  (scaling factors determined emperically to minimize crest factor)  TODO:  needs verification
				break;
			case 2:
		//		intCarStartIndex = 3;
				dblCarScalingFactor = 0.53f; // Starting at 1400 Hz
				break;
			case 4:
		//		intCarStartIndex = 2;
				dblCarScalingFactor = 0.29f; // Starting at 1200 Hz
				break;
			case 8:
		//		intCarStartIndex = 0;
				dblCarScalingFactor = 0.17f; // Starting at 800 Hz
			}
			
			// Reenter to send rest of variable length packet frame
		
			if (pktFSK[pktMode])
				goto Reenter;
			else
				ModPSKDataAndPlay(bytEncodedBytes, 0, 0);
			return;
		}

		Flush();

		break;

	case 2:			// use carriers 0-3 and 8-11 (50 baud only)

		dblCarScalingFactor = 0.6f; //  (scaling factors determined emperically to minimize crest factor)

		for (m = 0; m < intDataBytesPerCar; m++)	  // For each byte of input data 
		{
			bytMask = 0xC0;	// Initialize mask each new data byte
                        			
			for (k = 0; k < 4; k++)		// for 4 symbol values per byte of data
			{
				for (n = 0; n < intSampPerSym; n++)	 // for all the samples of a symbol for 2 carriers
				{
					//' First carrier
                      
					bytSymToSend = (bytMask & bytEncodedBytes[intDataPtr]) >> (2 * (3 - k)); // Values 0-3
					intSample = intFSK50bdCarTemplate[bytSymToSend][n];
					// Second carrier
                    
					bytSymToSend = (bytMask & bytEncodedBytes[intDataPtr + intDataBytesPerCar]) >> (2 * (3 - k));	// Values 0-3
					intSample = SoftClip(dblCarScalingFactor * (intSample + intFSK50bdCarTemplate[8 + bytSymToSend][n]));
	
					SampleSink(intSample);
				}
				bytMask = bytMask >> 2;
			}
			intDataPtr += 1;
		}
             
		Flush();

		break;
}
}

// Function to extract an 8PSK symbol from an encoded data array


UCHAR GetSym8PSK(int intDataPtr, int k, int intCar, UCHAR * bytEncodedBytes, int intDataBytesPerCar)
{
	int int3Bytes = bytEncodedBytes[intDataPtr + intCar * intDataBytesPerCar];
//	int intMask  = 7;
	int intSym;
	UCHAR bytSym;

	int3Bytes = int3Bytes << 8;
	int3Bytes += bytEncodedBytes[intDataPtr + intCar * intDataBytesPerCar + 1];
	int3Bytes = int3Bytes << 8;
	int3Bytes += bytEncodedBytes[intDataPtr + intCar * intDataBytesPerCar + 2];  // now have 3 bytes, 24 bits or 8 8PSK symbols 
//	intMask = intMask << (3 * (7 - k));
	intSym = int3Bytes >> (3 * (7 - k));
	bytSym = intSym & 7;	//(intMask && int3Bytes) >> (3 * (7 - k));

	return bytSym;
}



// Function to Modulate data encoded for PSK and 16QAM, create
// the 16 bit samples and send to sound interface
   

void ModPSKDataAndPlay(unsigned char * bytEncodedBytes, int Len, int intLeaderLen)
{
	int intNumCar, intBaud, intDataLen, intRSLen, intDataPtr, intSampPerSym, intDataBytesPerCar;
	BOOL blnOdd;
	int Type = bytEncodedBytes[0];

	int intSample;
    char strType[18] = "";
    char strMod[16] = "";
	UCHAR bytSym, bytSymToSend, bytMinQualThresh;
	float dblCarScalingFactor;
	int intMask = 0;
	int intLeaderLenMS;
	int i, j, k, l = 4, n;
	int intCarStartIndex;
	int intPeakAmp;
	int intCarIndex;
	BOOL QAM = 0;

	UCHAR bytLastSym[43]; // = {0}; // Holds the last symbol sent (per carrier). bytLastSym(4) is 1500 Hz carrier (only used on 1 carrier modes) 
 
	if (!FrameInfo(Type, &blnOdd, &intNumCar, strMod, &intBaud, &intDataLen, &intRSLen, &bytMinQualThresh, strType))
		return;

	intDataBytesPerCar = (Len - 2) / intNumCar;		// We queue the samples here, so dont copy below

	// These new scaling factor combined with soft clipping to provide near optimum scaling Jan 6, 2018 
	// The Test form was changed to calculate the Peak power to RMS power (PAPR) of the test waveform and count the number of "soft clips" out of ~ 50,000 samples. 
	// These values arrived at emperically using the Test form (Quick brown fox message) to minimize PAPR at a minor decrease in maximum constellation quality

	if (strstr(strMod, "16QAM"))
	{
		// QAM Modes

		QAM = 1;
		l = 2;				// 2 symbols per byte
	
		switch(intNumCar)
		{
   		case 1:
			intCarStartIndex = 5;
			dblCarScalingFactor = 2.0f; // Starting at 1500 Hz  Selected to give < 13% clipped values yielding a PAPR = 1.6 Constellation Quality >98
			break;
		case 2:
			intCarStartIndex = 4;
			dblCarScalingFactor = 1.0f;
			break;
		case 10:
			intCarStartIndex = 0;
			dblCarScalingFactor = 0.4f;
		} 
	}
	else  // 4PSK
	{
		switch(intNumCar)
		{
		case 1:
			intCarStartIndex = 5;
			dblCarScalingFactor = 2.0f; // Starting at 1500 Hz  Selected to give < 13% clipped values yielding a PAPR = 1.6 Constellation Quality >98
			break;
		case 2:
			intCarStartIndex = 4;
			dblCarScalingFactor = 1.2f;
			break;
		case 10:
			intCarStartIndex = 0;
			dblCarScalingFactor = 0.35f;
		} 
	}

	if (intBaud == 50)
		intSampPerSym = 240;
	else
		intSampPerSym = 120;

	if (Type == PktFrameData)
	{
		intDataBytesPerCar = pktDataLen + pktRSLen + 3;
		intDataPtr = 11;		// Over Header
		goto PktLoopBack;
	}
	
	WriteDebugLog(LOGDEBUG, "Sending Frame Type %s", strType);
	DrawTXFrame(strType);

	if (intNumCar == 1)
		initFilter(200,1500);
	else if (intNumCar == 2 || intNumCar == 9)
		initFilter(500,1500);
	else
		initFilter(2500,1500);

	if (intLeaderLen == 0)
		intLeaderLenMS = LeaderLength;
	else
		intLeaderLenMS = intLeaderLen;

	intSoftClipCnt = 0;
	
	// Create the leader

	SendLeaderAndSYNC(bytEncodedBytes, intLeaderLen);
	SendingHeader200 = FALSE;

	intPeakAmp = 0;

	intDataPtr = 2;  // initialize pointer to start of data.

PktLoopBack:		// Reenter here to send rest of variable length packet frame

         
	// Now create a reference symbol for each carrier
      
	//	We have to do each carrier for each sample, as we write
	//	the sample immediately 

		for (n = 0; n < intSampPerSym; n++)  // Sum for all the samples of a symbols 
		{
			intSample = 0;
			intCarIndex = intCarStartIndex;  // initialize to correct starting carrier

				for (i = 0; i < intNumCar; i++)	// across all carriers
			{
				bytSymToSend = 0;  //  using non 0 causes error on first data byte 12/8/2014   ...Values 0-3  not important (carries no data).   (Possible chance for Crest Factor reduction?)       
				bytLastSym[intCarIndex] = 0;

			
				intSample += intQAM50bdCarTemplate[intCarIndex][0][n % 120];

				intCarIndex ++;
				if (intCarIndex == 5)
					intCarIndex = 6;	// skip over 1500 Hz for multi carrier modes (multi carrier modes all use even hundred Hz tones)
			}
			intSample = SoftClip(intSample * 0.5 * dblCarScalingFactor);
			SampleSink(intSample);
		}
	
	// End of reference phase generation 

	// Unlike ARDOP_WIN we send samples as they are created,
	// so we loop through carriers, then data bytes

	for (j = 0; j < intDataBytesPerCar; j++)	//  for each referance and data symbol 
	{ 		
		// Loop through each symbol of byte (4 for PSK 2 for QAM
                        
		for (k = 0; k < l; k++)
		{
				for (n = 0; n < intSampPerSym; n++)
				{
					intSample = 0;
					intCarIndex = intCarStartIndex; // initialize the carrrier index
	
					for (i = 0; i < intNumCar; i++) // across all active carriers
					{					
						if (QAM == 0)
						{
							bytSym = (bytEncodedBytes[intDataPtr + i * intDataBytesPerCar] >> (2 * (3 - k))) & 3;
							bytSymToSend = ((bytLastSym[intCarIndex] + bytSym) & 3);  // Values 0-3
					
							if (bytSymToSend < 2) // This uses the symmetry of the symbols to reduce the table size by a factor of 2
								intSample += intQAM50bdCarTemplate[intCarIndex][2 * bytSymToSend][n % 120]; //  double the symbol value during template lookup for 4PSK. (skips over odd PSK 8 symbols)
							else if (bytSymToSend < 4)
							intSample -= intQAM50bdCarTemplate[intCarIndex][2 * (bytSymToSend - 2)][n % 120]; // subtract 2 from the symbol value before doubling and subtract value of table 

						}      
						else
						{
							// For 16QAM the angle is sent differential but the amplitude is sent as is for the symbol...verified 4/20 2018

							bytSym = (bytEncodedBytes[intDataPtr + i * intDataBytesPerCar] >> (4 * (1 - k))) & 15;
							bytSymToSend = ((bytLastSym[intCarIndex] & 7) + (bytSym & 7) & 7); // Compute the differential phase to send
							bytSymToSend = bytSymToSend | (bytSym & 8); // add in the amplitude bit directly from symbol 

							// 4bits/symbol (use table symbol values 0, 1, 2, 3, -0, -1, -2, -3) and modulate amplitude with MSB

							if (bytSymToSend < 4)// This uses the symmetry of the symbols to reduce the table size by a factor of 2
								intSample += intQAM50bdCarTemplate[intCarIndex][bytSymToSend][n % 120]; // double the symbol value during template lookup for 4PSK. (skips over odd PSK 8 symbols)
							else if (bytSymToSend < 8)
								intSample -= intQAM50bdCarTemplate[intCarIndex][(bytSymToSend - 4)][n % 120]; // subtract 4 from the symbol value before doubling and subtract value of table 
							else if (bytSymToSend < 12)
								intSample += dblQAMCarRatio * intQAM50bdCarTemplate[intCarIndex][bytSymToSend - 8][n % 120]; // subtract 4 from the symbol value before doubling and subtract value of table         
							else
								intSample -= dblQAMCarRatio * intQAM50bdCarTemplate[intCarIndex][bytSymToSend - 12][n % 120]; //  subtract 4 from the symbol value before doubling and subtract value of table 
						}

						if (n == intSampPerSym - 1)		// Last sample?
							bytLastSym[intCarIndex] = bytSymToSend;	

						intCarIndex += 1;
						if (intCarIndex == 5)
							intCarIndex = 6;  // skip over 1500 Hz carrier for multicarrier modes
					}
		
					// Done all carriers - send sample

					intSample = SoftClip(intSample * 0.5 * dblCarScalingFactor);
 					SampleSink(intSample);		
				}

				// Done all samples for this symbol
				// now next symbol of byte
			
		}
		intDataPtr++;
	}
				
   	if (Type == PktFrameHeader)
	{
		// just sent packet header. Send rest in current mode

		Type = 0;			// Prevent reentry

		strcpy(strMod, &pktMod[pktMode][0]);
		intDataBytesPerCar = pktDataLen + pktRSLen + 3;
		intDataPtr = 11;		// Over Header
		intNumCar = pktCarriers[pktMode];

		switch(intNumCar)
		{		
		case 1:
			intCarStartIndex = 4;
//			dblCarScalingFactor = 1.0f; // Starting at 1500 Hz  (scaling factors determined emperically to minimize crest factor)  TODO:  needs verification
			dblCarScalingFactor = 1.2f; // Starting at 1500 Hz  Selected to give < 13% clipped values yielding a PAPR = 1.6 Constellation Quality >98
		case 2:
			intCarStartIndex = 3;
//			dblCarScalingFactor = 0.53f;
			if (strcmp(strMod, "16QAM") == 0)
				dblCarScalingFactor = 0.67f; // Carriers at 1400 and 1600 Selected to give < 2.5% clipped values yielding a PAPR = 2.17, Constellation Quality >92
			else
				dblCarScalingFactor = 0.65f; // Carriers at 1400 and 1600 Selected to give < 4% clipped values yielding a PAPR = 2.0, Constellation Quality >95
			break;
		case 4:
			intCarStartIndex = 2;
//			dblCarScalingFactor = 0.29f; // Starting at 1200 Hz
			dblCarScalingFactor = 0.4f;  // Starting at 1200 Hz  Selected to give < 3% clipped values yielding a PAPR = 2.26, Constellation Quality >95
			break;
		case 8:
			intCarStartIndex = 0;
//			dblCarScalingFactor = 0.17f; // Starting at 800 Hz
			if (strcmp(strMod, "16QAM") == 0)
				dblCarScalingFactor = 0.27f; // Starting at 800 Hz  Selected to give < 1% clipped values yielding a PAPR = 2.64, Constellation Quality >94
			else
				dblCarScalingFactor = 0.25f; // Starting at 800 Hz  Selected to give < 2% clipped values yielding a PAPR = 2.5, Constellation Quality >95
		} 
		goto PktLoopBack;		// Reenter to send rest of variable length packet frame
	}
	Flush();
	if (intSoftClipCnt > 0)
		WriteDebugLog(LOGDEBUG, "Soft Clips %d ", intSoftClipCnt);


}


// Subroutine to add trailer before filtering

void AddTrailer()
{
	int intAddedSymbols = 1 + TrailerLength / 10; // add 1 symbol + 1 per each 10 ms of MCB.Trailer
	int i, k;

	for (i = 1; i <= intAddedSymbols; i++)
	{
		for (k = 0; k < 120; k++)
		{
			SampleSink(intQAM50bdCarTemplate[5][0][k % 60]);
		}
	}
}

//	Resends the last frame

void RemodulateLastFrame()
{	
	int intNumCar, intBaud, intDataLen, intRSLen;
	UCHAR bytMinQualThresh;
	BOOL blnOdd;

	char strType[18] = "";
    char strMod[16] = "";

	if (!FrameInfo(bytEncodedBytes[0], &blnOdd, &intNumCar, strMod, &intBaud, &intDataLen, &intRSLen, &bytMinQualThresh, strType))
		return;

	if (strcmp(strMod, "4FSK") == 0)
	{
		Mod4FSKDataAndPlay(bytEncodedBytes, EncLen, intCalcLeader);  // Modulate Data frame 
		return;
	}

	if (strcmp(strMod, "OFDM") == 0)
	{
		int save = OFDMMode;
		OFDMMode = LastSentOFDMMode;

		ModOFDMDataAndPlay(bytEncodedBytes, EncLen, intCalcLeader);  // Modulate Data frame 

		OFDMMode = save;

		return;
	}

	ModPSKDataAndPlay(bytEncodedBytes, EncLen, intCalcLeader);  // Modulate Data frame 
}

// Filter State Variables

static float dblR = (float)0.9995f;	// insures stability (must be < 1.0) (Value .9995 7/8/2013 gives good results)
static int intN = 120;				//Length of filter 12000/100
static float dblRn;

static float dblR2;
static float dblCoef[34] = {0.0f};			// the coefficients
float dblZin = 0, dblZin_1 = 0, dblZin_2 = 0, dblZComb= 0;  // Used in the comb generator

// The resonators 
      
float dblZout_0[34] = {0.0f};	// resonator outputs
float dblZout_1[34] = {0.0f};	// resonator outputs delayed one sample
float dblZout_2[34] = {0.0f};	// resonator outputs delayed two samples

int fWidth;				// Filter BandWidth
int SampleNo;
int outCount = 0;
int first, last;		// Filter slots
int centreSlot;

float largest = 0;
float smallest = 0;

short Last120[256];		// Now need 240 for 200 Hz filter

int Last120Get = 0;
int Last120Put = 120;

int Number = 0;				// Number waiting to be sent


extern unsigned short buffer[2][1200];

unsigned short * DMABuffer;

unsigned short * SendtoCard(unsigned short * buf, int n);
unsigned short * SoundInit();

// initFilter is called to set up each packet. It selects filter width

void initFilter(int Width, int Centre)
{
	int i, j;
	fWidth = Width;
	centreSlot = Centre / 100;
	largest = smallest = 0;
	SampleNo = 0;
	Number = 0;
	outCount = 0;
	memset(Last120, 0, 256);

	DMABuffer = SoundInit();

	KeyPTT(TRUE);
	SoundIsPlaying = TRUE;
	StopCapture();

	Last120Get = 0;
	Last120Put = 120;

	dblRn = powf(dblR, intN);
	dblR2 = powf(dblR, 2);

	dblZin_2 = dblZin_1 = 0;

	switch (fWidth)
	{
	case 200:

		// Used for PSK 200 Hz modulation XMIT filter  
		// implements 5 50 Hz wide sections centered on 1500 Hz  (~200 Hz wide @ - 30dB centered on 1500 Hz)
 
		SendingHeader200 = TRUE;
		intN = 240;
		Last120Put = 240;
		centreSlot = Centre / 50;
		first = centreSlot - 3;
		last = centreSlot + 3;		// 7 filter sections
		break;

	case 500:

		// implements 7 100 Hz wide sections centered on 1500 Hz  (~500 Hz wide @ - 30dB centered on 1500 Hz)

		intN = 120;
		first = centreSlot - 3;
		last = centreSlot + 3;		// 7 filter sections
		break;

	case 2500:
		
		// implements 26 100 Hz wide sections centered on 1500 Hz  (~2000 Hz wide @ - 30dB centered on 1500 Hz)

		intN = 120;
		first = centreSlot - 13;
		last = centreSlot + 13;		// 27 filter sections
		break;
	
	default:

		WriteDebugLog(LOGCRIT, "Invalid Filter Width %d", fWidth);
	}


	for (j = first; j <= last; j++)
	{
		dblZout_0[j] = 0;
		dblZout_1[j] = 0;
		dblZout_2[j] = 0;
	}

	// Initialise the coefficients

	if (dblCoef[last] == 0.0)
	{
		for (i = first; i <= last; i++)
		{
			dblCoef[i] = 2 * dblR * cosf(2 * M_PI * i / intN); // For Frequency = bin i
		}
	}
 }


void SampleSink(short Sample)
{
	//	Filter and send to sound interface

	// This version is passed samples one at a time, as we don't have
	//	enough RAM in embedded systems to hold a full audio frame

	int intFilLen = intN / 2;
	int j;
	float intFilteredSample = 0;			//  Filtered sample

	//	We save the previous intN samples
	//	The samples are held in a cyclic buffer

	if (SampleNo < intN)
		dblZin = Sample;
	else 
		dblZin = Sample - dblRn * Last120[Last120Get];

	if (++Last120Get == (intN + 1))
		Last120Get = 0;

	//Compute the Comb

	dblZComb = dblZin - dblZin_2 * dblR2;
	dblZin_2 = dblZin_1;
	dblZin_1 = dblZin;

	// Now the resonators
		
	for (j = first; j <= last; j++)
	{
		dblZout_0[j] = dblZComb + dblCoef[j] * dblZout_1[j] - dblR2 * dblZout_2[j];
		dblZout_2[j] = dblZout_1[j];
		dblZout_1[j] = dblZout_0[j];

		switch (fWidth)
		{
		case 200:

			// scale each by transition coeff and + (Even) or - (Odd) 

			if (SampleNo >= intFilLen)
			{
				if (j == first || j == last)
				{
					if (SendingHeader200)
						intFilteredSample -= dblZout_0[j];  // This provides no attenuation to the Frame Type tones at 1350 and 1650
					else
						intFilteredSample -= 0.1 * dblZout_0[j]; // This smaller value required to filter down to 200 Hz bandwidth
				}
				else if ((j & 1) == 0)
					intFilteredSample += (int)dblZout_0[j];
				else
					intFilteredSample -= (int)dblZout_0[j];
			}
  
			break;

		case 500:

			// scale each by transition coeff and + (Even) or - (Odd) 
			// Resonators 12 and 18 scaled to get best shape and side lobe supression to - 45 dB while keeping BW at 500 Hz @ -26 dB
			// practical range of scaling .05 to .25
			// Scaling also accomodates for the filter "gain" of approx 60. 

			if (SampleNo >= intFilLen)
			{
				if (j == first || j == last)
					intFilteredSample += 0.389f * dblZout_0[j];
				else if ((j & 1) == 0)
					intFilteredSample += (int)dblZout_0[j];
				else
					intFilteredSample -= (int)dblZout_0[j];
			}
        
			break;
		
		case 2500:

			// scale each by transition coeff and + (Even) or - (Odd) 
			// Resonators 2 and 28 scaled to get best shape and side lobe supression to - 45 dB while keeping BW at 500 Hz @ -26 dB
			// practical range of scaling .05 to .25
			// Scaling also accomodates for the filter "gain" of approx 60. 
          
			if (SampleNo >= intFilLen)
			{
				if (j == first || j == last)
					intFilteredSample +=  0.3891f * dblZout_0[j];
				else if ((j & 1) == 0)	// Even
					intFilteredSample += (int)dblZout_0[j];
				else
					intFilteredSample -= (int)dblZout_0[j];
			}
		}
	}

	if (SampleNo >= intFilLen)
	{
		intFilteredSample = intFilteredSample * 0.00833333333f; //  rescales for gain of filter
		largest = max(largest, intFilteredSample);	
		smallest = min(smallest, intFilteredSample);
		
		if (intFilteredSample > 32700)  // Hard clip above 32700
			intFilteredSample = 32700;
		else if (intFilteredSample < -32700)
			intFilteredSample = -32700;

#ifdef TEENSY	
		int work = (short)(intFilteredSample);
		DMABuffer[Number++] = (work + 32768) >> 4; // 12 bit left justify
#else
		DMABuffer[Number++] = (short)intFilteredSample;
#endif
		if (Number == SendSize)
		{
			// send this buffer to sound interface

			DMABuffer = SendtoCard(DMABuffer, SendSize);
			Number = 0;
		}
	}
		
	Last120[Last120Put++] = Sample;

	if (Last120Put == (intN + 1))
		Last120Put = 0;

	SampleNo++;
}

extern int dttTimeoutTrip;

extern UCHAR bytSessionID;


void Flush()
{
	SoundFlush(Number);
}



// Subroutine to make a CW ID Wave File

void sendCWID(char * strID, BOOL blnPlay)
{
	// This generates a phase synchronous FSK MORSE keying of strID
	// FSK used to maintain VOX on some sound cards
	// Sent at 90% of  max ampllitude

	char strAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/"; 

	//Look up table for strAlphabet...each bit represents one dot time, 3 adjacent dots = 1 dash
	// one dot spacing between dots or dashes

	int intCW[] = {0x17, 0x1D5, 0x75D, 0x75, 0x1, 0x15D, 
           0x1DD, 0x55, 0x5, 0x1777, 0x1D7, 0x175,
           0x77, 0x1D, 0x777, 0x5DD, 0x1DD7, 0x5D, 
           0x15, 0x7, 0x57, 0x157, 0x177, 0x757, 
           0x1D77, 0x775, 0x77777, 0x17777, 0x5777, 0x1577,
           0x557, 0x155, 0x755, 0x1DD5, 0x7775, 0x1DDDD, 0x1D57, 0x1D57};


	float dblHiPhaseInc = 2 * M_PI * 1609.375f / 12000; // 1609.375 Hz High tone
	float dblLoPhaseInc = 2 * M_PI * 1390.625f / 12000; // 1390.625  low tone
	float dblHiPhase = 0;
 	float dblLoPhase = 0;
 	int  intDotSampCnt = 768;  // about 12 WPM or so (should be a multiple of 256
	short intDot[768];
	short intSpace[768];
	int i, j, k;
	int intAmp = 26000;	   // Selected to have some margin in calculations with 16 bit values (< 32767) this must apply to all filters as well. 
	char * index;
	int intMask;
	int idoffset;

    strlop(strID, '-');		// Remove any SSID    

	// Generate the dot samples (high tone) and space samples (low tone) 

	for (i = 0; i < intDotSampCnt; i++)
	{
		if (CWOnOff)
			intSpace[i] = 0;
		else

			intSpace[i] = sin(dblLoPhase) * 0.9 * intAmp;

		intDot[i] = sin(dblHiPhase) * 0.9 * intAmp;
		dblHiPhase += dblHiPhaseInc;
		if (dblHiPhase > 2 * M_PI)
			dblHiPhase -= 2 * M_PI;
		dblLoPhase += dblLoPhaseInc;
		if (dblLoPhase > 2 * M_PI)
			dblLoPhase -= 2 * M_PI;
	}
	
	initFilter(500,1500);
   
	//Generate leader for VOX 6 dots long

	for (k = 6; k >0; k--)
		for (i = 0; i < intDotSampCnt; i++)
			SampleSink(intSpace[i]);

	for (j = 0; j < strlen(strID); j++)
	{
		index = strchr(strAlphabet, strID[j]);
		if (index)
			idoffset = index - &strAlphabet[0];
		else
			idoffset = 0;

		intMask = 0x40000000;

		if (index == NULL)
		{
			// process this as a space adding 6 dots worth of space to the wave file

			for (k = 6; k >0; k--)
				for (i = 0; i < intDotSampCnt; i++)
					SampleSink(intSpace[i]);
		}
		else
		{
		while (intMask > 0) //  search for the first non 0 bit
			if (intMask & intCW[idoffset])
				break;	// intMask is pointing to the first non 0 entry
			else
				intMask >>= 1;	//  Right shift mask
				
		while (intMask > 0) //  search for the first non 0 bit
		{
			if (intMask & intCW[idoffset])
				for (i = 0; i < intDotSampCnt; i++)
					SampleSink(intDot[i]);
			else
				for (i = 0; i < intDotSampCnt; i++)
					SampleSink(intSpace[i]);
	
			intMask >>= 1;	//  Right shift mask
		}
		}
			// add 3 dot spaces for inter letter spacing
			for (k = 6; k >0; k--)
				for (i = 0; i < intDotSampCnt; i++)
					SampleSink(intSpace[i]);
	}
	
	//add 3 spaces for the end tail
	
	for (k = 6; k >0; k--)
		for (i = 0; i < intDotSampCnt; i++)
			SampleSink(intSpace[i]);

	SoundFlush();
}


