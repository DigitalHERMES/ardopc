//
//	OFDM Module for ARDOP
//
/*

Thoughts on OFDM

We have lots (?43) carriers, so requiring all to be good is unlikely to succeed

If we ack each carrier separately we need some form of block number, so we can infill bits of data that were missed.

Always sending first block on first carrier seems a bad idea (but look at redundancy ideas below)

We could send the same data (with same block number) on multiple carriers for resilience

We need a ack frame with one bit per carrier (? 6 bytes) which is about 1 sec with 50 baud (do we need fec or just crc??).
Could send ACK at 100 baud, shortening it a bit, but is overall throughput increace worth it?

Using one byte block number but limit to 0 - 127. Window is 10160 in 16QAM
unless we use a different block length for each mode. Takes 10% of 2FSK throughput.

Receiver must be able to hold  window of frames (may be a problem with Teensy)

Must handle missed ack. ? base next ack on combination of repeats ?

Should we wait to pass to host till next seq frame received or all received ok?

Should we have multiple frame types for each mod mode, or add a frame type to carrier? 

Ideally would like 2, 4, 8 PSK, 16, 32QAM. Too many if we stick with ARDOP2 frame structure, so need frame type

Frame type has to be sent in fixed mode (not same as data) we need 2 or 3 bits. With 2FSK that is about 60 mS.
Need to validate. If we use same type for all carriers (any good reason not to?) we can decode type byte on all 
frames and compare. Very unlikely to choose wrong one, even if some are different. Could even try more than one (
may be a problem with Teensy). 

Could use combination of redundancy and mode to give very wide speed (and hopefully resilience) ratio, but gear 
shifting may be a nightmare. 

Is reducing carriers and therefore increasing power per carrier better than massive redundacy with lots of carriers?

Would dividing single carrier into multiple RS blocks be beneficial? Adds 3 byte overhead per block (Len and CRC)
if done with slow carriers would limit window size, so may need different block size per mode, which makes reassembly tricky

For a block of 4-5 secs we get

16OFDM 80 bytes/carrier, 3440 bytes per frame, approx 4600 BPS Net
 8OFDM 60 bytes/carrier, 2580 bytes per frame, approx 3440 BPS Net
 4OFDM 40 bytes/carrier, 1720 bytes per frame, approx 2300 BPS Net
 2OFDM 20 bytes/carrier,  860 bytes per frame, approx 1150 BPS Net

For Comparison 16QAM.2500.100

120 bytes/carrier, 1200 bytes per frame, approx 2225 BPS Net


*/



#ifdef WIN32
#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#include <windows.h>
#else
#define SOCKET int
#include <unistd.h>
#define closesocket close
#endif

#include "ARDOPC.h"

#pragma warning(disable : 4244)		// Code does lots of float to int

int OFDMMode;				// OFDM can use various modulation modes and redundancy levels
int RXOFDMMode = 0;

const char OFDMModes[8][6] = {"PSK2", "PSK4", "PSK8", "QAM16", "PSK16", "QAM32", "Undef", "Undef"};  

int OFDMCarriersReceived[8] = {0};
int OFDMCarriersDecoded[8] = {0};

int OFDMCarriersNaked[8] = {0};
int OFDMCarriersAcked[8] = {0};

// Functions to encode data for all OFDM frame types

// For the moment will will send all carriers with the same mode (I don't think there is an advantage being different),
// So we can use a different block length in each mode. We need to keep record of which blocks within bytDataToSend have
// been acked. Note that the first block is always unacked - acked data at the front of buffer is removed.

// Although we have an 8 bit sequence, I don't see the need for more than 128 outstanding blocks (carriers). If we miss
// just the first block of a 43 frame transmission, next time we send block 1 and 44 - 86. If 1 is still the only one 
// unacked I will repeat it several times in the next transmission, which will be the repeats plus 87 - 127.

// Unfortunately this means bytDataToSend must be at least 128 * max block size (80) = 10240, which is a lot on a Teensy.
// Maybe can come upwith better design!


UCHAR UnackedOFDMBlocks[128] = {0};			// This is bit list of outstanding blocks.
UCHAR UnackedOFDMBlockLen[128] = {0};		// Length of each block. do we need both (ie can we send a block of zero length ??)

int NextOFDMBlock = 0;
int UnAckedBlockPtr = 0;

int CarriersSent;
int BytesSent = 0;

int CarriersACKed;
int CarriersNAKed;

int lastOFDMRXMode;
int LastDemodType = 0;

int OFDMLevel; 		// % "compression" 


// This is used if we send a partial block. Will normally only happen
// at end of transmission, but could due to flow control
//
// Also used if we want to change mode

int DontSendNewData = 0;	// Dont send new till all acked
int LimitNewData = 0;		// Repeat unacked several times till all acked
int NeedShiftDown = 0;		// Shift down once all sent
int Duplicate = 0;			// Send data twice
int firstNewCarrier = 0;

UCHAR SentOFDMBlocks[MAXCAR];
UCHAR SentOFDMBlockLen[MAXCAR];	// Must match actual carrier number

UCHAR OFDMBlocks[MAXCAR];		// Build the carrier set in here
UCHAR OFDMBlockLen[MAXCAR];


UCHAR goodReceivedBlocks[128];
UCHAR goodReceivedBlockLen[128];


#define MAX_RAW_LENGTH_FSK 43	// 1 + 32 + 8 + 2
#define MAX_RAW_LENGTH	163     // Len Byte + Data + RS  + CRC I think!

int BytesSenttoHost = 0;


extern UCHAR bytData[128 * 80];
extern int bytQDataInProcessLen;

extern UCHAR bytSessionID;
extern UCHAR bytFrameData[10][MAX_RAW_LENGTH + 10];		// Received chars

extern char CarrierOk[MAXCAR];		// RS OK Flags per carrier

extern double dblPhaseInc;  // in milliradians
extern short intNforGoertzel[MAXCAR];
extern short intPSKPhase_1[MAXCAR], intPSKPhase_0[MAXCAR];
extern short intCP[MAXCAR];	  // Cyclic prefix offset 
extern float dblFreqBin[MAXCAR];
extern short intFilteredMixedSamples[];	// Get Frame Type need 2400 and we may add 1200
extern int intFilteredMixedSamplesLength;
extern int MaxFilteredMixedSamplesLength;
extern short intCarMagThreshold[MAXCAR];
extern short ** intMags;
extern short ** intPhases;
extern float floatCarFreq;			//(was int)	// Are these the same ??
extern int intNumCar;
extern int intSampPerSym;
extern int intDataLen;
extern int intRSLen;
extern int SymbolsLeft;
extern int intPhasesLen;
extern int intPhasesLen;
extern int intPSKMode;
extern int intSymbolsPerByte;
extern int PSKInitDone;
extern int intLastRcvdFrameQuality;
extern int frameLen;
extern int intFrameType;
extern const char Good[MAXCAR];
extern const char Bad[MAXCAR];
extern int pskStart;
extern int charIndex;			// Index into received chars
extern int RepeatedFrame;		// set if this data frame is a repeat
extern int	intShiftUpDn;
extern int dttTimeoutTrip;
extern int LastDataFrameType;			// Last data frame processed (for Memory ARQ, etc)
extern int intNAKctr;
extern int intACKctr;
extern int intTimeouts;

void GoertzelRealImag(short intRealIn[], int intPtr, int N, float m, float * dblReal, float * dblImag);
int ComputeAng1_Ang2(int intAng1, int intAng2);
int Demod1CarOFDMChar(int Start, int Carrier, int intNumOfSymbols);
VOID Decode1CarPSK(int Carrier, BOOL OFDM);
int CorrectRawDataWithRS(UCHAR * bytRawData, UCHAR * bytCorrectedData, int intDataLen, int intRSLen, int bytFrameType, int Carrier);
UCHAR GetSym8PSK(int intDataPtr, int k, int intCar, UCHAR * bytEncodedBytes, int intDataBytesPerCar);
int Track1CarPSK(int floatCarFreq, int PSKMode, BOOL QAM, BOOL OFDM, float dblUnfilteredPhase, BOOL blnInit);
void SendLeaderAndSYNC(UCHAR * bytEncodedBytes, int intLeaderLen);
void Flush();
BOOL  CheckCRC16(unsigned char * Data, int Length);
void CorrectPhaseForTuningOffset(short * intPhase, int intPhaseLength, int intPSKMode);

