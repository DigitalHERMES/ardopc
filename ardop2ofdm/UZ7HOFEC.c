//	Routines to support UZ7HO's PSK FEC Mode


//	4 * 100 baud carriers spaced at 175 HZ intervals. BPSK

#include "ARDOPC.h"

#pragma warning(disable : 4244)		// Code does lots of int float to int


// Get sample sync. ARDOP seems to do it by comparing a generated 1500 Hz 
// tone with received signal and finding sample position that gives best 
// correlation. This works as ARDOP has a 1500 Hz leader. Can we apply this here??





// Decode 1 carrier of PSK signal

// UZ7HO Version. Similar to ARDOP, but can run on unmixed

int ComputeAng1_Ang2(int intAng1, int intAng2);
void GoertzelRealImag(short intRealIn[], int intPtr, int N, float m, float * dblReal, float * dblImag);
int Track1CarPSK(int floatCarFreq, char * strPSKMod, float dblUnfilteredPhase, BOOL blnInit);
void MixNCOFilter(short * intNewSamples, int Length, float dblOffsetHz);


float CarFreq;
int UZ7HOIndex = -1;

extern short intPhases[8][8];	// We will decode as soon as we have 4 or 8 depending on mode
								//	(but need one set per carrier)

extern short ** intMags;
extern int intPhasesLen;
extern short intPSKPhase_1[8], intPSKPhase_0[8];
extern int Corrections;
extern	int intSampPerSym;
extern char strMod[16];
extern float dblOffsetHz;
extern short intFilteredMixedSamples[3600];	// Get Frame Type need 2400 and we may add 1200
extern int intFilteredMixedSamplesLength;


int Demod1CarPSKUZ7HO(short * Samples, int Start, int Carrier)
{
	// Converts intSample to an array of differential phase and magnitude values for the Specific Carrier Freq
	// intPtr should be pointing to the approximate start of the first reference/training symbol (1 of 3) 
	// intPhase() is an array of phase values (in milliradians range of 0 to 6283) for each symbol 
	// intMag() is an array of Magnitude values (not used in PSK decoding but for constellation plotting or QAM decoding)
	// Objective is to use Minimum Phase Error Tracking to maintain optimum pointer position

	// Decodes 8 successive symbols (960 bits)

	float dblReal, dblImag;
	int intMiliRadPerSample = CarFreq * M_PI / 6;
	int i;
	int intNumOfSymbols = 8;
	int origStart = Start;
	float dblFreqBin = CarFreq / 200.0f; //200 = 12000/60
	int	intCP = 28; // This value selected for best decoding percentage (56%) and best Averag 4PSK Quality (77) on mpg +5 dB
	int	intNforGoertzel = 60;
	int intDiff;
	int NewPhase;

	for (i = 0; i <  intNumOfSymbols; i++)
	{
		GoertzelRealImag(Samples, Start + intCP, intNforGoertzel, dblFreqBin, &dblReal, &dblImag);
		intMags[Carrier][intPhasesLen] = sqrtf(powf(dblReal, 2) + powf(dblImag, 2));
		NewPhase = 1000 * atan2f(dblImag, dblReal);

		intDiff = NewPhase - intPSKPhase_1[Carrier];

		if (intDiff < 0)
			intDiff += 6284;

		intPhases[Carrier][intPhasesLen] = intDiff;

		Corrections = Track1CarPSK(CarFreq, strMod, atan2f(dblImag, dblReal), FALSE);

//		if (Corrections != 0)
//		{
//			Start += Corrections;
//
//			GoertzelRealImag(Samples, Start + intCP, intNforGoertzel, dblFreqBin, &dblReal, &dblImag);
//			intPSKPhase_0[Carrier] = 1000 * atan2f(dblImag, dblReal);
//		}
		intPSKPhase_1[Carrier] = NewPhase;
		intPhasesLen++;
		Start += intSampPerSym;
	}
       // If AccumulateStats Then intPSKSymbolCnt += intPhase.Length

	return (Start - origStart);	// Symbols we've consumed
}