void GenCRC16Normal(char * Data, int Length)
{
	unsigned int CRC = GenCRC16(Data, Length);

	// Put the two CRC bytes after the stop index

	Data[Length++] = (CRC >> 8);	 // MS 8 bits of Register
	Data[Length] = (CRC & 0xFF);	 // LS 8 bits of Register
}


void ClearOFDMVariables()
{
	OFDMMode = PSK4;
	memset(UnackedOFDMBlocks, 0, sizeof(UnackedOFDMBlocks));
	memset(UnackedOFDMBlockLen, 0, sizeof(UnackedOFDMBlockLen));
	NextOFDMBlock = 0;
	BytesSent = 0;
	DontSendNewData = LimitNewData = Duplicate = 0;
	
	memset(SentOFDMBlocks, 0, sizeof(SentOFDMBlocks));
	memset(SentOFDMBlockLen, 0, sizeof(SentOFDMBlockLen));

	CarriersACKed = CarriersNAKed = NeedShiftDown = 0;
	lastOFDMRXMode = LastDemodType = 0;
}

void ProcessOFDMNak(int AckType)
{
	int AckedPercent;

	// We only get an OFDM NAK if no data is saved, so safe to shift down

	CarriersNAKed += CarriersSent;

	OFDMCarriersNaked[OFDMMode] += CarriersSent;

	AckedPercent = (CarriersACKed * 100) / (CarriersACKed + CarriersNAKed) ;

	WriteDebugLog(LOGDEBUG, "OFDM NAK Bytes Still outstanding %d OFDM Acked Percent %d", BytesSent, AckedPercent);

	// Shift down if too many naks or nothing acked

	if (((AckedPercent < 60) && (CarriersNAKed >= 2 * CarriersSent)) || OFDMCarriersAcked[OFDMMode] == 0)
	{
		if (OFDMMode > 0)
		{
			intNAKctr = 0;
			intACKctr = 0;

			OFDMMode--;
			if (OFDMMode == QAM16)		// skip 16QAM
				OFDMMode = PSK8;

			CarriersACKed = CarriersNAKed = 0;
			WriteDebugLog(LOGDEBUG, "OFDM Acked Percent %d. Shift down to Mode %d", AckedPercent, OFDMMode) ;
			dttTimeoutTrip = Now;	 // Retrigger the timeout on a shift and clear the NAK counter

			NextOFDMBlock = 0;
			BytesSent = 0;
			DontSendNewData = 0;
			memset(UnackedOFDMBlocks, 0, sizeof(UnackedOFDMBlocks));
			memset(UnackedOFDMBlockLen, 0, sizeof(UnackedOFDMBlockLen));
			
			SendData();				// Resend data in new mode
			return;
		}
		else
		{
			if (AckType == DataNAK)
				Gearshift_2(-1, False);
			else
				Gearshift_2(-2, False);
						
			if (intShiftUpDn != 0)
			{
				dttTimeoutTrip = Now;	 // Retrigger the timeout on a shift and clear the NAK counter
	
				// What should we do with OFDM Mode ??
		
				OFDMMode = PSK4;		// Start with 4PSK if going down
				NextOFDMBlock = 0;
				BytesSent = 0;
				DontSendNewData = LimitNewData = Duplicate = FALSE;

				memset(UnackedOFDMBlocks, 0, sizeof(UnackedOFDMBlocks));
				memset(UnackedOFDMBlockLen, 0, sizeof(UnackedOFDMBlockLen));
				
				SendData();				// Resend data in new mode
				return;
			}
		}
	}
	
	// Let timeout resend
				
	dttNextPlay = Now;		// Don't wait
	intTimeouts--;			// Keep stats clean
}

int ProcessOFDMAck(int AckType)
{
	unsigned long long ackbits = 0;
	int i;
	int Quality;
	UCHAR * FirstUnackedptr;
	int bytesacked = 0;		
	int AckedPercent;
	int Acked = 0;
	char Display[64] = "";

	if (AckType == OFDMACK)
	{
		strcpy(Display, "OFDMACK ");

		// Get acked frame bitmask from message

		for (i = 5; i >= 0; i--)
		{
			ackbits <<= 8;
			ackbits |= bytData[i];
		}
		Quality = (ackbits >> 43) << 2;
	}
	else
	{
		ackbits = 0xffffffffffff;			// DataAck or DataACKHiQ imply all acked
	
		if (AckType == DataACK)
			Quality = 80;
		else
			Quality = 90;
	}

	// Mark off all acked carriers

	for (i = 0; i < CarriersSent; i++)
	{
		if (ackbits & 1)     // carrier acked?
		{
			CarriersACKed++;
			Acked++;
			strcat(Display, "1");
			OFDMCarriersAcked[OFDMMode] ++;
		
			if (UnackedOFDMBlocks[SentOFDMBlocks[i]])		// Not Aleady acked?
			{
				UnackedOFDMBlocks[SentOFDMBlocks[i]] = 0;	// This block is acked
			}
		}
		else
		{
			CarriersNAKed++;
			strcat(Display, "0");
		}
		ackbits >>=1;
	}

	if (AckType == OFDMACK)
	{
		if (CarriersSent > 12)
			sprintf(Display, "OFDMACK %d/%d", Acked, CarriersSent);
	
		DrawRXFrame(1, Display);
	}
	else if (AckType == DataACK)
		DrawRXFrame(1, "DataACK");
		
	else if (AckType == DataACKHiQ)
		DrawRXFrame(1, "DataACKHiQ");

	AckedPercent = (CarriersACKed * 100) / (CarriersACKed + CarriersNAKed) ;

	FirstUnackedptr = memchr(UnackedOFDMBlocks, 1, 128);

	if (FirstUnackedptr)
	{
		// remove any acked data from queue. Can only remove from front (ie before FirstUnackedptr).

		int index = FirstUnackedptr - UnackedOFDMBlocks;
		int i = 0;

		while (i < index)
		{
			bytesacked += UnackedOFDMBlockLen[i];
			i++;
		}

		// Remove bytesacked from send buffer and move UnackedOFDMBlocks down.

		if (bytesacked)
		{
			RemoveDataFromQueue(bytesacked);
			memmove(UnackedOFDMBlocks, &UnackedOFDMBlocks[index], 128 - index);
			memmove(UnackedOFDMBlockLen, &UnackedOFDMBlockLen[index], 128 - index);

			memset(&UnackedOFDMBlocks[128 - index], 0, index);
			memset(&UnackedOFDMBlockLen[128 - index], 0, index);

			BytesSent -= bytesacked;

			NextOFDMBlock -= index;
			WriteDebugLog(LOGDEBUG, "OFDM Acked %d Blocks %d Bytes Still outstanding %d Carriers Acked %d OFDM Acked Percent %d Quality %d", index, bytesacked, BytesSent, Acked, AckedPercent, Quality);

			if (memchr(UnackedOFDMBlocks, 1, 128) == 0)
			{
				// All acked

				DontSendNewData = FALSE;
				LimitNewData = FALSE;
				Duplicate = FALSE;

				WriteDebugLog(LOGDEBUG, "OFDM All Acked");
			}
		}
		else
			WriteDebugLog(LOGDEBUG, "OFDM Nothing Acked Bytes Still outstanding %d Carriers Acked %d OFDM Acked Percent %d Quality %d", BytesSent, Acked, AckedPercent, Quality);

		// Actually, I don't think we need to wait for all acked. Both ends know (from
		// the OFDMAck) what is acked, and the IRS has passed to host. It is still
		// in the RX list, but will be removed when the next data frame is received.
		// So, as long as IRS discards any unacked data if the frame type or OFDM type 
		// changes it should be ok.


		if ((AckedPercent < 60) && (CarriersNAKed >= 2 * CarriersSent))
		{
			WriteDebugLog(LOGDEBUG, "OFDM Average below 60 - request shift down");

			intNAKctr = 0;
			intACKctr = 0;

			CarriersACKed = CarriersNAKed = 0;
			DontSendNewData = 0;
			NextOFDMBlock = 0;
			BytesSent = 0;
			memset(UnackedOFDMBlocks, 0, sizeof(UnackedOFDMBlocks));
			memset(UnackedOFDMBlockLen, 0, sizeof(UnackedOFDMBlockLen));
		
			if (OFDMMode > 0)
			{
				OFDMMode--;
				if (OFDMMode == QAM16)		// skip 16QAM
					OFDMMode = PSK8;

				WriteDebugLog(LOGDEBUG, "OFDM Acked Percent %d. Shift down to Mode %d", AckedPercent, OFDMMode) ;
			}
			else
			{
				// No lower OFDM modes, shift down frame type

				CarriersACKed = CarriersNAKed = 0;
				Gearshift_2(-10, False);		// Force shift
			}

			// We should only return -1 as a special flag
			// if nothing acked. We need to swap toggle,
			// as sending new data will swap again

			if (Acked)
				return Acked;
			
			return -1;			// Special value to stop repeats

		}

		WriteDebugLog(LOGDEBUG, "OFDM AckedPercent %d CarriersACKed %d CarriersSent %d", AckedPercent, CarriersACKed, CarriersSent);

		if ((AckedPercent > 80) && (CarriersACKed > CarriersSent))
		{
			// We would like to shift up, but can't till all acked. If we always miss
			// the odd carrier we will never shift up and will be very slow. So repeat missed
			// carriers to give us a high chance of all acked

			// Make sure there is somewhere to shift to

			if (OFDMMode < PSK16 || CarriersSent < 43)
			{
				// if next mode isn't working well delay shift up to prevent hunting

				int NextMode = 	OFDMMode + 1;
				int NextPercent= 0;
					
				if (NextMode == QAM16)
					NextMode = PSK16;		// Skip QAM for testing

				if (OFDMCarriersNaked[NextMode])	// Only if we've tried it
				{
					NextPercent = (100 * OFDMCarriersAcked[NextMode]) / OFDMCarriersNaked[NextMode];

					if (NextPercent < 20)		// Not working well
					{
						if ((CarriersACKed + CarriersNAKed) < CarriersSent * 8)
						{
							// dont shift

							return Acked;
						}
					}
				}

				LimitNewData = 2;		// Send outstanding 3 times
				Duplicate = 1;			// Duplicate New Data
				WriteDebugLog(LOGDEBUG, "OFDM Wants to shift up - limit new data");
			}
		}
	}
	else
	{
		// All acked - remove data from send buffer
		
		WriteDebugLog(LOGDEBUG, "OFDM All Acked %d Bytes removed from queue. AckedPercent %d Carriers Acked %d Quality %d", BytesSent, AckedPercent, Acked, Quality);
		RemoveDataFromQueue(BytesSent);
		NextOFDMBlock = 0;
		BytesSent = 0;
		memset(UnackedOFDMBlocks, 0, sizeof(UnackedOFDMBlocks));
		memset(UnackedOFDMBlockLen, 0, sizeof(UnackedOFDMBlockLen));
		DontSendNewData = FALSE;
		Duplicate = LimitNewData = FALSE;

		if (AckedPercent > 80 && CarriersACKed >= CarriersSent)
		{
			Gearshift_2(2, False);
			if (intShiftUpDn)
			{
				CarriersACKed = CarriersNAKed = 0;	// Reset counts
				OFDMMode = PSK4;
			}
			else
			{
				if (OFDMMode < PSK16)
				{
					OFDMMode++;
					if (OFDMMode == QAM16)
						OFDMMode = PSK16;		// Skip QAM dor testing
	
					if (OFDMCarriersNaked[OFDMMode])	// Only if we've tried it
					{
						int NextPercent = (100 * OFDMCarriersAcked[OFDMMode]) / OFDMCarriersNaked[OFDMMode];

						if (NextPercent < 20)		// Not working well
						{
							if ((CarriersACKed + CarriersNAKed) < CarriersSent * 8)
							{
								// dont shift
				
								OFDMMode--;
								if (OFDMMode == QAM16)
									OFDMMode = PSK8;		// Skip QAM dor testing

								return Acked;
							}
						}
					}
				
					CarriersACKed = CarriersNAKed = 0;
					WriteDebugLog(LOGDEBUG, "OFDM Acked Percent %d. Shift up to Mode %d", AckedPercent, OFDMMode) ;
					return Acked;
				}
			}
		}
	}
	
	return Acked;
}


int  GetNextOFDMBlockNumber(int * Len)
{
	BOOL Looping = 0;
resend:
	while (UnAckedBlockPtr >= 0)
	{
		if (UnackedOFDMBlocks[UnAckedBlockPtr])
		{
			*Len = UnackedOFDMBlockLen[UnAckedBlockPtr--];
			return UnAckedBlockPtr + 1;	// We send unacked blocks backwards
		}
		UnAckedBlockPtr--;
	}

	if (LimitNewData)
	{
		WriteDebugLog(LOGDEBUG, "LimitNewData Set - repeating unacked blocks");
		UnAckedBlockPtr = 127;		// Send unacked again
		LimitNewData--;
		goto resend;
	}

	if (DontSendNewData && Looping == 0)
	{
		WriteDebugLog(LOGDEBUG, "DontSendNewData Set - repeating unacked blocks");
		UnAckedBlockPtr = 127;		// Send unacked again
		Looping = 1;				// Protect against loop
		goto resend;
	}

	// No unacked blocks, send new

	NextOFDMBlock++; 
	*Len = -1;
	return NextOFDMBlock - 1;
}

UCHAR * GetNextOFDMBlock(int Block, int intDataLen)
{
	return  &bytDataToSend[Block * intDataLen];
}

void GetOFDMFrameInfo(int OFDMMode, int * intDataLen, int * intRSLen, int * Mode, int * Symbols)
{
	switch (OFDMMode)
	{
	case PSK2:

		*intDataLen = 19;
		*intRSLen = 6;				// Must be even
		*Symbols = 8;
		*Mode = 2;

		break;

	case PSK4:

		*intDataLen = 40;
		*intRSLen = 10;
		*Symbols = 4;
		*Mode = 4;
		break;

	case PSK8:

		*intDataLen = 57;			// Must be multiple of 3
		*intRSLen = 18;				// Must be multiple of 3 and even (so multiple of 6)
		*Symbols = 8;
		*Mode = 8;
		break;

	case PSK16:

		*intDataLen = 80;
		*intRSLen = 20;
		*Symbols = 2;
		*Mode = 16;

		break;

	case QAM16:

		*intDataLen = 80;
		*intRSLen = 20;
		*Symbols = 2;
		*Mode = 8;
		break;

	default:
				
		*intDataLen = *intRSLen = 0;
	}
}