BOOL LookforUZ7HOLeader(short * Samples, int nSamples)
{
	//	4 100 baud carriers spaced at 175 HZint Used;. BPSK

	int Used;

	int OuterStart = 0, i, x, y;

	float carriers[4] = {
		1500 - (1.5 * 175),
		1500 - (0.5 * 175),
		1500 + (0.5 * 175),
		1500 + (1.5 * 175)};
		
//	dblPhaseInc = 2 * M_PI * 1000 / 4;

	intSampPerSym = 120;
	intPhasesLen = 0;

	for (i= 0; i < 4; i++)
	{
//		dblFreqBin[i] = CarFrew / 200.0f; //200 = 12000/60
//		CarFreq -= 175;
//		intPSKPhase_1[i] = 0; 
	}
	

	MixNCOFilter(Samples, nSamples, dblOffsetHz); // Mix and filter new samples (Mixing consumes all intRcvdSamples)
	OuterStart = 0;

	while (intFilteredMixedSamplesLength > (960 + 120))		// Experimental UZ7HO FSK Detect
	{
		if (UZ7HOIndex == -1)
		{
			// repeat decode with sample pointer incremented to get sample sync

			x = 0;
			y = 120;
		}
		else
		{
			x = UZ7HOIndex;
			y = x + 1;
		}
		for (i = x; i < y; i++)
		{
			char x1[10] = "";
			char x2[10] = "";
			char x3[10] = "";
			char x4[10] = "";
			int j;
			int Start = OuterStart;

			CarFreq = carriers[0];
		
			Used = Demod1CarPSKUZ7HO(intFilteredMixedSamples, Start + i, 0);
			intPhasesLen -= 8; //intPSKMode;
			CarFreq = carriers[1];
	
			Demod1CarPSKUZ7HO(intFilteredMixedSamples, Start + i, 1);
			intPhasesLen -= 8; //intPSKMode;
			CarFreq = carriers[2];

			Demod1CarPSKUZ7HO(intFilteredMixedSamples, Start + i, 2);
			intPhasesLen -= 8; //intPSKMode;
			CarFreq = carriers[3];

			Used = Demod1CarPSKUZ7HO(intFilteredMixedSamples, Start + i, 3);

			//	Decode Phases. 0 = 180 shift 1 = no shift
			for (j = 0; j < 8; j++)
			{
				if (intPhases[0][j] < 1572 || intPhases[0][j] > 1572 * 3)
					x1[j] = '1';
				else
					x1[j] = '0';
				if (intPhases[1][j] < 1572 || intPhases[1][j] > 1572 * 3)
					x2[j] = '1';
				else
					x2[j] = '0';
				if (intPhases[2][j] < 1572 || intPhases[2][j] > 1572 * 3)
					x3[j] = '1';
				else
					x3[j] = '0';
				if (intPhases[3][j] < 1572 || intPhases[3][j] > 1572 * 3)
					x4[j] = '1';
				else
					x4[j] = '0';
			}

	//		CorrectPhaseForTuningOffset(intPhases, intPhasesLen, strMod);

			printf("%d %d %d %d %d %d %d %d %d\n", 
				i, intMags[0][0], intMags[0][1], intMags[0][2], intMags[0][3], 
					intMags[0][4], intMags[0][5], intMags[0][6], intMags[0][7]);

			printf("%d %d %d %d %d %d %d %d %d\n", 
					i, intPhases[0][0], intPhases[0][1], intPhases[0][2], intPhases[0][3], 
					intPhases[0][4], intPhases[0][5], intPhases[0][6], intPhases[0][7]);

			printf ("%d %s %s %s %s \n", i, x1, x2, x3, x4);
//					i, intPhases[0][0], intPhases[0][1], intPhases[0][2], intPhases[0][3], 
//					intPhases[0][4], intPhases[0][5], intPhases[0][6], intPhases[0][7]);

			//	When we get the TXDelay preamble we should see all zeros
	
			if (UZ7HOIndex == -1)
			{
				if (strcmp(x1, "00000000") == 0 &&
					strcmp(x2, "00000000") == 0 &&
					strcmp(x3, "00000000") == 0 &&
					strcmp(x4, "00000000") == 0
					)
				{
					UZ7HOIndex = i + 1;
					if (UZ7HOIndex > 119)
						UZ7HOIndex -= 120;
				}
			}
			intPhasesLen = 0;
		}	
		intFilteredMixedSamplesLength -= 960;
		OuterStart += 960;
	}
	memmove(intFilteredMixedSamples,
		&intFilteredMixedSamples[OuterStart], intFilteredMixedSamplesLength * 2);

	return FALSE;
	
}	// end of UZ7HO