int EncodeOFDMData(UCHAR bytFrameType, UCHAR * bytDataToSend, int Length, unsigned char * bytEncodedBytes)
{
	//  Output is a byte array which includes:
	//  1) A 2 byte Header which include the Frame ID.  This will be sent using 4FSK at 50 baud. It will include the Frame ID and ID Xored by the Session bytID.
	//  2) n sections one for each carrier that will include all data (with FEC appended) for the entire frame. Each block will be identical in length.

	//  Each carrier starts with an 8 bit block number, which may not be sequential (each carrier is ack'ed separately)
	//  and may be repeated (for redundancy)

	// OFDM Has several modes, selected by OFDMMode not Frame Type (all are OFDM.500 or OFDM.2500

	// For the moment will will send all carriers wirh the same mode (I don't think there is an advantage being different),
	// So we can use a different block length in each mode. We need to keep record of which blocks within bytDataToSend have
	// been acked. Note that the first block is always unacked - acked data at the front of buffer is removed.


	int intNumCar, intBaud, intDataLen, intRSLen, bytDataToSendLengthPtr, intEncodedDataPtr;

	int intCarDataCnt;
	BOOL blnOdd;
	char strType[18];
	char strMod[16];
	BOOL blnFrameTypeOK;
	UCHAR bytQualThresh;
	int i, j, Dummy;
	UCHAR * bytToRS = &bytEncodedBytes[2]; 
	int RepeatIndex = 0;			// used to duplicate data if too short to fill frame

	blnFrameTypeOK = FrameInfo(bytFrameType, &blnOdd, &intNumCar, strMod, &intBaud, &intDataLen, &intRSLen, &bytQualThresh, strType);

	if (intDataLen == 0 || Length == 0 || !blnFrameTypeOK)
		return 0;

	GetOFDMFrameInfo(OFDMMode, &intDataLen, &intRSLen, &Dummy, &Dummy);

	//	Generate the 2 bytes for the frame type data:

	CarriersSent = intNumCar;
	
	bytEncodedBytes[0] = bytFrameType;
	bytEncodedBytes[1] = bytFrameType ^ bytSessionID;

	bytDataToSendLengthPtr = 0;
	firstNewCarrier = -1;

	intEncodedDataPtr = 2;

	UnAckedBlockPtr = 127;		// We send unacked blocks backwards

	Length -= BytesSent;		// New data to send

	if (Length == 0)
		DontSendNewData = 1;

	WriteDebugLog(LOGDEBUG, "OFDM Bytes to Send %d DontSendNewData %d", Length, DontSendNewData);

	// Often the first carrier is the only one missed, and if we repeat it first it will always
	// fail. So it would be good if logical block number 0 isn't always sent on carrier 0
	
	// The carrier number must match the block number so we can ack it. 



	for (i = 0; i < intNumCar; i++)		//  across all carriers
	{
		int blkLen;
		int blkNum;

		intCarDataCnt = Length - bytDataToSendLengthPtr;

		// If we have no new data to send we would repeat the last sent blocks from
		// SentOFDMBlocks which is wrong if there is no new data. So in that case just
		// send outstanding data. 

		if (DontSendNewData && BytesSent)			// Just send outstanding data repeatedly if necessary
		{	
			OFDMBlocks[i] = blkNum = GetNextOFDMBlockNumber(&blkLen);
			OFDMBlockLen[i] = UnackedOFDMBlockLen[blkNum] = blkLen;
			UnackedOFDMBlocks[blkNum] = 1;

		}
		else if (Duplicate & (i >= ((intNumCar - firstNewCarrier) /2)))
			goto repeatblocks;

		else if (intCarDataCnt > intDataLen) // why not > ??
		{
			// Won't all fit 
tryagain:
			OFDMBlocks[i] = blkNum = GetNextOFDMBlockNumber(&blkLen);
	
			if (blkLen == -1)
			{
				// New Block. Make sure it will fit in window

				int limit;

				if (intNumCar == 9)
					limit = 24;			// Don't want too many outstanding or shift up will be slow
				else
					limit = 125;

				if (firstNewCarrier == -1)
					firstNewCarrier = i;

				if ((NextOFDMBlock + (intNumCar - i)) > limit)
				{
					// no room

					NextOFDMBlock--	;	// we aren't going to send it
					UnAckedBlockPtr = 127;	// send unacked again
					goto tryagain;
				}

				blkLen = intDataLen;
				bytDataToSendLengthPtr += intDataLen;	// Don't reduce bytes to send if repeating
				BytesSent += intDataLen;
			}

			OFDMBlockLen[i] = UnackedOFDMBlockLen[blkNum] = blkLen;
			UnackedOFDMBlocks[blkNum] = 1;
		}
		else
		{
			// Last bit

			memset(&bytToRS[0], 0, intDataLen);

			bytToRS[0] = intCarDataCnt;  // Could be 0 if insuffient data for # of carriers 

			if (intCarDataCnt > 0)
			{
				OFDMBlocks[i] = blkNum = GetNextOFDMBlockNumber(&blkLen);
				if (blkLen == -1)
				{
					if (firstNewCarrier == -1)
						firstNewCarrier = i;

					blkLen = intCarDataCnt;  // Could be 0 if insuffient data for # of carriers 
					bytDataToSendLengthPtr += intCarDataCnt;	// Don't reduce bytes to send if repeating
					BytesSent += intCarDataCnt;
					if (intCarDataCnt < intDataLen)
						DontSendNewData = TRUE;			// sending a part block so mustnt send more till all acked
				}
				
				UnackedOFDMBlockLen[blkNum] = OFDMBlockLen[i] = blkLen;
				UnackedOFDMBlocks[blkNum] = 1;
			}
			else
			{
				// No more data to send - duplicate sent carriers. Gives extra redundancy
repeatblocks:
				blkNum = OFDMBlocks[RepeatIndex];
				blkLen = OFDMBlockLen[RepeatIndex++];
				OFDMBlocks[i] = blkNum;
				OFDMBlockLen[i] = blkLen;
				UnackedOFDMBlockLen[blkNum] = blkLen;
				UnackedOFDMBlocks[blkNum] = 1;
			}
		}
	}

	// We now have pointers to the logical blocks in OFDMBlocks/Len. We don't
	// have to modulate in that order, but must update SentOFDMBlocks with the real
	// Carrier number

	j = rand() % intNumCar;

	for (i = 0; i < intNumCar; i++)
	{
		if (j >= intNumCar)
			j = 0;

		SentOFDMBlockLen[i] = bytToRS[0] = OFDMBlockLen[j];
		SentOFDMBlocks[i] = bytToRS[1] = OFDMBlocks[j++]; 

		WriteDebugLog(LOGDEBUG, "Sending OFDM Carrier %d Block %d Len %d", i,bytToRS[1], bytToRS[0]);
		memcpy(&bytToRS[2], GetNextOFDMBlock(bytToRS[1], intDataLen), bytToRS[0]);
	
		GenCRC16Normal(bytToRS, intDataLen + 2); // calculate the CRC on the byte count + data bytes

		// Data + RS + 1 byte byteCount + 1 byte blockno + 2 Byte CRC
		
		RSEncode(bytToRS, bytToRS+intDataLen+4, intDataLen + 4, intRSLen);  // Generate the RS encoding

 		intEncodedDataPtr += intDataLen + 4 + intRSLen;

		bytToRS += intDataLen + 4 + intRSLen;
	}

	return intEncodedDataPtr;
}


// OFDM RX Routines

extern int NErrors;

BOOL Decode4FSKOFDMACK()
{
	BOOL FrameOK;
	BOOL blnRSOK;

	// 6 Byte payload, 2 CRC 4 RS
 
	if (CheckCRC16(&bytFrameData[0][0], 6)) 
	{
		WriteDebugLog(LOGDEBUG, "OFDMACK Decode OK");
		return  TRUE;
	}

	// Try RS Correction


	FrameOK = RSDecode(&bytFrameData[0][0], 12, 4, &blnRSOK);

	if (FrameOK && blnRSOK == FALSE)
	{
		// RS Claims to have corrected it, but check

		WriteDebugLog(LOGDEBUG, "OFDMACK %d Errors Corrected by RS", NErrors);

		if (CheckCRC16(&bytFrameData[0][0], 6)) 
		{
			WriteDebugLog(LOGDEBUG, "OFDMACK Corrected by RS OK");
			return  TRUE;
		}
	}
	WriteDebugLog(LOGDEBUG, "OFDMACK Decode Failed after RS");

	return FALSE;
}



void RemoveProcessedOFDMData()
{
	// ISS has changed toggle, so last ack was processed.
	
	// if the last frame wasn't completely decoded then we need to remove any data sent to host and corresponding
	// entries in goodReceivedBlocks and goodReceivedBlockLen

	// This allows us to accumulate carriers from repeated frames. This could be good for FEC, but I think it is
	// of limited value for ARQ. Is it worth it ???


	int i, n, Len = 0;
	
	for (i = 0; i < 128; i++)
	{
		n = goodReceivedBlockLen[i];

		if (n)
			Len += n;
		else
			break;					// exit loop on first missed block.
	}

	// i is number of blocks to remove

	if (i == 0)
		return;

	WriteDebugLog(LOGDEBUG, "Removing %d received OFDM blocks Length %d", i, Len);

	memmove(goodReceivedBlocks, &goodReceivedBlocks[i], 128 - i);
	memmove(goodReceivedBlockLen, &goodReceivedBlockLen[i], 128 - i);
	memset(&goodReceivedBlocks[128 - i], 0, i);
	memset(&goodReceivedBlockLen[128 - i], 0, i);
	memmove(bytData, &bytData[Len], sizeof(bytData) - Len);
}


VOID InitDemodOFDM()
{
	// Called at start of frame

	int i;
	float dblPhase, dblReal, dblImag;
	short modePhase[MAXCAR][3];
	int OFDMType[MAXCAR] = {0};
	int ModeCount[8] = {0};
	int MaxModeCount = 0;
	char Msg[64];

	intSampPerSym = 240;

	floatCarFreq = 1500.0f + ((intNumCar /2) * 10000.0f) / 180.0f;		// Top freq (spacing is 10000/180)

	for (i= 0; i < intNumCar; i++)
	{
		// OFDM uses 55.5555 Hz carrier interval
						
		intCP[i] = 24;					//CP length
		intNforGoertzel[i] = 216;
		dblFreqBin[i] = floatCarFreq / 55.5555f;
	
		// Get initial Reference Phase
		
		GoertzelRealImag(intFilteredMixedSamples, intCP[i], intNforGoertzel[i], dblFreqBin[i], &dblReal, &dblImag);
		dblPhase = atan2f(dblImag, dblReal);

		// Set initial mag from Reference Phase and Mode Bits (which should be full power)

		intCarMagThreshold[i] = sqrtf(powf(dblReal, 2) + powf(dblImag, 2));

		intPSKPhase_1[i] = 1000 * dblPhase;

		// Get the 3 OFDM mode bits

		GoertzelRealImag(intFilteredMixedSamples + 240, intCP[i], intNforGoertzel[i], dblFreqBin[i], &dblReal, &dblImag);
		dblPhase = atan2f(dblImag, dblReal);

		intPSKPhase_0[i] = 1000 * atan2f(dblImag, dblReal);
		modePhase[i][0] = -(ComputeAng1_Ang2(intPSKPhase_0[i], intPSKPhase_1[i]));
		intPSKPhase_1[i] = intPSKPhase_0[i];
				
		intCarMagThreshold[i] += sqrtf(powf(dblReal, 2) + powf(dblImag, 2));


		GoertzelRealImag(intFilteredMixedSamples + 480, intCP[i], intNforGoertzel[i], dblFreqBin[i], &dblReal, &dblImag);
		dblPhase = atan2f(dblImag, dblReal);

		intPSKPhase_0[i] = 1000 * atan2f(dblImag, dblReal);
		modePhase[i][1] = -(ComputeAng1_Ang2(intPSKPhase_0[i], intPSKPhase_1[i]));
		intPSKPhase_1[i] = intPSKPhase_0[i];
				
		intCarMagThreshold[i] += sqrtf(powf(dblReal, 2) + powf(dblImag, 2));

		GoertzelRealImag(intFilteredMixedSamples + 720, intCP[i], intNforGoertzel[i], dblFreqBin[i], &dblReal, &dblImag);
		dblPhase = atan2f(dblImag, dblReal);
	
		intPSKPhase_0[i] = 1000 * atan2f(dblImag, dblReal);
		modePhase[i][2] = -(ComputeAng1_Ang2(intPSKPhase_0[i], intPSKPhase_1[i]));
		intPSKPhase_1[i] = intPSKPhase_0[i];
				
		intCarMagThreshold[i] += sqrtf(powf(dblReal, 2) + powf(dblImag, 2));
		intCarMagThreshold[i] *= 0.75f;

		// We have accumulated 4 values so divide by 4

		intCarMagThreshold[i] /= 4.0f;

		if (modePhase[i][0] >= 1572 || modePhase[i][0] <= -1572)
			 OFDMType[i] |= 1;

		if (modePhase[i][1] >= 1572 || modePhase[i][1] <= -1572)
			 OFDMType[i] |= 2;

		if (modePhase[i][2] >= 1572 || modePhase[i][2] <= -1572)
			 OFDMType[i] |= 4;

		floatCarFreq -= 55.555664f;  // Step through each carrier Highest to lowest which is equivalent to lowest to highest before RSB mixing. 
	}

	// Get RX Mode. May be corrupt on some carriers, so go with majority
	// But incorrectly seeing a change will cause corruption, so perhaps
	// need more than simple majority

	// Or maybe don't actually clear old data until we decode at least
	// one frame. That way an incorrect frame type won't cause a problem
	// (as frame won't decode). But what if type is correct and frame still
	// won't decode ?? 

	// So if almost all types aren't the same, and type is new, discard.


	
	for (i = 0; i < intNumCar; i++)
		ModeCount[OFDMType[i]]++;

	for (i = 0; i < 8; i++)
	{
		if (ModeCount[i] > MaxModeCount)
		{
			MaxModeCount = ModeCount[i];
			RXOFDMMode = i;
		}
	}

	if (MaxModeCount != intNumCar)
		WriteDebugLog(LOGDEBUG, "Not all OFDM Types the same (%d)", intNumCar - MaxModeCount);


	if (RXOFDMMode != lastOFDMRXMode)
	{
		// has changed. Only accept if all ok 
		// ?? is this a bit extreme ??. Try 1 error 

		if (MaxModeCount < (intNumCar - 1))
		{
			// Not sure. Safer to assume wrong
			// if it really has changed decode will fail
			// and frame repeat
			
			RXOFDMMode = lastOFDMRXMode;

			WriteDebugLog(LOGDEBUG, "New OFDM Mode but more than 1 carrier different (%d) - assume corrupt and don't change", intNumCar - MaxModeCount);
		}
	}

	GetOFDMFrameInfo(RXOFDMMode, &intDataLen, &intRSLen, &intPSKMode, &intSymbolsPerByte);

	// if OFDM mode (or frame type) has changed clear any received but unprocessed data

	// If we aren't going to decode because it is a repeat we don't need to 
	// check, as type can't have changed, and new type might be corrupt

	if (!RepeatedFrame || (memcmp(CarrierOk, Bad, intNumCar) == 0))
	{
		// We are going to decode it, so check

		if (RXOFDMMode != lastOFDMRXMode || (intFrameType & 0xFE) != (LastDemodType & 0xFE))
		{
			memset(goodReceivedBlocks, 0, sizeof(goodReceivedBlocks)); 
			memset(goodReceivedBlockLen, 0, sizeof(goodReceivedBlockLen));
			BytesSenttoHost = 0;

			lastOFDMRXMode = RXOFDMMode;

			if ((intFrameType & 0xFE) != (LastDemodType & 0xFE))
				WriteDebugLog(LOGDEBUG, "New OFDM Mode - clear any received data");
			else
				WriteDebugLog(LOGDEBUG, "New Frame Type - clear any received data");
		}
	}

	Track1CarPSK(floatCarFreq, intPSKMode, FALSE, TRUE, dblPhase, TRUE);

	SymbolsLeft = intDataLen + intRSLen + 4; // Data has length Blockno and CRC

	dblPhaseInc = 2 * M_PI * 1000 / intPSKMode;
	intPhasesLen = 0;

	PSKInitDone = TRUE;

	WriteDebugLog(LOGDEBUG, "OFDM Mode %s", OFDMModes[RXOFDMMode]);

	sprintf(Msg, "%s/%s", Name(intFrameType), OFDMModes[RXOFDMMode]);
	DrawRXFrame(0, Msg);
}

VOID Decode1CarOFDM(int Carrier)
{
	unsigned int intData;
	int k;
	float dblAlpha = 0.1f; // this determins how quickly the rolling average dblTrackingThreshold responds.

	// dblAlpha value of .1 seems to work well...needs to be tested on fading channel (e.g. Multipath)
	
	int Threshold = intCarMagThreshold[Carrier];
	int Len = intPhasesLen;

	UCHAR * Decoded = bytFrameData[0];			// Always use first buffer

	pskStart = 0;
	charIndex = 0;

	// We calculated initial mag from reference symbol

	// use filtered tracking of refernce phase amplitude
	// (should be full amplitude value)
     
	// On WGN this appears to improve decoding threshold about 1 dB 9/3/2016
    	
	while (Len >= 0)
	{
		// Phase Samples are in intPhases

		intData = 0;

		for (k = 0; k < 2; k++)
		{
			intData <<= 4;

			if (intPhases[Carrier][pskStart] < 393 && intPhases[Carrier][pskStart] > -393)
			{
			}		// Zero so no need to do anything
			else if (intPhases[Carrier][pskStart] >= 393 && intPhases[Carrier][pskStart] < 1179)
				intData += 1;
			else if (intPhases[Carrier][pskStart] >= 1179 && intPhases[Carrier][pskStart] < 1965)
				intData += 2;
			else if (intPhases[Carrier][pskStart] >= 1965 && intPhases[Carrier][pskStart] < 2751)
				intData += 3;
			else if (intPhases[Carrier][pskStart] >= 2751 || intPhases[Carrier][pskStart] < -2751)
				intData += 4;
			else if (intPhases[Carrier][pskStart] >= -2751 && intPhases[Carrier][pskStart] < -1965)
				intData += 5;
			else if (intPhases[Carrier][pskStart] >= -1965 && intPhases[Carrier][pskStart] <= -1179)
				intData += 6;
			else 
				intData += 7;

			if (intMags[Carrier][pskStart] < Threshold)
			{
				intData += 8;		//  add 8 to "inner circle" symbols. 
				Threshold = (Threshold * 900 + intMags[Carrier][pskStart] * 150) / 1000;
			}
			else
			{
				Threshold = ( Threshold * 900 + intMags[Carrier][pskStart] * 75) / 1000;
			}
		
			intCarMagThreshold[Carrier] = Threshold;
			pskStart++;
		}
		Decoded[charIndex++] = intData;
		Len -=2;
	}
}

BOOL DemodOFDM()
{
	int Used = 0;
	int Start = 0;
	int i, n, MemARQOk = 0;
	int skip = rand() % intNumCar;

	// We can't wait for the full frame as we don't have enough RAM, so
	// we do one DMA Buffer at a time, until we run out or end of frame

	// Only continue if we have enough samples

	while (State == AcquireFrame)
	{	
		if (PSKInitDone == 0)		// First time through
		{
			if (intFilteredMixedSamplesLength < (240 * 4))		// Reference and 3 Mode bits
				return FALSE;

			InitDemodOFDM();
			intFilteredMixedSamplesLength -= 4 * intSampPerSym;

			if (intFilteredMixedSamplesLength < 0)
				WriteDebugLog(LOGERROR, "Corrupt intFilteredMixedSamplesLength");

			Start += 4 * intSampPerSym;	

			// We normally don't decode Repeated frames. But if all carriers failed to
			// decode we should

			if (RepeatedFrame)
			{
				if (memcmp(CarrierOk, Bad, intNumCar) == 0)
					RepeatedFrame = FALSE;
			}
		}

		if (intFilteredMixedSamplesLength < intSymbolsPerByte * intSampPerSym + 10) 
		{
			// Move any unprocessessed data down buffer

			//	(while checking process - will use cyclic buffer eventually

			if (intFilteredMixedSamplesLength > 0)
				memmove(intFilteredMixedSamples,
					&intFilteredMixedSamples[Start], intFilteredMixedSamplesLength * 2); 

			return FALSE;
		}


		// call the decode char routine for each carrier

		// start at the highest carrier freq which is actually the lowest transmitted carrier due to Reverse sideband mixing
	
		floatCarFreq = 1500.0f + ((intNumCar / 2) * 10000.0f) / 180.0f;	// spacing is 10000/180 = 55.5555555
	
		for (i = 0; i < intNumCar; i++)
		{
			Used = Demod1CarOFDMChar(Start, i, intSymbolsPerByte);		// demods 2 phase values - enough for one char
			intPhasesLen -= intSymbolsPerByte;
			floatCarFreq -= 55.555664f;  // Step through each carrier Highest to lowest which is equivalent to lowest to highest before RSB mixing. 
		}

		intPhasesLen += intSymbolsPerByte;
		
		if (RXOFDMMode == PSK8)
			SymbolsLeft -=3;
		else
			SymbolsLeft--;		// number still to decode

	
		Start += Used;
		intFilteredMixedSamplesLength -= Used;

		if (intFilteredMixedSamplesLength < 0)
			WriteDebugLog(LOGERROR, "Corrupt intFilteredMixedSamplesLength");


		if (SymbolsLeft <= 0)	
		{
			// Frame complete - decode it

			DecodeCompleteTime = Now;

			// prepare for next so we can exit when we have finished decode

			DiscardOldSamples();
			ClearAllMixedSamples();
			State = SearchingForLeader;

			// Rick uses the last carrier for Quality
			// Get quality from middle carrier (?? is this best ??
	
			intLastRcvdFrameQuality = UpdatePhaseConstellation(&intPhases[intNumCar/2][0], &intMags[intNumCar/2][0], intPSKMode, FALSE, TRUE);

			// Decode the phases. Mode was determined from header

			frameLen = 0;

			if (RepeatedFrame)
			{
				WriteDebugLog(LOGDEBUG, "Repeated frame - discard");
	
				frameLen = BytesSenttoHost;
				return TRUE;
			}

			for (i = 0; i < intNumCar; i++)	
			{
				UCHAR decodeBuff[256];				// 82 doesnt seem to be enough ??Max length of OFDM block
				int decodeLen, ofdmBlock;;

				CarrierOk[i] = 0;					// Always reprocess carriers
		
				if (RXOFDMMode == QAM16)
					Decode1CarOFDM(i);
				else
					Decode1CarPSK(i, TRUE);

				// with OFDM each carrier has a sequence number, as we can do selective repeats if a carrier is missed.
				// so decode into a separate buffer, and copy good data into the correct place in the received data buffer. 

			decodeLen = CorrectRawDataWithRS(&bytFrameData[0][0], decodeBuff , intDataLen + 1, intRSLen, intFrameType, i);

				// if decode fails try with a tuning offset correction 

				if (CarrierOk[i] == 0)
				{
					CorrectPhaseForTuningOffset(&intPhases[i][0], intPhasesLen, intPSKMode);
	
					if (RXOFDMMode == QAM16)
						Decode1CarOFDM(i);
					else
						Decode1CarPSK(i, TRUE);
	
					decodeLen = CorrectRawDataWithRS(&bytFrameData[0][0], decodeBuff , intDataLen + 1, intRSLen, intFrameType, i);
				}
				
				OFDMCarriersReceived[RXOFDMMode]++;

				if (CarrierOk[i])
				{
					ofdmBlock = decodeBuff[0];

					// CRC check isn't perfect. At least we can check that Block and Length
					// are reasonable

					if (ofdmBlock < 128 && decodeLen <=  intDataLen)
					{
						// copy data to correct place in bytData

						OFDMCarriersDecoded[RXOFDMMode]++;

						if (goodReceivedBlocks[ofdmBlock] == 0)
						{
							memcpy(&bytData[intDataLen * ofdmBlock], &decodeBuff[1], decodeLen);
							goodReceivedBlocks[ofdmBlock] = 1;
							goodReceivedBlockLen[ofdmBlock] = decodeLen;
						}
					}
				}
			}

			// Pass any contiguous blocks starting from 0 to host (may need to reconsider!)

			for (i = 0; i < 128; i++)
			{
				n = goodReceivedBlockLen[i];

				if (n)
					frameLen += n;
				else
					break;					// exit loop on first missed block.
			}

			// If this is a repeated frame, we should only send any data that is beyond what we sent at last try
		
			BytesSenttoHost = frameLen;
		}

		// if all carriers have been decoded we must have passed all data to the host, so clear partial receive info

		if (memcmp(CarrierOk, Good, intNumCar) == 0)
		{
			 memset(goodReceivedBlocks, 0, sizeof(goodReceivedBlocks));
			 memset(goodReceivedBlockLen, 0, sizeof(goodReceivedBlockLen));
			 BytesSenttoHost = 0;
		}
	}
	return TRUE;
}

int Demod1CarOFDMChar(int Start, int Carrier, int intNumOfSymbols)
{
	// Converts intSample to an array of differential phase and magnitude values for the Specific Carrier Freq
	// intPtr should be pointing to the approximate start of the first reference/training symbol (1 of 3) 
	// intPhase() is an array of phase values (in milliradians range of 0 to 6283) for each symbol 
	// intMag() is an array of Magnitude values (not used in PSK decoding but for constellation plotting or QAM decoding)
	// Objective is to use Minimum Phase Error Tracking to maintain optimum pointer position

	//	It demodulates one byte's worth of samples (2 or 4)

	float dblReal, dblImag, dblPhase;
	int intMiliRadPerSample = floatCarFreq * M_PI / 6;
	int i;
	int origStart = Start;
//	int Corrections;

	// With OFDM we save received data in Receive buffer, so don't keep
	// the raw frames. So we must always decode
	
	if (RepeatedFrame)		// We just repeat previous ack/nak, so don't bother to decode
	{
		intPhasesLen += intNumOfSymbols;
		return intSampPerSym * intNumOfSymbols;
	}

	for (i = 0; i <  intNumOfSymbols; i++)
	{
		GoertzelRealImag(intFilteredMixedSamples, Start + intCP[Carrier], intNforGoertzel[Carrier], dblFreqBin[Carrier], &dblReal, &dblImag);
		intMags[Carrier][intPhasesLen] = sqrtf(powf(dblReal, 2) + powf(dblImag, 2));
		dblPhase =  atan2f(dblImag, dblReal);
		intPSKPhase_0[Carrier] = 1000 * dblPhase;
		intPhases[Carrier][intPhasesLen] = -(ComputeAng1_Ang2(intPSKPhase_0[Carrier], intPSKPhase_1[Carrier]));


		// Should we track each carrier ??
/*		
		if (Carrier == 0)
		{
			Corrections = Track1CarPSK(floatCarFreq, intPSKMode, FALSE, TRUE, dblPhase, FALSE);

			if (Corrections != 0)
			{
				Start += Corrections;

				GoertzelRealImag(intFilteredMixedSamples, Start + intCP[Carrier], intNforGoertzel[Carrier], dblFreqBin[Carrier], &dblReal, &dblImag);
				intPSKPhase_0[Carrier] = 1000 * atan2f(dblImag, dblReal);
			}
		}
*/
		intPSKPhase_1[Carrier] = intPSKPhase_0[Carrier];
		intPhasesLen++;
		Start += intSampPerSym;
	}
       
	if (AccumulateStats)
		intOFDMSymbolCnt += intNumOfSymbols;

	return (Start - origStart);	// Symbols we've consumed
}


VOID EncodeAndSendOFDMACK(UCHAR bytSessionID, int LeaderLength)
{
	// OFDMACK has one bit per carrier. As this needs 43 bits meassage is 6 bytes. The spare 5 bits are 
	// used to send quality

	unsigned long long val = intLastRcvdFrameQuality >> 2;
	int i;

	// Not sure if best to use CRC or FEC. Could use both, but message gets a bit long.
	// Lets go with crc for now
	// Now try CRC and 4 RS. OTT but see if it reduces number
	// of failed OFDMACKs

	bytEncodedBytes[0] = OFDMACK;
	bytEncodedBytes[1] = OFDMACK ^ bytSessionID;

	for (i = MAXCAR - 1; i >= 0; i--)
	{
		val <<= 1;

		if (CarrierOk[i])
			val |= 1;
	}

	for (i = 2; i < 8; i++)
	{
		bytEncodedBytes[i] = val & 0xff;
		val >>= 8;
	}

	GenCRC16Normal(&bytEncodedBytes[2], 6);	// calculate the CRC

	RSEncode(&bytEncodedBytes[2], &bytEncodedBytes[10], 8, 4);  // Generate the RS encoding ...now 14 bytes total

	Mod4FSKDataAndPlay(&bytEncodedBytes[0], 14, LeaderLength);

}

UCHAR bytLastSym[43];

float dblOFDMCarRatio = 0.5f; 


void SendOFDM2PSK(int symbol, int intNumCar)
{
	int intCarIndex = (MAXCAR - intNumCar) / 2;
	int intSample;
	int OFDMFrame[240] = {0};	// accumulated samples for each carrier
	short OFDMSamples[240];		// 216 data, 24 CP
	int i, n, p, q;					// start at 24, copy CP later

	for (i = 0; i < intNumCar; i++) // across all active carriers
	{					
		p = 24;
		memset(OFDMSamples, 0, sizeof(OFDMSamples));

		for (n = 0; n < 216; n++)
		{
			if (symbol)
				OFDMSamples[p++] -= intOFDMTemplate[intCarIndex][0][n]; 
			else
				OFDMSamples[p++] += intOFDMTemplate[intCarIndex][0][n]; 
		}

		// we now have the 216 samples. Copy last 24 to front as CP

		memcpy(OFDMSamples, &OFDMSamples[216], 24 * 2);

		// and add into the multicarrier value

		for (q = 0; q < 240; q++)
			OFDMFrame[q] += OFDMSamples[q];
			
		// now do the next carrier
				
		bytLastSym[intCarIndex] = symbol;	
		intCarIndex++;
	}
		
	// Done all carriers - send sample

	for (q = 0; q < 240; q++)
	{
		intSample = OFDMFrame[q] / intNumCar;
 		SampleSink((intSample * OFDMLevel)/100);		
	}
}



void ModOFDMDataAndPlay(unsigned char * bytEncodedBytes, int Len, int intLeaderLen)
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
	int i, j, k, s, n;
	int intCarStartIndex;
	int intPeakAmp;
	int intCarIndex;
	BOOL QAM = 0;
	int OFDMFrame[240] = {0};	// accumulated samples for each carrier
	short OFDMSamples[240];		// 216 data, 24 CP
	int p, q;					// start at 24, copy CP later
	char Msg[64];

	if (!FrameInfo(Type, &blnOdd, &intNumCar, strMod, &intBaud, &intDataLen, &intRSLen, &bytMinQualThresh, strType))
		return;

	intDataBytesPerCar = (Len - 2) / intNumCar;		// We queue the samples here, so dont copy below

	intCarIndex = intCarStartIndex = (MAXCAR - intNumCar) / 2;

	switch (OFDMMode)
	{
	case PSK2:

		s = 8;				// 8 symbols per byte
		break;

	case PSK4:

		s = 4;				// 4 symbols per byte
		break;

	case PSK8:

		s = 8;				// 8 symbols for 3 bytes
		break;

	case PSK16:

		s = 2;				// 2 symbols per byte
		break;

	case QAM16:

		s = 2;				// 2 symbols per byte
		QAM = 1;
		break;

	default:

		WriteDebugLog(LOGCRIT, "Undefined OFDM Mode %d", OFDMMode);
		return;
	}

	intSampPerSym = 216;			// 55 baud

	if (Type == PktFrameData)
	{
		intDataBytesPerCar = pktDataLen + pktRSLen + 3;
		intDataPtr = 11;		// Over Header
		goto PktLoopBack;
	}
	
	WriteDebugLog(LOGDEBUG, "Sending Frame Type %s Mode %s", strType, OFDMModes[OFDMMode]);

	sprintf(Msg, "%s/%s", strType, OFDMModes[OFDMMode]);
	DrawTXFrame(Msg);

	if (intNumCar == 9)
	{
		initFilter(500,1500);
		OFDMLevel = 100;	
	}
	else
	{
		initFilter(2500,1500);
		OFDMLevel = 125;
	}

	if (intLeaderLen == 0)
		intLeaderLenMS = LeaderLength;
	else
		intLeaderLenMS = intLeaderLen;
	
	// Create the leader

	SendLeaderAndSYNC(bytEncodedBytes, intLeaderLen);

	intPeakAmp = 0;

	intDataPtr = 2;  // initialize pointer to start of data.

PktLoopBack:		// Reenter here to send rest of variable length packet frame

         
	//	Now create a reference symbol for each carrier
      
	//	We have to do each carrier for each sample, as we write
	//	the sample immediately

	SendOFDM2PSK(0, intNumCar);				// Reference symbol is always zero, so same in any mode

	// Now send OFDM Type as 3 x 2PSK symbols. Same value sent on all carriers. The Type is send as 2PSK
	// bytLastSym ends up correct


	bytSym = (OFDMMode) & 1;
	bytSymToSend = ((bytLastSym[intCarIndex] + bytSym) & 1);  // Values 0=1
	SendOFDM2PSK(bytSymToSend, intNumCar);	

	bytSym = (OFDMMode >> 1) & 1;
	bytSymToSend = ((bytLastSym[intCarIndex] + bytSym) & 1);  // Values 0-1
	SendOFDM2PSK(bytSymToSend, intNumCar);

	bytSym = (OFDMMode >> 2) & 1;
	bytSymToSend = ((bytLastSym[intCarIndex] + bytSym) & 1);  // Values 0-1
	SendOFDM2PSK(bytSymToSend, intNumCar);

	// Correct bytLastSYM to match PSK level of actual mode

	for (i = intCarStartIndex; i < intCarStartIndex + intNumCar; i++)
	{
		if (OFDMMode == PSK4)
			bytLastSym[i] <<= 1;
		else if (OFDMMode == PSK8 || OFDMMode == QAM16)
			bytLastSym[i] <<= 2;
		if (OFDMMode == PSK16)
			bytLastSym[i] <<= 3;
	}

	// Unlike ARDOP_WIN we send samples as they are created,
	// so we loop through carriers, then data bytes

	for (j = 0; j < intDataBytesPerCar; j++)	//  for each referance and data symbol 
	{		
		// Loop through each symbol of byte (4 for PSK 2 for QAM
               
		for (k = 0; k < s; k++)
		{
			// with OFDM we must create separate samples for  each
			// carrier, so we can add the cyclic prefix

				intCarIndex = intCarStartIndex; // initialize the carrrier index
	
				for (i = 0; i < intNumCar; i++) // across all active carriers
				{
					if (OFDMMode == PSK2)
					{
						bytSym = (bytEncodedBytes[intDataPtr + i * intDataBytesPerCar] >> ((7 - k))) & 1;
						bytSymToSend = ((bytLastSym[intCarIndex] + bytSym) & 1);  // Values 0-1
					}
					else if (OFDMMode == PSK4)
					{
						bytSym = (bytEncodedBytes[intDataPtr + i * intDataBytesPerCar] >> (2 * (3 - k))) & 3;
						bytSymToSend = ((bytLastSym[intCarIndex] + bytSym) & 3);  // Values 0-3
					}
					else if (OFDMMode == PSK8)
					{
						// More complex ...must go through data in 3 byte chunks creating 8 Three bit symbols for each 3 bytes of data. 
     
						bytSym = GetSym8PSK(intDataPtr, k, i, bytEncodedBytes, intDataBytesPerCar);
						bytSymToSend = ((bytLastSym[intCarIndex] + bytSym) & 7);	// mod 8
					}
					else if (OFDMMode == PSK16)
					{
						bytSym = (bytEncodedBytes[intDataPtr + i * intDataBytesPerCar] >> (4 * (1 - k))) & 15;
						bytSymToSend = ((bytLastSym[intCarIndex] + bytSym) & 15);  // Values 0-3
					}
					else
					{
						// 16QAM
						
						bytSym = (bytEncodedBytes[intDataPtr + i * intDataBytesPerCar] >> (4 * (1 - k))) & 15;
						bytSymToSend = ((bytLastSym[intCarIndex] & 7) + (bytSym & 7) & 7); // Compute the differential phase to send
						bytSymToSend = bytSymToSend | (bytSym & 8); // add in the amplitude bit directly from symbol 
					}
					p = 24;
					memset(OFDMSamples, 0, sizeof(OFDMSamples));

					for (n = 0; n < intSampPerSym; n++)
					{
						if (OFDMMode == PSK2)
						{
							if (bytSymToSend) // This uses the symmetry of the symbols to reduce the table size by a factor of 2
								OFDMSamples[p++] -= intOFDMTemplate[intCarIndex][0][n];
							else
								OFDMSamples[p++]+= intOFDMTemplate[intCarIndex][0][n];
						}
						else if (OFDMMode == PSK4)
						{
							if (bytSymToSend < 2) // This uses the symmetry of the symbols to reduce the table size by a factor of 2
								OFDMSamples[p++] += intOFDMTemplate[intCarIndex][4 * bytSymToSend][n]; //  double the symbol value during template lookup for 4PSK. (skips over odd PSK 8 symbols)
							else
								OFDMSamples[p++] -= intOFDMTemplate[intCarIndex][4 * (bytSymToSend - 2)][n]; // subtract 2 from the symbol value before doubling and subtract value of table 
						}
						else if (OFDMMode == PSK16)
						{
							if (bytSymToSend < 8) // This uses the symmetry of the symbols to reduce the table size by a factor of 2
								OFDMSamples[p++] += intOFDMTemplate[intCarIndex][bytSymToSend][n]; //  double the symbol value during template lookup for 4PSK. (skips over odd PSK 8 symbols)
							else
								OFDMSamples[p++] -= intOFDMTemplate[intCarIndex][(bytSymToSend - 8)][n]; // subtract 2 from the symbol value before doubling and subtract value of table 
						}
						else
						{
							// This works for both 8PSK and 16QAM as 8PSK does'nt have the ampiltude bit
							// 4bits/symbol (use table symbol values 0, 1, 2, 3, -0, -1, -2, -3) and modulate amplitude with MSB
					
							if (bytSymToSend < 4)// This uses the symmetry of the symbols to reduce the table size by a factor of 2
								OFDMSamples[p++] = intOFDMTemplate[intCarIndex][2* bytSymToSend][n]; // double the symbol value during template lookup for 4PSK. (skips over odd PSK 8 symbols)
							else if (bytSymToSend < 8)
								OFDMSamples[p++] = -intOFDMTemplate[intCarIndex][2 * (bytSymToSend - 4)][n]; // subtract 4 from the symbol value before doubling and subtract value of table 
							else if (bytSymToSend < 12)
								OFDMSamples[p++] = dblOFDMCarRatio * intOFDMTemplate[intCarIndex][2 *(bytSymToSend - 8)][n]; // subtract 4 from the symbol value before doubling and subtract value of table         
							else
								OFDMSamples[p++] = -dblOFDMCarRatio * intOFDMTemplate[intCarIndex][2 * (bytSymToSend - 12)][n]; //  subtract 4 from the symbol value before doubling and subtract value of table 
						}
					}

					// we now have the 216 samples. Copy last 24 to front as CP

					memcpy(OFDMSamples, &OFDMSamples[216], 24 * 2);

					// and add into the multicarrier value

					for (q = 0; q < 240; q++)
						OFDMFrame[q] += OFDMSamples[q];
			
					// now do the next carrier
				
					bytLastSym[intCarIndex] = bytSymToSend;	
					intCarIndex++;
				}

		
				// Done all carriers - send sample

				for (q = 0; q < 240; q++)
				{
					intSample = OFDMFrame[q] / intNumCar;
			 		SampleSink((intSample * OFDMLevel)/100);		
					OFDMFrame[q] = 0;
				}
				
		}
		if (OFDMMode == PSK8)
		{
			intDataPtr += 3;
			j += 2;				// We've used 3 bytes
		}
		else
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
}



// Function to compute a 16 bit CRC value and check it against the last 2 bytes of Data (the CRC)
 
unsigned short int compute_crc(unsigned char *buf,int len);

BOOL  CheckCRC16(unsigned char * Data, int Length)
{
	// returns TRUE if CRC matches, else FALSE
    // For  CRC-16-CCITT =    x^16 + x^12 +x^5 + 1  intPoly = 1021 Init FFFF
    // intSeed is the seed value for the shift register and must be in the range 0-0xFFFF

	unsigned int CRC = GenCRC16(Data, Length);
	unsigned short CRC2 =  compute_crc(Data, Length);
	CRC2 ^= 0xffff;
  
	// Compare the register with the last two bytes of Data (the CRC) 
    
	if ((CRC >> 8) == Data[Length])
		if ((CRC & 0xFF) == Data[Length + 1])
			return TRUE;

	return FALSE;
}

// Subroutine to get intDataLen bytes from outbound queue (bytDataToSend)



