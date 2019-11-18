// ARDOP TNC ARQ Code
//

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

#include "ARDOPC.h"

#ifdef TEENSY
#define PKTLED LED3		// flash when packet received

extern unsigned int PKTLEDTimer;
#endif


extern UCHAR bytData[];
extern int intLastRcvdFrameQuality;
extern int intRmtLeaderMeasure;
extern BOOL blnAbort;
extern int intRepeatCount;
extern unsigned int dttLastFECIDSent;
extern unsigned int tmrSendTimeout;
extern BOOL blnFramePending;
extern int dttLastBusyTrip;
extern int dttPriorLastBusyTrip;
extern int dttLastBusyClear;


int intLastFrameIDToHost = 0;
int	intLastFailedFrameID = 0;
int	intLastARQDataFrameToHost = -1;
int	intShiftUpDn = 0;
int intFrameTypePtr = 0;	 // Pointer to the current data mode in bytFrameTypesForBW() 
int	intRmtLeaderMeas = 0;
int intTrackingQuality = -1;
UCHAR bytLastARQDataFrameSent = 0;  // initialize to an improper data frame
UCHAR bytLastARQDataFrameAcked = 0;  // initialize to an improper data frame
void ClearTuningStats();
void ClearQualityStats();
void updateDisplay();
void DrawTXMode(char * TXMode);

int bytQDataInProcessLen = 0;		// Lenght of frame to send/last sent

BOOL blnLastFrameSentData = FALSE;

extern char CarrierOk[10];
extern int LastDataFrameType;	
extern BOOL blnARQDisconnect;
extern const short FrameSize[64];

int intNAKLoQThresholds[6]; // the following two integer arrays hold the quality thresholds for making faster mode shifts, one value for each data type. 
int intACKHiQThresholds[6];

// ARQ State Variables

char AuxCalls[10][10] = {0};
int AuxCallsLength = 0;

int intBW;			// Requested connect speed
int intSessionBW;	// Negotiated speed

const char ARQBandwidths[9][12] = {"200", "500", "2500", "UNDEFINED"};
enum _ARQSubStates ARQState;

const char ARQSubStates[10][11] = {"None", "ISSConReq", "ISSConAck", "ISSData", "ISSId", "IRSConAck", "IRSData", "IRSBreak", "IRSfromISS", "DISCArqEnd"};

char strRemoteCallsign[10];
char strLocalCallsign[10];
char strFinalIDCallsign[10];
char strGridSquare[10];

UCHAR bytLastARQSessionID;
BOOL blnEnbARQRpt;
BOOL blnListen = TRUE;
BOOL Monitor = TRUE;
BOOL AutoBreak = TRUE;
BOOL blnBREAKCmd = FALSE;
BOOL BusyBlock = FALSE;

UCHAR bytPendingSessionID;
UCHAR bytSessionID = 0x3f;
BOOL blnARQConnected;

UCHAR bytCurrentFrameType = 0;	// The current frame type used for sending
UCHAR * bytFrameTypesForBW;		// Holds the byte array for Data modes for a session bandwidth. First are most robust, last are fastest
int bytFrameTypesForBWLength = 0;

UCHAR * bytShiftUpThresholds;
int bytShiftUpThresholdsLength;

BOOL blnPending;
int dttTimeoutTrip;
int intLastARQDataFrameToHost;
int intReceivedLeaderLen;
unsigned int tmrFinalID = 0;
unsigned int tmrIRSPendingTimeout = 0;
unsigned int tmrPollOBQueue;
UCHAR bytLastReceivedDataFrameType;
BOOL blnDISCRepeating;

int intRmtLeaderMeas;

int	intOBBytesToConfirm = 0;	// remaining bytes to confirm  
int	intBytesConfirmed = 0;		// Outbound bytes confirmed by ACK and squenced
int	intReportedLeaderLen = 0;	// Zero out the Reported leader length the length reported to the remote station 
BOOL blnLastPSNPassed = FALSE;	// the last PSN passed True for Odd, FALSE for even. 
BOOL blnInitiatedConnection = FALSE; // flag to indicate if this station initiated the connection
short dblAvgPECreepPerCarrier = 0; // computed phase error creep
int dttLastIDSent;				// date/time of last ID
int	intTotalSymbols = 0;		// To compute the sample rate error

extern int bytDataToSendLength;
int intFrameRepeatInterval;


extern int intLeaderRcvdMs;	

int intTrackingQuality;
int intNAKctr = 0;
int intACKctr = 0;
UCHAR bytLastACKedDataFrameType;

int EncodeConACKwTiming(UCHAR bytFrameType, int intRcvdLeaderLenMs, UCHAR bytSessionID, UCHAR * bytreturn);
int IRSNegotiateBW(int intConReqFrameType);
int GetNextFrameData(int * intUpDn, UCHAR * bytFrameTypeToSend, UCHAR * strMod, BOOL blnInitialize);
BOOL CheckForDisconnect();
BOOL Send10MinID();
void ProcessPingFrame(char * bytData);
void ProcessCQFrame(char * bytData);

void LogStats();
int ComputeInterFrameInterval(int intRequestedIntervalMS);
BOOL CheckForDisconnect();

// Tuning Stats

int intLeaderDetects;
int intLeaderSyncs;
int intAccumLeaderTracking;
float dblFSKTuningSNAvg;
int intGoodFSKFrameTypes;
int intFailedFSKFrameTypes;
int intAccumFSKTracking;
int intFSKSymbolCnt;
int intGoodFSKFrameDataDecodes;
int intFailedFSKFrameDataDecodes;
int intAvgFSKQuality;
int intFrameSyncs;
int intGoodPSKSummationDecodes;
int intGoodFSKSummationDecodes;
int intGoodQAMSummationDecodes;
float dblLeaderSNAvg;
int intAccumPSKLeaderTracking;
float dblAvgPSKRefErr;
int intPSKTrackAttempts;
int intAccumPSKTracking;
int intQAMTrackAttempts;
int intAccumQAMTracking;
int intPSKSymbolCnt;
int intQAMSymbolCnt;
int intGoodPSKFrameDataDecodes;
int intFailedPSKFrameDataDecodes;
int intGoodQAMFrameDataDecodes;
int intFailedQAMFrameDataDecodes;
int intAvgPSKQuality;
float dblAvgDecodeDistance;
int intDecodeDistanceCount;
int	intShiftUPs;
int intShiftDNs;
unsigned int dttStartSession;
int intLinkTurnovers;
int intEnvelopeCors;
float dblAvgCorMaxToMaxProduct;
int intConReqSN;
int intConReqQuality;
int intTimeouts;

char strLastStringPassedToHost[80] = "";	// Used to suppress duplicate CON Req reports


VOID GetNAKLoQLevels(int intBW)
{
	switch(intBW)
	{
	// These are the thresholds for each data type/bandwidth that determine when to send a NAK (Qual>NAKLoQLevel) or NAKLoQ (Qual<NAKLowQLevel)
	//Preliminary assignments subject to change  TODO: consider optimising using HF simulator

	case 200:

		intNAKLoQThresholds[0] = 0;
		intNAKLoQThresholds[1] = 60;
		intNAKLoQThresholds[2] = 60;
		break;

	case 500:

		intNAKLoQThresholds[0] = 0;
		intNAKLoQThresholds[1] = 55;
		intNAKLoQThresholds[2] = 55;
		intNAKLoQThresholds[3] = 60;
		break;

	case 2500:

		intNAKLoQThresholds[0] = 0;
		intNAKLoQThresholds[1] = 52;
		intNAKLoQThresholds[2] = 50;
		intNAKLoQThresholds[3] = 55;
		intNAKLoQThresholds[4] = 60;
		intNAKLoQThresholds[5] = 65;
		break;
	}
}


// Function to get ACK HiQ thresholds for the ARQ data modes 
VOID GetACKHiQLevels(int intBW)
{
	switch(intBW)
	{
	// These are the thresholds for each data type/bandwidth that determine when to send a NAK (Qual>NAKLoQLevel) or NAKLoQ (Qual<NAKLowQLevel)
	//Preliminary assignments subject to change  TODO: consider optimising using HF simulator

	case 200:

		intACKHiQThresholds[0] = 75;
		intACKHiQThresholds[1] = 80;
		intACKHiQThresholds[2] = 0;
		break;

	case 500:

		intACKHiQThresholds[0] = 72;
		intACKHiQThresholds[1] = 80;
		intACKHiQThresholds[2] = 70;
		intACKHiQThresholds[3] = 0;
		break;

	case 2500:

		intACKHiQThresholds[0] = 66;
		intACKHiQThresholds[1] = 77;
		intACKHiQThresholds[2] = 75;
		intACKHiQThresholds[3] = 80;
		intACKHiQThresholds[4] = 80;
		intACKHiQThresholds[5] = 0;
		break;
	}
}


// Subroutine to compute a 8 bit CRC value and append it to the Data...

UCHAR GenCRC8(char * Data)
{
	//For  CRC-8-CCITT =    x^8 + x^7 +x^3 + x^2 + 1  intPoly = 1021 Init FFFF

	int intPoly = 0xC6; // This implements the CRC polynomial  x^8 + x^7 +x^3 + x^2 + 1
	int intRegister  = 0xFF;
	int i; 
	unsigned int j;
	BOOL blnBit;

	for (j = 0; j < strlen(Data); j++)
	{
		int Val = Data[j];
		
		for (i = 7; i >= 0; i--) // for each bit processing MS bit first
		{
            blnBit = (Val & 0x80) != 0;
			Val = Val << 1;

			if ((intRegister & 0x80) == 0x80)  // the MSB of the register is set
			{
				// Shift left, place data bit as LSB, then divide
				// Register := shiftRegister left shift 1
				// Register := shiftRegister xor polynomial

				if (blnBit) 
					intRegister = 0xFF & (1 + 2 * intRegister);
				else
					intRegister = 0xFF & (2 * intRegister);
                 
				intRegister = intRegister ^ intPoly;
			}
			else
			{
				// the MSB is not set
				// Register is not divisible by polynomial yet.
				// Just shift left and bring current data bit onto LSB of shiftRegister

				if (blnBit)
					intRegister = 0xFF & (1 + 2 * intRegister);
				else
					intRegister = 0xFF & (2 * intRegister);
			}
		}
	}
	return intRegister & 0xFF; // LS 8 bits of Register 

}

int ComputeInterFrameInterval(int intRequestedIntervalMS)
{
	return max(1000, intRequestedIntervalMS + intRmtLeaderMeas);
}


//  Subroutine to Set the protocol state 

void SetARDOPProtocolState(int value)
{
	char HostCmd[24];

	if (ProtocolState == value)
		return;

	ProtocolState = value;

	displayState(ARDOPStates[ProtocolState]);

	newStatus = TRUE;				// report to PTC

        //Dim stcStatus As Status
        //stcStatus.ControlName = "lblState"
        //stcStatus.Text = ARDOPState.ToString

	switch(ProtocolState)
	{
	case DISC:

		blnARQDisconnect = FALSE; // always clear the ARQ Disconnect Flag from host.
		//stcStatus.BackColor = System.Drawing.Color.White
		blnARQConnected = FALSE;
		blnPending = FALSE;
		ClearDataToSend();
		SetLED(ISSLED, FALSE);
		SetLED(IRSLED, FALSE);

		break;

	case FECRcv:
		//stcStatus.BackColor = System.Drawing.Color.PowderBlue
		break;
		
	case FECSend:

		InitializeConnection();
		intLastFrameIDToHost = -1;
		intLastFailedFrameID = -1;
		//ReDim bytFailedData(-1)
		//stcStatus.BackColor = System.Drawing.Color.Orange
		break;

        //    Case ProtocolState.IRS
        //        stcStatus.BackColor = System.Drawing.Color.LightGreen

	case ISS:
	case IDLE:

		blnFramePending = FALSE;	//  Added 0.6.4 to insure any prior repeating frame is cancelled before new data. 
		blnEnbARQRpt = FALSE;
		SetLED(ISSLED, TRUE);
		SetLED(IRSLED, FALSE);
  
        //        stcStatus.BackColor = System.Drawing.Color.LightSalmon

		break;

	case IRS:
	case IRStoISS:

		SetLED(IRSLED, TRUE);
		SetLED(ISSLED, FALSE);
		bytLastACKedDataFrameType = 0;	// Clear on entry to IRS or IRS to ISS states. 3/15/2018

		break;


        //    Case ProtocolState.IDLE
        //        stcStatus.BackColor = System.Drawing.Color.NavajoWhite
        //    Case ProtocolState.OFFLINE
         //       stcStatus.BackColor = System.Drawing.Color.Silver
	}
	//queTNCStatus.Enqueue(stcStatus)

	sprintf(HostCmd, "NEWSTATE %s ", ARDOPStates[ProtocolState]);
	QueueCommandToHost(HostCmd);
}

 

//  Function to Get the next ARQ frame returns TRUE if frame repeating is enable 

BOOL GetNextARQFrame()
{
	//Dim bytToMod(-1) As Byte

	char HostCmd[80];

	if (blnAbort)  // handles ABORT (aka Dirty Disconnect)
	{
		//if (DebugLog) ;(("[ARDOPprotocol.GetNextARQFrame] ABORT...going to ProtocolState DISC, return FALSE")

		ClearDataToSend();
		
		SetARDOPProtocolState(DISC);
		InitializeConnection();
		blnAbort = FALSE;
		blnEnbARQRpt = FALSE;
		blnDISCRepeating = FALSE;
		intRepeatCount = 0;

		return FALSE;
	}

	if (blnDISCRepeating)	// handle the repeating DISC reply 
	{
		intRepeatCount += 1;
		blnEnbARQRpt = FALSE;

		if (intRepeatCount > 5)  // do 5 tries then force disconnect 
		{
			QueueCommandToHost("DISCONNECTED");
			sprintf(HostCmd, "STATUS END NOT RECEIVED CLOSING ARQ SESSION WITH %s", strRemoteCallsign);
			QueueCommandToHost(HostCmd);
			if (AccumulateStats) LogStats();

			blnDISCRepeating = FALSE;
			blnEnbARQRpt = FALSE;
			ClearDataToSend();
			SetARDOPProtocolState(DISC);
			intRepeatCount = 0;
			InitializeConnection();
			return FALSE;			 //indicates end repeat
		}
		WriteDebugLog(LOGDEBUG, "Repeating DISC %d", intRepeatCount);
		EncodeAndSend4FSKControl(DISCFRAME, bytSessionID, LeaderLength);

		return TRUE;			// continue with DISC repeats
	}

	if (ProtocolState == ISS || ProtocolState == IDLE)
		if (CheckForDisconnect())
			return FALSE;

	if (ProtocolState == ISS && ARQState == ISSConReq) // Handles Repeating ConReq frames 
	{
		intRepeatCount++;
		if (intRepeatCount > ARQConReqRepeats)
		{
		    ClearDataToSend();
			SetARDOPProtocolState(DISC);
			intRepeatCount = 0;
			blnPending = FALSE;
			displayCall(0x20, "");

			if (strRemoteCallsign[0])
			{
				sprintf(HostCmd, "STATUS CONNECT TO %s FAILED!", strRemoteCallsign);
				QueueCommandToHost(HostCmd);
				InitializeConnection();
				return FALSE;		// 'indicates end repeat
			}
			else
			{
				QueueCommandToHost("STATUS END ARQ CALL");
				InitializeConnection();
				return FALSE;		  //indicates end repeat
			}

			
			//Clear the mnuBusy status on the main form
            ///    Dim stcStatus As Status = Nothing
            //    stcStatus.ControlName = "mnuBusy"
            //    queTNCStatus.Enqueue(stcStatus)
		}

		return TRUE;		// ' continue with repeats
	}
	
	if (ProtocolState == ISS && ARQState == IRSConAck)
	{
		// Handles ISS repeat of ConAck

		intRepeatCount += 1;
		if (intRepeatCount <= ARQConReqRepeats)
			return TRUE;
		else
		{
			SetARDOPProtocolState(DISC);
			ARQState = DISCArqEnd;
			sprintf(HostCmd, "STATUS CONNECT TO %s FAILED!", strRemoteCallsign);
			QueueCommandToHost(HostCmd);
			intRepeatCount = 0;
			InitializeConnection();
			return FALSE;
		}
	}
	// Handles a timeout from an ARQ connected State

	if (ProtocolState == ISS || ProtocolState == IDLE || ProtocolState == IRS || ProtocolState == IRStoISS)
	{
		if ((Now - dttTimeoutTrip) / 1000 > ARQTimeout) // (Handles protocol rule 1.7)
		{
            if (!blnTimeoutTriggered)
			{
				WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.GetNexARQFrame] Timeout setting SendTimeout timer to start.");

				blnEnbARQRpt = FALSE;
				blnTimeoutTriggered = TRUE; // prevents a retrigger
                tmrSendTimeout = Now + 1000;
				return FALSE;
			}
		}
	}

	if (ProtocolState == DISC && intPINGRepeats > 0)
	{
		intRepeatCount++;
		if (intRepeatCount <= intPINGRepeats && blnPINGrepeating)
		{
			dttLastPINGSent = Now;
			return TRUE;				// continue PING
		}
		
		intPINGRepeats = 0;
		blnPINGrepeating = False;
        return FALSE;

	}

	// Handles the DISC state (no repeats)
 
	if (ProtocolState == DISC) // never repeat in DISC state
	{
		blnARQDisconnect = FALSE;
		intRepeatCount = 0;
		return FALSE;
	}

	// ' Handles all other possibly repeated Frames

	return blnEnbARQRpt;  // not all frame types repeat...blnEnbARQRpt is set/cleared in ProcessRcvdARQFrame
}

 
// function to generate 8 bit session ID

UCHAR GenerateSessionID(char * strCallingCallSign, char *strTargetCallsign)
{
	char bytToCRC[20];
	
	int Len = sprintf(bytToCRC, "%s%s", strCallingCallSign, strTargetCallsign);

	UCHAR ID = GenCRC8(bytToCRC);

	if ((ID & 0x3F) != 0x3F)
		ID &= 0x3F;
	else // rare case where the computed session ID woudl be 3F	
		ID = 0; //Remap a SessionID of 3F to 0...3F reserved for FEC mode
    
   	return ID;

}

// Function to compute the optimum leader based on the Leader sent and the reported Received leader

void CalculateOptimumLeader(int intReportedReceivedLeaderMS,int  intLeaderSentMS)
{
	intCalcLeader = max(200, 120 + intLeaderSentMS - intReportedReceivedLeaderMS);  //  This appears to work well on HF sim tests May 31, 2015
    //    WriteDebugLog(LOGDEBUG, ("[ARDOPprotocol.CalcualteOptimumLeader] Leader Sent=" & intLeaderSentMS.ToString & "  ReportedReceived=" & intReportedReceivedLeaderMS.ToString & "  Calculated=" & stcConnection.intCalcLeader.ToString)
}

 

// Function to determine if call is to Callsign or one of the AuxCalls

BOOL IsCallToMe(char * strCallsign)
{
	// returns true and sets bytReplySessionID if is to me.

	int i;
	
	if (strcmp(strCallsign, Callsign) == 0)
		return TRUE;
	
	for (i = 0; i < AuxCallsLength; i++)
	{
		if (strcmp(strCallsign, AuxCalls[i]) == 0)
			return TRUE;
	}

	return FALSE;
}
BOOL IsPingToMe(char * strCallsign)
{
	int i;
	
	if (strcmp(strCallsign, Callsign) == 0)
		return TRUE;
	
	for (i = 0; i < AuxCallsLength; i++)
	{
		if (strcmp(strCallsign, AuxCalls[i]) == 0)
			return TRUE;
	}

	return FALSE;
}
/*
ModeToSpeed() = {

	40 768
	42 
	44 1296
	46 429
	48
	4A 881
	4C
	4E 288

	50 1536
	52 2592
	54 
	56 4305
	58 429
	5A 329
	5C
	5E 

	60 3072
	62 5184
	64 
	66 8610
	68 1762
	6A
	6C
	6E 

	70 6144 
	72 10286
	74 
	76 17228
	78 3624
	7A 5863
	7C 4338
	7E 

	*/
// Function to get base (even) data modes by bandwidth for ARQ sessions

static UCHAR DataModes200[] = {D4PSK_200_50_E, D4PSK_200_100_E, D16QAM_200_100_E};
 
static UCHAR DataModes500[] = {D4FSK_500_50_E, D4PSK_500_50_E, D16QAMR_500_100_E, D16QAM_500_100_E};
static UCHAR DataModes500FSK[] = {D4FSK_500_50_E};

// 2000 Non-FM

static UCHAR DataModes2500[] = {D4FSK_500_50_E, D4FSK_1000_50_E,
								D4PSKR_2500_50_E, D4PSK_2500_50_E,
								D16QAMR_2500_100_E, D16QAM_2500_100_E};
static UCHAR DataModes2500FSK[] = {D4FSK_500_50_E, D4FSK_1000_50_E};

static UCHAR NoDataModes[1] = {0};

UCHAR  * GetDataModes(int intBW)
{
	// Revised version 0.3.5
	// idea is to use this list in the gear shift algorithm to select modulation mode based on bandwidth and robustness.
    // Sequence modes in approximate order of robustness ...most robust first, shorter frames of same modulation first

	if (intBW == 200)
	{
		bytFrameTypesForBWLength = sizeof(DataModes200);
		return DataModes200;
	}
	if (intBW == 500) 
	{
		if (FSKOnly)
		{
			bytFrameTypesForBWLength = sizeof(DataModes500FSK);
			return DataModes500FSK;
		}

		bytFrameTypesForBWLength = sizeof(DataModes500);
		return DataModes500;
	}

	if (intBW == 2500) 
	{
		if (FSKOnly)
		{
			bytFrameTypesForBWLength = sizeof(DataModes2500FSK);
			return DataModes2500FSK;
		}
	
		bytFrameTypesForBWLength = sizeof(DataModes2500);
		return DataModes2500;
	}
	bytFrameTypesForBWLength = 0;
	return NoDataModes;
}

// Function to get Shift up thresholds by bandwidth for ARQ sessions

static UCHAR byt200[] = {82, 84, 84, 85, 0};
static UCHAR byt500[] = {80, 84, 84, 75, 79, 0};
static UCHAR byt1000[] = {80, 80, 80, 80, 75, 0}; 
static UCHAR byt2000[] = {80, 80, 80, 76, 85, 75, 0}; //  Threshold for 8PSK 167 baud changed from 73 to 80 on rev 0.7.2.3
static UCHAR byt2000FM[] = {60, 85, 85, 0};
 
UCHAR * GetShiftUpThresholds(int intBW)
{
	//' Initial values determined by finding the following process: (all using Pink Gaussian Noise channel 0 to 3 KHz) 
	//'       1) Find Min S:N that will give reliable (at least 4/5 tries) decoding at the fastest mode for the bandwidth.
	//'       2) At that SAME S:N use the next fastest (more robust mode for the bandwidth)
	//'       3) Over several frames average the Quality of the decoded frame in 2) above That quality value is the one that
	//'       is then used as the shift up threshold for that mode. (note the top mode will never use a value to shift up).
	//'       This might be adjusted some but should along with a requirement for two successive ACKs make a good algorithm

	if (intBW == 200)
		return byt200;

	if (intBW == 500) 
		return byt500;

	if (intBW == 1000) 
		return byt1000;

	// default to 2000

	if (TuningRange > 0  && !Use600Modes)
		return byt2000;
	else
		return byt2000FM;
}


unsigned short  ModeHasWorked[16] = {0};		// used to attempt to make gear shift more stable.
unsigned short  ModeHasBeenTried[16] = {0};
unsigned short  ModeNAKS[16] = {0};

//  Subroutine to shift up to the next higher throughput or down to the next more robust data modes based on average reported quality 

float dblQuality = 0.0f;

VOID Gearshift_2(int intAckNakValue, BOOL blnInit)
{
	// Goal here is to create an algorithm that will:
	//	1) Shift up if the number of DataACKHiQ received outnumber DataACKs received 
	//	2) Don't shift on continous DataACKs
	//	3) Shift down quickly on a few DataNAKLoQ
	//	4) Shift down if DataNAKs exceed DataACKs .e.g.  < 50% ACKs 
	//	This can be refined later with different or dynamic Trip points etc. 
       

	char strOldMode[18] = "";
	char strNewMode[18] = "";

	int intBytesRemaining = bytDataToSendLength;
	
    float dblLowTrip = -0.0f; // May wish to adjust should be at least > -1
	float dblHiTrip = 1.2f;	  //  may wish to adjust should be at least  > 1
	float dblAlpha = 0.25;
	
	if (blnInit)
	{
		intShiftUpDn = 0;
		dblQuality = 1; // may want to optimize this initial value
		WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.Gearshift_2] dblQuality intitialized to %f", dblQuality);
		ModeHasBeenTried[intFrameTypePtr] = 1;
		return;
	}

	dblQuality = (dblAlpha * intAckNakValue) + ((1 - dblAlpha) * dblQuality);  // Exponenial low passfilter 

	if (intAckNakValue > 0)
	{
		ModeHasWorked[intFrameTypePtr]++;
		
		// if the next new mode has been tried before, and immediately failed, don't try again
		// till we get at least 5 sucessive acks

		if (ModeHasBeenTried[intFrameTypePtr + 1] && ModeHasWorked[intFrameTypePtr + 1] == 0)
		{
			dblHiTrip = 1.5;
		}
	}
	else
	{
		ModeNAKS[intFrameTypePtr]++;
		if (ModeHasWorked[intFrameTypePtr] == 0 && intFrameTypePtr > 0)	// This mode has never worked
			dblQuality = dblLowTrip;		// Revert immediately
	}

	if (dblQuality <= dblLowTrip) //  NAK Shift down conditions
	{
		if (intFrameTypePtr > 0)
		{
			// Can shift down

			strcpy(strOldMode, Name(bytFrameTypesForBW[intFrameTypePtr]));
			strOldMode[strlen(strOldMode) - 2] = 0;	// Remove .E
			strcpy(strNewMode, Name(bytFrameTypesForBW[intFrameTypePtr - 1]));
			strNewMode[strlen(strNewMode) - 2] = 0;
	
			WriteDebugLog(LOGINFO, "[ARDOPprotocol.Gearshift_2]  Shift Down: dblQuality= %f Shift down from Frame type %s New Mode: %s", dblQuality, strOldMode, strNewMode);
			intShiftUpDn = -1;
			intShiftDNs++;
			dblQuality = 1;  // preset to nominal middle on shift
			ModeHasBeenTried[intFrameTypePtr + intShiftUpDn] = 1;
		}
		else
		{
			// Low quality, but no more modes.
			// Limit Quality, so we can go back up without excessive retries

			dblQuality = dblLowTrip;
			WriteDebugLog(LOGINFO, "[ARDOPprotocol.Gearshift_2]  No Change possible: dblQuality= %f", dblQuality);
		}
	}
	else if (dblQuality > dblHiTrip)
	{
		if (intFrameTypePtr < (bytFrameTypesForBWLength - 1))
		{
			// Can shift up

	 		strcpy(strOldMode, Name(bytFrameTypesForBW[intFrameTypePtr]));
			strOldMode[strlen(strOldMode) - 2] = 0;
			strcpy(strNewMode, Name(bytFrameTypesForBW[intFrameTypePtr + 1]));
			strNewMode[strlen(strNewMode) - 2] = 0;
			WriteDebugLog(LOGINFO, "[ARDOPprotocol.Gearshift_2]  Shift Up: dblQuality= %f Shift up from Frame type %s New Mode: %s", dblQuality, strOldMode, strNewMode);
			intShiftUpDn = 1;
			dblQuality = 1.0f;		// preset to nominal middle on shift
			intShiftUPs++;

			ModeHasBeenTried[intFrameTypePtr + intShiftUpDn] = 1;
		}
		else
		{
			// Hi quality, but no more modes.
			// Limit Quality, so we can go back down without excessive retries

			dblQuality = dblHiTrip;
			WriteDebugLog(LOGINFO, "[ARDOPprotocol.Gearshift_2]  No Change possible: dblQuality= %f", dblQuality);
		}

		// In the future may want to use something  to base shifting on how many bytes remaining vs capacity of the current frame (low priority)  
	}
	else
	{
		WriteDebugLog(LOGINFO, "[ARDOPprotocol.Gearshift_2]  No Change: dblQuality= %f", dblQuality);
		intShiftUpDn = 0;
	}
}

int GetNumCarriers(UCHAR bytFrameType)
{
	int intNumCar, dummy;
	char strType[18];
	char strMod[16];
	
	if (FrameInfo(bytFrameType, &dummy, &intNumCar, strMod, &dummy, &dummy, &dummy, (UCHAR *)&dummy, strType))
		return intNumCar;
	
	return 0;
}
 
 // Subroutine to determine the next data frame to send (or IDLE if none) 

void SendData()
{
	char strMod[16];
	int Len;

	// Check for ID frame required (every 10 minutes)
	
	if (blnDISCRepeating)
		return;
	
	switch (ProtocolState)
	{
	case IDLE:

		WriteDebugLog(LOGDEBUG, "[ARDOPProtocol.SendData] Sending Data from IDLE state! Exit SendData");
		return;

	case ISS:
			
		if (CheckForDisconnect())
			return;
		
		Send10MinID();  // Send ID if 10 minutes since last

		if (bytDataToSendLength > 0)
		{
			WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.SendData] DataToSend = %d bytes, In ProtocolState ISS", bytDataToSendLength);

			//' Get the data from the buffer here based on current data frame type
			//' (Handles protocol Rule 2.1)

			Len = bytQDataInProcessLen = GetNextFrameData(&intShiftUpDn, &bytCurrentFrameType, strMod, FALSE);

			blnLastFrameSentData = TRUE;

			// This mechanism lengthens the intFrameRepeatInterval for multiple carriers (to provide additional decoding time at remote end)
			// This does not slow down the throughput significantly since if an ACK or NAK is received by the sending station 
			// the repeat interval does not come into play.

			switch(GetNumCarriers(bytCurrentFrameType))
			{
			case 1:
                intFrameRepeatInterval = ComputeInterFrameInterval(1500); // fairly conservative based on measured leader from remote end 
				break;

			case 2:
				intFrameRepeatInterval = ComputeInterFrameInterval(1700); //  fairly conservative based on measured leader from remote end 
				break;

			case 4:                
				intFrameRepeatInterval = ComputeInterFrameInterval(1900); // fairly conservative based on measured leader from remote end 
				break;
			
			case 8:
                intFrameRepeatInterval = ComputeInterFrameInterval(2100); // fairly conservative based on measured leader from remote end 
                break;

			default:
				intFrameRepeatInterval = 2000;  // shouldn't get here
			}

			dttTimeoutTrip = Now;
			blnEnbARQRpt = TRUE;
			ARQState = ISSData;		 // Should not be necessary

			if (strcmp(strMod, "4FSK") == 0)
			{
				EncLen = EncodeFSKData(bytCurrentFrameType, bytDataToSend, Len, bytEncodedBytes);
				Mod4FSKDataAndPlay(bytEncodedBytes, EncLen, intCalcLeader);  // Modulate Data frame 
			}
			else		// This handles PSK and QAM
			{
				EncLen = EncodePSKData(bytCurrentFrameType, bytDataToSend, Len, bytEncodedBytes);
				ModPSKDataAndPlay(bytEncodedBytes, EncLen, intCalcLeader);  // Modulate Data frame 
			}

			return;
		}
		else
		{
			// Nothing to send - set IDLE

			//ReDim bytQDataInProcess(-1) ' added 0.3.1.3
			SetARDOPProtocolState(IDLE);

			blnEnbARQRpt = TRUE;
			dttTimeoutTrip = Now;
			
			blnLastFrameSentData = FALSE;

			intFrameRepeatInterval = ComputeInterFrameInterval(2000);  // keep IDLE repeats at 2 sec 
			ClearDataToSend(); // ' 0.6.4.2 This insures new OUTOUND queue is updated (to value = 0)
	
			WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.SendData]  Send IDLE with Repeat, Set ProtocolState=IDLE ");
	
			EncodeAndSend4FSKControl(IDLEFRAME, bytSessionID, LeaderLength); // only returns when all sent
  			return;
		}
	}
}


 

//	a simple function to get an available frame type for the session bandwidth. 
    
int GetNextFrameData(int * intUpDn, UCHAR * bytFrameTypeToSend, UCHAR * strMod, BOOL blnInitialize)
{
	// Initialize if blnInitialize = true
	// Then call with intUpDn and blnInitialize = FALSE:
	//       intUpDn = 0 ' use the current mode pointed to by intFrameTypePtr
	//       intUpdn < 0    ' Go to a more robust mode if available limited to the most robust mode for the bandwidth 
	//       intUpDn > 0    ' Go to a less robust (faster) mode if avaialble, limited to the fastest mode for the bandwidth

	BOOL blnOdd;
	int intNumCar, intBaud, intDataLen, intRSLen;
	UCHAR bytQualThresh;
	char strType[18];
    char * strShift = NULL;
	int MaxLen;

	if (blnInitialize)	//' Get the array of supported frame types in order of Most robust to least robust
	{
		bytFrameTypesForBW = GetDataModes(intSessionBW);
		bytShiftUpThresholds = GetShiftUpThresholds(intSessionBW);

		GetNAKLoQLevels(intSessionBW);
		GetACKHiQLevels(intSessionBW);
 
		if (fastStart)
			intFrameTypePtr = ((bytFrameTypesForBWLength - 1) >> 1);	// Start mid way
		else
			intFrameTypePtr = 0;

		bytCurrentFrameType = bytFrameTypesForBW[intFrameTypePtr];
#ifdef PLOTCONSTELLATION
		DrawTXMode(shortName(bytCurrentFrameType));
		updateDisplay();
#endif
		if(DebugLog) WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.GetNextFrameData] Initial Frame Type: %s", Name(bytCurrentFrameType));
		Gearshift_2(0, True);  // initialize the gear shift settings and averages
		blnFramePending = False;
		*intUpDn = 0;
		return 0;
	}
	if (*intUpDn < 0)		// go to a more robust mode
	{
		if (intFrameTypePtr > 0)
		{
			intFrameTypePtr = max(0, intFrameTypePtr + *intUpDn);
			bytCurrentFrameType = bytFrameTypesForBW[intFrameTypePtr];
#ifdef PLOTCONSTELLATION
			DrawTXMode(shortName(bytCurrentFrameType));
			updateDisplay();
#endif
			strShift = "Shift Down";
		}
		*intUpDn = 0;
	}
	else if (*intUpDn > 0)	//' go to a faster mode
	{
		if (intFrameTypePtr < bytFrameTypesForBWLength)
		{
			intFrameTypePtr = min(bytFrameTypesForBWLength, intFrameTypePtr + *intUpDn);
			bytCurrentFrameType = bytFrameTypesForBW[intFrameTypePtr];
#ifdef PLOTCONSTELLATION
			DrawTXMode(shortName(bytCurrentFrameType));
			updateDisplay();
#endif
			strShift = "Shift Up";
		}
		*intUpDn = 0;
	}
        //If Not objFrameInfo.IsDataFrame(bytCurrentFrameType) Then
        //    Logs.Exception("[ARDOPprotocol.GetNextFrameData] Frame Type " & Format(bytCurrentFrameType, "X") & " not a data type.")
        //    Return Noth
	
	if ((bytCurrentFrameType & 1) == (bytLastARQDataFrameAcked & 1))
	{
		*bytFrameTypeToSend = bytCurrentFrameType ^ 1;  // This insures toggle of  Odd and Even 
		bytLastARQDataFrameSent = *bytFrameTypeToSend;
	}
	else
	{
		*bytFrameTypeToSend = bytCurrentFrameType;
		bytLastARQDataFrameSent = *bytFrameTypeToSend;
	}
	
	if (DebugLog)
	{
		if (strShift == 0)
			WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.GetNextFrameData] No shift, Frame Type: %s", Name(bytCurrentFrameType));
		else
			WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.GetNextFrameData] %s, Frame Type: %s", strShift, Name(bytCurrentFrameType));
	}

	FrameInfo(bytCurrentFrameType, &blnOdd, &intNumCar, strMod, &intBaud, &intDataLen, &intRSLen, &bytQualThresh, strType);

	MaxLen = intDataLen * intNumCar;

	if (strchr(strMod, 'R'))
		MaxLen = intDataLen * intNumCar / 2;
	else
		MaxLen = intDataLen * intNumCar;

	if (MaxLen > bytDataToSendLength)
		MaxLen = bytDataToSendLength;

	return MaxLen;
}
 

void InitializeConnection()
{
	// Sub to Initialize before a new Connection

	strRemoteCallsign[0] = 0; // remote station call sign
	intOBBytesToConfirm = 0; // remaining bytes to confirm  
	intBytesConfirmed = 0; // Outbound bytes confirmed by ACK and squenced
	intReceivedLeaderLen = 0; // Zero out received leader length (the length of the leader as received by the local station
	intReportedLeaderLen = 0; // Zero out the Reported leader length the length reported to the remote station 
	bytSessionID = 0x3F; //  Session ID 
	blnLastPSNPassed = FALSE; //  the last PSN passed True for Odd, FALSE for even. 
	blnInitiatedConnection = FALSE; //  flag to indicate if this station initiated the connection
	dblAvgPECreepPerCarrier = 0; //  computed phase error creep
	dttLastIDSent = Now ; //  date/time of last ID
	intTotalSymbols = 0; //  To compute the sample rate error
	strLocalCallsign[0] = 0; //  this stations call sign
	intSessionBW = 0; 
	bytLastACKedDataFrameType = 0;
	bytCurrentFrameType = 0;

	intCalcLeader = LeaderLength;

	ClearQualityStats();
	ClearTuningStats();

	memset(ModeHasWorked, 0, sizeof(ModeHasWorked));
	memset(ModeHasBeenTried, 0, sizeof(ModeHasBeenTried));
	memset(ModeNAKS, 0, sizeof(ModeNAKS));
}

// This sub processes a correctly decoded ConReq frame, decodes it an passed to host for display if it doesn't duplicate the prior passed frame. 

void ProcessUnconnectedConReqFrame(int intFrameType, UCHAR * bytData)
{
	char strDisplay[128];
	int Len;

	if (!(intFrameType >= ConReq200 && intFrameType <= ConReq2500))
		return;

	//" [ConReq2500 >  G8XXX] "
 
	Len = sprintf(strDisplay, " [%s > %s]", Name(intFrameType), bytData); 
	if (strcmp(strLastStringPassedToHost, strDisplay) == 0)
		return;

	AddTagToDataAndSendToHost(strDisplay, "ARQ", Len);
}

 
//	This is the main subroutine for processing ARQ frames 

void ProcessRcvdARQFrame(UCHAR intFrameType, UCHAR * bytData, int DataLen, BOOL blnFrameDecodedOK)
{
	//	blnFrameDecodedOK should always be true except in the case of a failed data frame ...Which is then NAK'ed if in IRS Data state
    
	int intReply;
	static UCHAR * strCallsign;
	int intReportedLeaderMS = 0;
	char HostCmd[80];
	int timeSinceDecoded = Now - DecodeCompleteTime;

	// Allow for link turnround before responding

	WriteDebugLog(LOGDEBUG, "Time since received = %d", timeSinceDecoded);

	if (timeSinceDecoded < 250)
		txSleep(250 - timeSinceDecoded);

	// Note this is called as part of the RX sample poll routine

	switch (ProtocolState)
	{
	case DISC:
		
		// DISC State *******************************************************************************************

		if (blnFrameDecodedOK && intFrameType == DISCFRAME) 
		{
			// Special case to process DISC from previous connection (Ending station must have missed END reply to DISC) Handles protocol rule 1.5
    
			WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame]  DISC frame received in ProtocolState DISC, Send END with SessionID= %X Stay in DISC state", bytLastARQSessionID);

			tmrFinalID = Now + 3000;			
			blnEnbARQRpt = FALSE;
			EncodeAndSend4FSKControl(END, bytLastARQSessionID, LeaderLength);

			return;
		}

		if (intFrameType == PING && blnFrameDecodedOK)
		{
			ProcessPingFrame(bytData);
            return;
		}

		if (intFrameType == CQ_de && blnFrameDecodedOK)
		{
			ProcessCQFrame(bytData);
            return;
		}
    
		// Process Connect request to MyCallsign or Aux Call signs  (Handles protocol rule 1.2)
   
		if (!blnFrameDecodedOK || intFrameType < ConReq200 || intFrameType > ConReq2500)
			return;			// No decode or not a ConReq

		strlop(bytData, ' ');	 // Now Just Tocall
		strCallsign  =  bytData;

		WriteDebugLog(LOGDEBUG, "CONREQ to %s Listen = %d", strCallsign, blnListen);

		if (!blnListen)
			return;			 // ignore connect request if not blnListen

		// see if connect request is to MyCallsign or any Aux call sign
        
		if (IsCallToMe(strCallsign)) // (Handles protocol rules 1.2, 1.3)
		{
			BOOL blnLeaderTrippedBusy;
			
			// This logic works like this: 
			// The Actual leader for this received frame should have tripped the busy detector making the last Busy trip very close
			// (usually within 100 ms) of the leader detect time. So the following requires that there be a Busy clear (last busy clear) following 
			// the Prior busy Trip AND at least 600 ms of clear time (may need adjustment) prior to the Leader detect and the Last Busy Clear
			// after the Prior Busy Trip. The initialization of times on objBusy.ClearBusy should allow for passing the following test IF there
			// was no Busy detection after the last clear and before the actual reception of the next frame. 

			blnLeaderTrippedBusy = (dttLastLeaderDetect - dttLastBusyTrip) < 300;
	
			if (BusyBlock)
			{
				if ((blnLeaderTrippedBusy && dttLastBusyClear - dttPriorLastBusyTrip < 600) 
				|| (!blnLeaderTrippedBusy && dttLastBusyClear - dttLastBusyTrip < 600))
				{
					WriteDebugLog(LOGDEBUG, "[ProcessRcvdARQFrame] Con Req Blocked by BUSY!  LeaderTrippedBusy=%d, Prior Last Busy Trip=%d, Last Busy Clear=%d,  Last Leader Detect=%d",
						blnLeaderTrippedBusy, Now - dttPriorLastBusyTrip, Now - dttLastBusyClear, Now - dttLastLeaderDetect);

					ClearBusy();
			
					// Clear out the busy detector. This necessary to keep the received frame and hold time from causing
					// a continuous busy condition.

 					EncodeAndSend4FSKControl(ConRejBusy, bytPendingSessionID, LeaderLength);

					sprintf(HostCmd, "REJECTEDBUSY %s", strRemoteCallsign);
					QueueCommandToHost(HostCmd);
					sprintf(HostCmd, "STATUS ARQ CONNECTION REQUEST FROM %s REJECTED, CHANNEL BUSY.", strRemoteCallsign);
					QueueCommandToHost(HostCmd);

					return;
				}
			}

			InitializeConnection();	

			intReply = IRSNegotiateBW(intFrameType); // NegotiateBandwidth

			if (intReply != ConRejBW)	// If not ConRejBW the bandwidth is compatible so answer with correct ConAck frame
			{
				GetNAKLoQLevels(intSessionBW);
				GetACKHiQLevels(intSessionBW);
 
				sprintf(HostCmd, "TARGET %s", strCallsign);
				QueueCommandToHost(HostCmd);

				bytDataToSendLength = 0;		// Clear queue
#ifdef TEENSY
				SetLED(TRAFFICLED, FALSE);
#endif
				blnPending = TRUE;				
				blnEnbARQRpt = FALSE;

				tmrIRSPendingTimeout = Now + 10000;  // Triggers a 10 second timeout before auto abort from pending

				// (Handles protocol rule 1.2)
                            
				dttTimeoutTrip = Now;
                            
				SetARDOPProtocolState(IRS);
				ARQState = IRSConAck; // now connected 

				intLastARQDataFrameToHost = -1;	 // precondition to an illegal frame type
 				memset(CarrierOk, 0, sizeof(CarrierOk));	// CLear MEM ARQ Stuff
				LastDataFrameType = -1;
  
				strcpy(strRemoteCallsign, bytData);
				strcpy(strLocalCallsign, strCallsign);
				strcpy(strFinalIDCallsign, strCallsign);

				intReceivedLeaderLen = intLeaderRcvdMs;		 // capture the received leader from the remote ISS's ConReq (used for timing optimization)
				dttLastFECIDSent = Now;
				EncodeAndSend4FSKControl(ConAck, bytPendingSessionID, 200);
			}
			else
			{
				// ' ConRejBW  (Incompatible bandwidths)

				// ' (Handles protocol rule 1.3)
             
				sprintf(HostCmd, "REJECTEDBW %s", strRemoteCallsign);
				QueueCommandToHost(HostCmd);
				sprintf(HostCmd, "STATUS ARQ CONNECTION REJECTED BY %s", strRemoteCallsign);
				QueueCommandToHost(HostCmd);
			
				EncodeAndSend4FSKControl(intReply, bytPendingSessionID, LeaderLength);
			}
		}
		else
		{
			// Not for us - cancel pending
			
			QueueCommandToHost("CANCELPENDING");
//			ProcessUnconnectedConReqFrame(intFrameType, bytData);  //  displays data if not connnected.  
		}
		blnEnbARQRpt = FALSE;
		return;
	

	case IRS:

		//IRS State ****************************************************************************************
		//  Process ConReq, ConAck, DISC, END, Host DISCONNECT, DATA, IDLE, BREAK 

		if (ARQState == IRSConAck)		// Process ConAck or ConReq if reply ConAck sent above in Case ProtocolState.DISC was missed by ISS
		{         
			if (!blnFrameDecodedOK)
				return;					// no reply if no correct decode

			// ConReq processing (to handle case of ISS missing initial ConAck from IRS)

			if (intFrameType >= ConReq200 && intFrameType <= ConReq2500) // Process Connect request to MyCallsign or Aux Call signs as for DISC state above (ISS must have missed initial ConACK from ProtocolState.DISC state)
			{
				if (!blnListen)
					return;
				
				// see if connect request is to MyCallsign or any Aux call sign

				strlop(bytData, ' ');	 // Now Just Tocall
				strCallsign  =  bytData;
       
				if (IsCallToMe(strCallsign)) // (Handles protocol rules 1.2, 1.3)
				{
					//WriteDebugLog(LOGDEBUG, "[ProcessRcvdARQFrame]1 strCallsigns(0)=" & strCallsigns(0) & "  strCallsigns(1)=" & strCallsigns(1) & "  bytPendingSessionID=" & Format(bytPendingSessionID, "X"))
            
					intReply = IRSNegotiateBW(intFrameType); // NegotiateBandwidth

					if (intReply != ConRejBW)	// If not ConRejBW the bandwidth is compatible so answer with correct ConAck frame
					{
						// Note: CONNECTION and STATUS notices were already sent from  Case ProtocolState.DISC above...no need to duplicate

  						SetARDOPProtocolState(IRS);
						ARQState = IRSConAck; // now connected 

						intLastARQDataFrameToHost = -1;	 // precondition to an illegal frame type
						memset(CarrierOk, 0, sizeof(CarrierOk));	// CLear MEM ARQ Stuff
						LastDataFrameType = -1;
  
						intReceivedLeaderLen = intLeaderRcvdMs;		 // capture the received leader from the remote ISS's ConReq (used for timing optimization)
						InitializeConnection();
						bytDataToSendLength = 0;

						dttTimeoutTrip = Now;

						//Stop and restart the Pending timer upon each ConReq received to ME
 						tmrIRSPendingTimeout= Now + 10000;  // Triggers a 10 second timeout before auto abort from pending

						strcpy(strRemoteCallsign, bytData);
						strcpy(strLocalCallsign, strCallsign);
						strcpy(strFinalIDCallsign, strCallsign);

						EncodeAndSend4FSKControl(ConAck, bytPendingSessionID, LeaderLength);
						// ' No delay to allow ISS to measure its TX>RX delay}
						return;
					}
		
					// ' ConRejBW  (Incompatible bandwidths)

					// ' (Handles protocol rule 1.3)
             
					//	WriteDebugLog(LOGDEBUG, ("[ProcessRcvdARQFrame] Incompatible bandwidth connect request. Frame type: " & objFrameInfo.Name(intFrameType) & "   MCB.ARQBandwidth:  " & MCB.ARQBandwidth)
  					
					sprintf(HostCmd, "REJECTEDBW %s", strRemoteCallsign);
					QueueCommandToHost(HostCmd);
					sprintf(HostCmd, "STATUS ARQ CONNECTION FROM %s REJECTED, INCOMPATIBLE BANDWIDTHS.", strRemoteCallsign);
					QueueCommandToHost(HostCmd);

					EncodeAndSend4FSKControl(intReply, bytPendingSessionID, LeaderLength);

					return;
				}

				//this normally shouldn't happen but is put here in case another Connect request to a different station also on freq...may want to change or eliminate this
				
				//if (DebugLog) WriteDebug("[ARDOPprotocol.ProcessRcvdARQFrame] Call to another target while in ProtocolState.IRS, ARQSubStates.IRSConAck...Ignore");

				return;
			}

			if (intFrameType == IDFRAME)
			{
				char * Loc = strlop(&bytData[3], ' ');
				char Msg[80];

				strcpy(strRemoteCallsign, &bytData[3]);
				strcpy(strGridSquare, &Loc[1]);
				strlop(strGridSquare, ']');

				sprintf(Msg, "CONNECTED %s %d [%s]", strRemoteCallsign, intSessionBW, strGridSquare);
				SendCommandToHost(Msg);

				sprintf(Msg, "STATUS ARQ CONNECTION ESTABLISHED WITH %s,BW=%d,GS=%s", strRemoteCallsign, intSessionBW, strGridSquare);
				SendCommandToHost(Msg);
				ProtocolState = IRS;
				ARQState = IRSData;		//  Now connected 
				tmrIRSPendingTimeout = 0;
				blnPending = False;
				blnARQConnected = True;
				blnEnbARQRpt = False;	// 'setup for no repeats 
				bytSessionID = bytPendingSessionID; // This sets the session ID now 

				EncodeAndSend4FSKControl(DataACK, bytSessionID, LeaderLength);

				 return;
			}
			// ConAck processing from ISS
/*                
			if (intFrameType == ConAck)	// Process ConACK frames from ISS confirming Bandwidth and providing ISS's received leader info.
			{
				// WriteDebugLog(LOGDEBUG, ("[ARDOPprotocol.ProcessRcvdARQFrame] IRS Measured RoundTrip = " & intARQRTmeasuredMs.ToString & " ms")
                      				
				CalculateOptimumLeader(10 * bytData[0], LeaderLength);

				bytSessionID = bytPendingSessionID; // This sets the session ID now 
               
				blnARQConnected = TRUE;
				blnPending = FALSE;
				tmrIRSPendingTimeout = 0;

				dttTimeoutTrip = Now;
				ARQState = IRSData;
				intLastARQDataFrameToHost = -1;
				intTrackingQuality = -1;
				intNAKctr = 0;
				dttLastFECIDSent = Now;

				blnEnbARQRpt = FALSE;
				sprintf(HostCmd, "CONNECTED %s %d", strRemoteCallsign, intSessionBW);
				QueueCommandToHost(HostCmd);
 
				sprintf(HostCmd, "STATUS ARQ CONNECTION FROM %s: SESSION BW = %d HZ", strRemoteCallsign, intSessionBW);
				QueueCommandToHost(HostCmd);

				EncodeAndSend4FSKControl(DataACK, bytSessionID, LeaderLength);

				// Initialize the frame type and pointer based on bandwidth (added 0.3.1.3)
				
				GetNextFrameData(&intShiftUpDn, 0, NULL, TRUE);		//' just sets the initial data, frame type, and sets intShiftUpDn= 0
          
				//       ' Update the main form menu status lable 
                //        Dim stcStatus As Status = Nothing
                //        stcStatus.ControlName = "mnuBusy"
                //       stcStatus.Text = "Connected " & .strRemoteCallsign
                 //       queTNCStatus.Enqueue(stcStatus)

				return;
			}
*/
		}

		if (ARQState == IRSData || ARQState == IRSfromISS)  // Process Data or ConAck if ISS failed to receive ACK confirming bandwidth so ISS repeated ConAck
		{
			// ConAck processing from ISS

			if (intFrameType == IDFRAME)
			{
				//  Process ID frames (ISS failed to receive prior ACK)

				// But beware of 10 Minute ID, which shouldnt be acked

				if ((Now - dttLastFECIDSent) > 300000)		// 5 mins - if connection takes that long something is wrong!
					return;

				      
				dttTimeoutTrip = Now;

				EncodeAndSend4FSKControl(DataACK, bytSessionID, LeaderLength);
				return;
			}

			// handles DISC from ISS

			if (blnFrameDecodedOK && intFrameType == DISCFRAME) //  IF DISC received from ISS Handles protocol rule 1.5
			{
				WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame]  DISC frame received in ProtocolState IRS, IRSData...going to DISC state");
                if (AccumulateStats) LogStats();
				
				QueueCommandToHost("DISCONNECTED");		// Send END
				sprintf(HostCmd, "STATUS ARQ CONNECTION ENDED WITH %s", strRemoteCallsign);
				QueueCommandToHost(HostCmd);
				
				bytLastARQSessionID = bytSessionID;  // capture this session ID to allow answering DISC from DISC state if ISS missed Sent END
                       
				ClearDataToSend();
				tmrFinalID = Now + 3000;
				blnDISCRepeating = FALSE;
				
				SetARDOPProtocolState(DISC);
                InitializeConnection();
				blnEnbARQRpt = FALSE;

				EncodeAndSend4FSKControl(END, bytSessionID, LeaderLength);
				return;
			}
			
			// handles END from ISS
			
			if (blnFrameDecodedOK && intFrameType == END) //  IF END received from ISS 
			{
				WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame]  END frame received in ProtocolState IRS, IRSData...going to DISC state");
				if (AccumulateStats) LogStats();
				
				QueueCommandToHost("DISCONNECTED");
				sprintf(HostCmd, "STATUS ARQ CONNECTION ENDED WITH %s", strRemoteCallsign);
				QueueCommandToHost(HostCmd);
				blnDISCRepeating = FALSE;
				ClearDataToSend();

				SetARDOPProtocolState(DISC);
				InitializeConnection();
				blnEnbARQRpt = FALSE;

				if (CheckValidCallsignSyntax(strLocalCallsign))
				{
					dttLastFECIDSent = Now;
					EncLen = Encode4FSKIDFrame(strLocalCallsign, GridSquare, bytEncodedBytes, 0x3F);
					Mod4FSKDataAndPlay(&bytEncodedBytes[0], EncLen, 0);		// only returns when all sent
				}
				

				return;
			}

			// handles BREAK from remote IRS that failed to receive ACK
			
			if (blnFrameDecodedOK && intFrameType == BREAK)
			{
				WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame]  BREAK received in ProtocolState %s , IRSData. Sending ACK", ARDOPStates[ProtocolState]);

				blnEnbARQRpt = FALSE; /// setup for no repeats

				// Send ACK

				EncodeAndSend4FSKControl(DataACK, bytSessionID, LeaderLength);
				dttTimeoutTrip = Now;
				return;
			}

			if (blnFrameDecodedOK && intFrameType == IDLEFRAME) //  IF IDLE received from ISS 
			{
				WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame]  IDLE received in ProtocolState %s substate %s", ARDOPStates[ProtocolState], ARQSubStates[ARQState]);
				{
					blnEnbARQRpt = FALSE; // setup for no repeats
				
					if (CheckForDisconnect())
						return;

					if ((AutoBreak && bytDataToSendLength > 0) || blnBREAKCmd)
					{
						// keep BREAK Repeats fairly short (preliminary value 1 - 3 seconds)
						intFrameRepeatInterval = ComputeInterFrameInterval(1000 + rand() % 2000);

						SetARDOPProtocolState(IRStoISS); // (ONLY IRS State where repeats are used)
						SendCommandToHost("STATUS QUEUE BREAK new Protocol State IRStoISS");
						blnEnbARQRpt = TRUE;  // setup for repeats until changeover
 						EncodeAndSend4FSKControl(BREAK, bytSessionID, LeaderLength);
					}
					else
					{
						// Send ACK

						dttTimeoutTrip = Now;
						EncodeAndSend4FSKControl(DataACK, bytSessionID, LeaderLength);
						dttTimeoutTrip = Now;
					}
					return;
				}
			}
            
			// handles DISCONNECT command from host
			
//			if (CheckForDisconnect())
//				return;

			// This handles normal data frames

			if (blnFrameDecodedOK && IsDataFrame(intFrameType)) // Frame even/odd toggling will prevent duplicates in case of missed ACK
			{
				if (intRmtLeaderMeas == 0)
				{
					intRmtLeaderMeas = intRmtLeaderMeasure;  // capture the leader timing of the first ACK from IRS, use this value to help compute repeat interval. 
					WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame] IRS (receiving data) RmtLeaderMeas=%d ms", intRmtLeaderMeas);
				}

				if (ARQState == IRSData && blnBREAKCmd && intFrameType != bytLastACKedDataFrameType)
				{
					// if BREAK Command and first time the new frame type is seen then 
					// Handles Protocol Rule 3.4 
					// This means IRS wishes to BREAK so start BREAK repeats and go to protocol state IRStoISS 
					// This implements the  important IRS>ISS changeover...may have to adjust parameters here for reliability 
					// The incorporation of intFrameType <> objMain.bytLastACKedDataFrameType insures only processing a BREAK on a frame
					// before it is ACKed to insure the ISS will correctly capture the frame being sent in its purge buffer. 

					dttTimeoutTrip = Now;
					blnBREAKCmd = FALSE;
					blnEnbARQRpt = TRUE;  // setup for repeats until changeover
					intFrameRepeatInterval = ComputeInterFrameInterval(1000 + rand() % 2000);
					SetARDOPProtocolState(IRStoISS); // (ONLY IRS State where repeats are used)
					SendCommandToHost("STATUS QUEUE BREAK new Protocol State IRStoISS");
 					EncodeAndSend4FSKControl(BREAK, bytSessionID, LeaderLength);
					return;
				}

				if (intFrameType != intLastARQDataFrameToHost) // protects against duplicates if ISS missed IRS's ACK and repeated the same frame  
				{
					AddTagToDataAndSendToHost(bytData, "ARQ", DataLen); // only correct data in proper squence passed to host   
					intLastARQDataFrameToHost = intFrameType;
					dttTimeoutTrip = Now;
				}
				else
                    WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame] Frame with same Type - Discard");
 				
				if (ARQState == IRSfromISS)
				{
					WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame] Data Rcvd in ProtocolState=IRSData, Substate IRSfromISS Go to Substate IRSData");
					ARQState = IRSData;   //This substate change is the final completion of ISS to IRS changeover and allows the new IRS to now break if desired (Rule 3.5) 
				}
			
				// Always ACK good data frame ...ISS may have missed last ACK

				blnEnbARQRpt = FALSE;
				
				if (intLastRcvdFrameQuality > intACKHiQThresholds[intFrameTypePtr])
					EncodeAndSend4FSKControl(DataACKHiQ, bytSessionID, LeaderLength);
				else
					EncodeAndSend4FSKControl(DataACK, bytSessionID, LeaderLength);

				bytLastACKedDataFrameType = intFrameType;
				return;
			}

			// handles Data frame which did not decode correctly but was previously ACKed to ISS  Rev 0.4.3.1  2/28/2016  RM
			// this to handle marginal decoding cases where ISS missed an original ACK from IRS, IRS passed that data to host, and channel has 
			//  deteriorated to where data decode is now now not possible. 
 
			if ((!blnFrameDecodedOK) && intFrameType == bytLastACKedDataFrameType)
			{
				EncodeAndSend4FSKControl(DataACK, bytSessionID, LeaderLength);
				blnEnbARQRpt = FALSE;

				WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame] Data Decode Failed but Frame Type matched last ACKed. Send ACK, data already passed to host. ");

				// handles Data frame which did not decode correctly (Failed CRC) and hasn't been acked before
			}
			else if ((!blnFrameDecodedOK) && IsDataFrame(intFrameType)) //Incorrectly decoded frame. Send NAK with Quality
			{
				if (ARQState == IRSfromISS)
				{
					WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame] Data Frame Type Rcvd in ProtocolState=IRSData, Substate IRSfromISS Go to Substate IRSData");

					ARQState = IRSData;  //This substate change is the final completion of ISS to IRS changeover and allows the new IRS to now break if desired (Rule 3.5) 
				}
				blnEnbARQRpt = FALSE;
				
				if (intLastRcvdFrameQuality < intNAKLoQThresholds[intFrameTypePtr])
					EncodeAndSend4FSKControl(DataNAKLoQ, bytSessionID, LeaderLength);
				else
					EncodeAndSend4FSKControl(DataNAK, bytSessionID, LeaderLength);

				return;
			}

			break;


			// IRStoISS State **************************************************************************************************

	
		case IRStoISS: // In this state answer any data frame with a BREAK. If ACK received go to Protocol State ISS

			break;
 
		case IDLE:			// The state where the ISS has no data to send and is looking for a BREAK from the IRS
 
			if (!blnFrameDecodedOK)
				return; // No decode so continue to wait

			if (intFrameType == DataACK && bytDataToSendLength > 0)	 // If ACK and Data to send
			{
				WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessedRcvdARQFrame] Protocol state IDLE, ACK Received with Data to send. Go to ISS Data state.");
					
				SetARDOPProtocolState(ISS);
				ARQState = ISSData;
				SendData(FALSE);
				return;
			}

			// process BREAK here Send ID if over 10 min. 

			if (intFrameType == BREAK)
			{
				// Initiate the transisiton to IRS

				dttTimeoutTrip = Now;
				blnEnbARQRpt = FALSE;
				EncodeAndSend4FSKControl(DataACK, bytSessionID, LeaderLength);

				WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame] BREAK Rcvd from IDLE, Go to IRS, Substate IRSfromISS");
                SendCommandToHost("STATUS BREAK received from Protocol State IDLE, new state IRS");
				SetARDOPProtocolState(IRS);
				//Substate IRSfromISS enables processing Rule 3.5 later
				ARQState = IRSfromISS; 
				
				intLinkTurnovers += 1;
				intLastARQDataFrameToHost = -1;  // precondition to an illegal frame type (insures the new IRS does not reject a frame)
				memset(CarrierOk, 0, sizeof(CarrierOk));	// CLear MEM ARQ Stuff
				LastDataFrameType = -1;
				return;
			}
			if (intFrameType == DISCFRAME) //  IF DISC received from IRS Handles protocol rule 1.5
			{
				WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame]  DISC frame received Send END...go to DISC state");
                
				if (AccumulateStats) LogStats();
					
				QueueCommandToHost("DISCONNECTED");
				sprintf(HostCmd, "STATUS ARQ CONNECTION ENDED WITH %s ", strRemoteCallsign);
				QueueCommandToHost(HostCmd);
				tmrFinalID = Now + 3000;
				blnDISCRepeating = FALSE;
				EncodeAndSend4FSKControl(END, bytSessionID, LeaderLength);
                bytLastARQSessionID = bytSessionID; // capture this session ID to allow answering DISC from DISC state
                ClearDataToSend();
                SetARDOPProtocolState(DISC);
				blnEnbARQRpt = FALSE;
				return;
			}
			if (intFrameType == END)
			{
				WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame]  END received ... going to DISC state");
				if (AccumulateStats) LogStats();
				QueueCommandToHost("DISCONNECTED");	
				sprintf(HostCmd, "STATUS ARQ CONNECTION ENDED WITH %s ", strRemoteCallsign);
				QueueCommandToHost(HostCmd);
				ClearDataToSend();

				if (CheckValidCallsignSyntax(strLocalCallsign))
				{
					dttLastFECIDSent = Now;
					EncLen = Encode4FSKIDFrame(strLocalCallsign, GridSquare, bytEncodedBytes, 0x3F);
					Mod4FSKDataAndPlay(&bytEncodedBytes[0], EncLen, 0);		// only returns when all sent
				}     
				SetARDOPProtocolState(DISC); 
				blnEnbARQRpt = FALSE;
				blnDISCRepeating = FALSE;
				return;
			}

			// Shouldn'r get here ??
  			return;
		}             


	// ISS state **************************************************************************************

	case ISS:
		
		if (ARQState == ISSConReq)  //' The ISS is sending Connect requests waiting for a ConAck from the remote IRS
		{
			// Session ID should be correct already (set by the ISS during first Con Req to IRS)
			// Process IRS Conack and capture IRS received leader for timing optimization
			// Process ConAck from IRS (Handles protocol rule 1.4)

			if (!blnFrameDecodedOK)
				return;

			if (intFrameType == ConAck)  // Process ConACK frames from IRS confirming BW is compatible and providing received leader info.
			{
				UCHAR bytDummy = 0;

				//WriteDebugLog(LOGDEBUG, ("[ARDOPprotocol.ProcessRcvdARQFrame] ISS Measured RoundTrip = " & intARQRTmeasuredMs.ToString & " ms")

				intSessionBW = atoi(ARQBandwidths[ARQBandwidth]);
    			
				CalculateOptimumLeader(10 * bytData[0], LeaderLength);
	
				// Initialize the frame type based on bandwidth
			
				GetNextFrameData(&intShiftUpDn, &bytDummy, NULL, TRUE);	// just sets the initial data frame type and sets intShiftUpDn = 0

				// prepare the ConACK answer with received leader length

				intReceivedLeaderLen = intLeaderRcvdMs;
				
				intFrameRepeatInterval = 2000;
				blnEnbARQRpt = TRUE;	// Setup for repeats of the ConACK if no answer from IRS
				WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame] Compatible bandwidth received from IRS ConAck: %d Hz", intSessionBW);
				ARQState = ISSConAck;
				dttLastFECIDSent = Now;

				// V2 sends ID not ConAck

				EncLen = Encode4FSKIDFrame(strLocalCallsign, GridSquare, bytEncodedBytes, bytSessionID);
				Mod4FSKDataAndPlay(&bytEncodedBytes[0], EncLen, 0);		// only returns when all sent

				return;
			}
			
			if (intFrameType == ConRejBusy) // ConRejBusy Handles Protocol Rule 1.5
			{
				WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame] ConRejBusy received from %s ABORT Connect Request", strRemoteCallsign);
 				sprintf(HostCmd, "REJECTEDBUSY %s", strRemoteCallsign);
				QueueCommandToHost(HostCmd);
				sprintf(HostCmd, "STATUS ARQ CONNECTION REJECTED BY %s, REMOTE STATION BUSY.", strRemoteCallsign);
				QueueCommandToHost(HostCmd);
				Abort();
				return;
			}
			if (intFrameType == ConRejBW) // ConRejBW Handles Protocol Rule 1.3
			{
				WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame] ConRejBW received from %s ABORT Connect Request", strRemoteCallsign);
 				sprintf(HostCmd, "REJECTEDBW %s", strRemoteCallsign);
				QueueCommandToHost(HostCmd);
				sprintf(HostCmd, "STATUS ARQ CONNECTION REJECTED BY %s, INCOMPATIBLE BW.", strRemoteCallsign);
				QueueCommandToHost(HostCmd);
				Abort();
				return;
			}
			return;		// Shouldn't get here

		}
		if (ARQState == ISSConAck)
		{
			if (blnFrameDecodedOK && (intFrameType == DataACK || intFrameType == BREAK))  // if ACK received then IRS correctly received the ISS ConACK 
			{	
				// Note BREAK added per input from John W. to handle case where IRS has data to send and ISS missed the IRS's ACK from the ISS's ConACK Rev 0.5.3.1
				// Not sure about this. Not needed with AUTOBREAK but maybe with BREAK command

				if (intRmtLeaderMeas == 0)
				{
					intRmtLeaderMeas = intRmtLeaderMeasure; // capture the leader timing of the first ACK from IRS, use this value to help compute repeat interval. 
					WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame] ISS RmtLeaderMeas=%d ms", intRmtLeaderMeas);
				}                       
				blnEnbARQRpt = FALSE;	// stop the repeats of ConAck and enables SendDataOrIDLE to get next IDLE or Data frame

				if (intFrameType == DataACK && DebugLog)
					WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame] ACK received in ARQState %s ", ARQSubStates[ARQState]);

				if (intFrameType == BREAK && DebugLog)
					WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame] BREAK received in ARQState %s Processed as implied ACK", ARQSubStates[ARQState]);
                                        
				blnARQConnected = TRUE;
				bytLastARQDataFrameAcked = 1; // initialize to Odd value to start transmission frame on Even
				blnPending = FALSE;
   			
				sprintf(HostCmd, "CONNECTED %s %d", strRemoteCallsign, intSessionBW);
				QueueCommandToHost(HostCmd);
				sprintf(HostCmd, "STATUS ARQ CONNECTION ESTABLISHED WITH %s, SESSION BW = %d HZ", strRemoteCallsign, intSessionBW);
				QueueCommandToHost(HostCmd);

				ARQState = ISSData;

				intTrackingQuality = -1; //initialize tracking quality to illegal value
				intNAKctr = 0;
		
				if (intFrameType == BREAK && bytDataToSendLength == 0)
				{
					//' Initiate the transisiton to IRS
	
					WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame] implied ACK, no data to send so action BREAK, send ACK");

					ClearDataToSend();
					blnEnbARQRpt = FALSE;  // setup for no repeats

					// Send ACK

					EncodeAndSend4FSKControl(DataACK, bytSessionID, LeaderLength);
					dttTimeoutTrip = Now;
					SetARDOPProtocolState(IRS);
					ARQState = IRSfromISS;  // Substate IRSfromISS allows processing of Rule 3.5 later
	
					intLinkTurnovers += 1;
					intLastARQDataFrameToHost = -1;		// precondition to an illegal frame type (insures the new IRS does not reject a frame)
					memset(CarrierOk, 0, sizeof(CarrierOk));	// CLear MEM ARQ Stuff
					LastDataFrameType = intFrameType;
				}
				else
					SendData();				// Send new data from outbound queue and set up repeats
				
				return;
			}

			if (blnFrameDecodedOK && intFrameType == ConRejBusy)  // ConRejBusy
			{
				WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame] ConRejBusy received in ARQState %s. Going to Protocol State DISC", ARQSubStates[ARQState]);

				sprintf(HostCmd, "REJECTEDBUSY %s", strRemoteCallsign);
				QueueCommandToHost(HostCmd);
				sprintf(HostCmd, "STATUS ARQ CONNECTION REJECTED BY %s", strRemoteCallsign);
				QueueCommandToHost(HostCmd);

				SetARDOPProtocolState(DISC);
				InitializeConnection();
				return;
			}
			if (blnFrameDecodedOK && intFrameType == ConRejBW)	 // ConRejBW
			{
				//if (DebugLog) WriteDebug("[ARDOPprotocol.ProcessRcvdARQFrame] ConRejBW received in ARQState " & ARQState.ToString & " Going to Protocol State DISC")

				sprintf(HostCmd, "REJECTEDBW %s", strRemoteCallsign);
				QueueCommandToHost(HostCmd);
				sprintf(HostCmd, "STATUS ARQ CONNECTION REJECTED BY %s", strRemoteCallsign);
				QueueCommandToHost(HostCmd);

				SetARDOPProtocolState(DISC);
				InitializeConnection();
				return;
			}
			return;				// Shouldn't get here
		}
		
		if (ARQState == ISSData)
		{
			if (CheckForDisconnect())
				return;					// DISC sent
			
			if (!blnFrameDecodedOK)
				return;					// No decode so continue repeating either data or idle
                    
			// process ACK, NAK, DISC, END or BREAK here. Send ID if over 10 min. 
 
			if (intFrameType == DataACKHiQ)	// if ACK
			{
				dttTimeoutTrip = Now;
				if (blnLastFrameSentData)
				{
#ifdef TEENSY
					SetLED(PKTLED, TRUE);		// Flash LED
					PKTLEDTimer = Now + 200;	// for 200 mS
#endif
					bytLastARQDataFrameAcked = bytLastARQDataFrameSent;
					
					if (bytQDataInProcessLen)
					{
						RemoveDataFromQueue(bytQDataInProcessLen);
						bytQDataInProcessLen = 0;
					}
					
					Gearshift_2(2, False);
					SendData();		 // Send new data from outbound queue and set up repeats
					return;
   				}
				return;
			}

			if (intFrameType == DataACK)	// if ACK
			{
				dttTimeoutTrip = Now;
				if (blnLastFrameSentData)
				{
#ifdef TEENSY
					SetLED(PKTLED, TRUE);		// Flash LED
					PKTLEDTimer = Now + 200;	// for 200 mS
#endif
					bytLastARQDataFrameAcked = bytLastARQDataFrameSent;
					
					if (bytQDataInProcessLen)
					{
						RemoveDataFromQueue(bytQDataInProcessLen);
						bytQDataInProcessLen = 0;
					}
					
					Gearshift_2(1, False);
					SendData();		 // Send new data from outbound queue and set up repeats
   				}
				return;

			}
			
			if (intFrameType == BREAK)
			{
				if (!blnARQConnected)
				{
					// Handles the special case of this ISS missed last Ack from the 
					// IRS ConAck and remote station is now BREAKing to become ISS.
					// clean up the connection status
					
					blnARQConnected = TRUE;
					blnPending = FALSE;

					sprintf(HostCmd, "CONNECTED %s %d", strRemoteCallsign, intSessionBW);
					QueueCommandToHost(HostCmd);
					sprintf(HostCmd, "STATUS ARQ CONNECTION ESTABLISHED WITH %s, SESSION BW = %d HZ", strRemoteCallsign, intSessionBW);
					QueueCommandToHost(HostCmd);
				}
				
				//' Initiate the transisiton to IRS
		
				WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame] BREAK Rcvd from ARQState ISSData, Go to ProtocolState IRS & substate IRSfromISS , send ACK");

				// With new rules IRS can use BREAK to interrupt data from ISS. It will only
				// be sent on IDLE or changed data frame type, so we know the last sent data
				// wasn't processed by IRS

				if (bytDataToSendLength)
					SaveQueueOnBreak();			// Save the data so appl can restore it 

				ClearDataToSend();
				blnEnbARQRpt = FALSE;  // setup for no repeats
				intTrackingQuality = -1; // 'initialize tracking quality to illegal value
				intNAKctr = 0;
				SendCommandToHost("STATUS BREAK received from Protocol State ISS, new state IRS");

				// Send ACK

				EncodeAndSend4FSKControl(DataACK, bytSessionID, LeaderLength);

				dttTimeoutTrip = Now;
				SetARDOPProtocolState(IRS);
				ARQState = IRSfromISS;  // Substate IRSfromISS allows processing of Rule 3.5 later
	
				intLinkTurnovers += 1;
				memset(CarrierOk, 0, sizeof(CarrierOk));	// CLear MEM ARQ Stuff
				LastDataFrameType = intFrameType;
				intLastARQDataFrameToHost = -1;		// precondition to an illegal frame type (insures the new IRS does not reject a frame)
				return;
			}
			
			if (intFrameType == DataNAK)		 // if NAK
			{
				if (blnLastFrameSentData)
				{
                    Gearshift_2(-1, False);
					
					if (intShiftUpDn != 0)
					{
						dttTimeoutTrip = Now;	 // Retrigger the timeout on a shift and clear the NAK counter
						SendData();		//Added 0.3.5.2     Restore the last frames data, Send new data from outbound queue and set up repeats
					}
				}
                			
				return;
			}

			if (intFrameType == DataNAKLoQ)		 // if NAK
			{
				if (blnLastFrameSentData)
				{
                    Gearshift_2(-2, False);
					
					if (intShiftUpDn != 0)
					{
						dttTimeoutTrip = Now;	 // Retrigger the timeout on a shift and clear the NAK counter
						SendData();		//Added 0.3.5.2     Restore the last frames data, Send new data from outbound queue and set up repeats
					}
				}
                			
				return;
			}

			if (intFrameType == DISCFRAME) // if DISC  Handles protocol rule 1.5
			{
				WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame]  DISC frame received Send END...go to DISC state");
				if (AccumulateStats) LogStats();
					
				QueueCommandToHost("DISCONNECTED");
				sprintf(HostCmd, "STATUS ARQ CONNECTION ENDED WITH %s", strRemoteCallsign);
				QueueCommandToHost(HostCmd);
      
				bytLastARQSessionID = bytSessionID; // capture this session ID to allow answering DISC from DISC state
				blnDISCRepeating = FALSE;
				tmrFinalID = Now + 3000;
				ClearDataToSend();
				SetARDOPProtocolState(DISC);
				InitializeConnection();
				blnEnbARQRpt = FALSE;

				EncodeAndSend4FSKControl(END, bytSessionID, LeaderLength);
				return;
			}
				
			if (intFrameType == END)	// ' if END
			{
				WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.ProcessRcvdARQFrame]  END received ... going to DISC state");
				if (AccumulateStats) LogStats();
					
				QueueCommandToHost("DISCONNECTED");
				sprintf(HostCmd, "STATUS ARQ CONNECTION ENDED WITH %s", strRemoteCallsign);
				QueueCommandToHost(HostCmd);
				ClearDataToSend();
				blnDISCRepeating = FALSE;

				if (CheckValidCallsignSyntax(strLocalCallsign))
				{
					dttLastFECIDSent = Now;
					EncLen = Encode4FSKIDFrame(strLocalCallsign, GridSquare, bytEncodedBytes, 0x3F);
					Mod4FSKDataAndPlay(&bytEncodedBytes[0], EncLen, 0);		// only returns when all sent
				}
					
				SetARDOPProtocolState(DISC);
				InitializeConnection();
				return;
			}
		}

	default:
		WriteDebugLog(LOGDEBUG, "Shouldnt get Here" );           
		//Logs.Exception("[ARDOPprotocol.ProcessRcvdARQFrame] 
	}
		// Unhandled Protocol state=" & GetARDOPProtocolState.ToString & "  ARQState=" & ARQState.ToString)
}


// Function to determine the IRS ConAck to reply based on intConReqFrameType received and local MCB.ARQBandwidth setting

int IRSNegotiateBW(int intConReqFrameType)
{
	//	returns the correct ConAck frame number to establish the session bandwidth to the ISS or the ConRejBW frame number if incompatible 
    //  if acceptable bandwidth sets stcConnection.intSessionBW

	switch (intConReqFrameType)
	{
	case ConReq200:

		if ((ARQBandwidth == XB200) || NegotiateBW)
		{
			intSessionBW = 200;
			return ConAck;
		}
		break;

	case ConReq500:

		if ((ARQBandwidth == XB500) || NegotiateBW && (ARQBandwidth == XB2500))
		{
			intSessionBW = 500;
			return ConAck;
		}
		break;

	case ConReq2500:
		
		if (ARQBandwidth == XB2500)
		{
			intSessionBW = 2500;
			return ConAck;
		}
		break;
	}

	return ConRejBW;
}

//	Function to send and ARQ connect request for the current MCB.ARQBandwidth
 
BOOL SendARQConnectRequest(char * strMycall, char * strTargetCall)
{
	// Psuedo Code:
	//  Determine the proper bandwidth and target call
	//  Go to the ISS State and ISSConREq sub state
	//  Encode the connect frame with extended Leader
	//  initialize the ConReqCount and set the Frame repeat interval
	//  (Handles protocol rule 1.1) 

	InitializeConnection();

	// Clear any queued data
	
	GetSemaphore();
	bytDataToSendLength = 0;
	FreeSemaphore();

#ifdef TEENSY
	SetLED(TRAFFICLED, FALSE);
#endif

	GetNAKLoQLevels(atoi(ARQBandwidths[ARQBandwidth]));
	GetACKHiQLevels(atoi(ARQBandwidths[ARQBandwidth]));
  
	intRmtLeaderMeas = 0;
	strcpy(strRemoteCallsign, strTargetCall);
	strcpy(strLocalCallsign, strMycall);
	strcpy(strFinalIDCallsign, strLocalCallsign);

    bytSessionID = GenerateSessionID(strMycall, strTargetCall);
 
	EncLen = EncodeARQConRequest(strMycall, strTargetCall, ARQBandwidth, bytEncodedBytes);

	if (EncLen == 0)
		return FALSE;
	
	// generate the modulation with 2 x the default FEC leader length...Should insure reception at the target
	// Note this is sent with session ID 0x3F

	//	Set all flags before playing, as the End TX is called before we return here
	blnAbort = FALSE;
	dttTimeoutTrip = Now;
	SetARDOPProtocolState(ISS);
	ARQState = ISSConReq;    
	intRepeatCount = 1;

	displayCall('>', strTargetCall);
	
	bytSessionID = GenerateSessionID(strMycall, strTargetCall);  // Now set bytSessionID to receive ConAck (note the calling staton is the first entry in GenerateSessionID) 
	bytPendingSessionID = bytSessionID;

	WriteDebugLog(LOGINFO, "[SendARQConnectRequest] strMycall=%s  strTargetCall=%s bytPendingSessionID=%x", strMycall, strTargetCall, bytPendingSessionID);
	blnPending = TRUE;
	blnARQConnected = FALSE;
	
	intFrameRepeatInterval = 2000;  // ms ' Finn reported 7/4/2015 that 1600 was too short ...need further evaluation but temporarily moved to 2000 ms
	blnEnbARQRpt = TRUE;

	Mod4FSKDataAndPlay(&bytEncodedBytes[0], EncLen, LeaderLength);		// only returns when all sent

	//' Update the main form menu status lable 
    //        Dim stcStatus As Status = Nothing
    //        stcStatus.ControlName = "mnuBusy"
    //        stcStatus.Text = "Calling " & strTargetCall
    //        queTNCStatus.Enqueue(stcStatus)

	return TRUE;
}


// Function to send 10 minute ID

BOOL Send10MinID()
{
	int dttSafetyBailout = 40;	// 100 mS intervals

	if (Now - dttLastFECIDSent > 600000 && !blnDISCRepeating)
	{

		// WriteDebugLog(LOGDEBUG, ("[ARDOPptocol.Send10MinID] Send ID Frame")
		// Send an ID frame (Handles protocol rule 4.0)

		blnEnbARQRpt = FALSE;

		dttLastFECIDSent = Now;
		EncLen = Encode4FSKIDFrame(strLocalCallsign, GridSquare, bytEncodedBytes, 0x3F);
		Mod4FSKDataAndPlay(&bytEncodedBytes[0], EncLen, 0);		// only returns when all sent		
		return TRUE;
	}
	return FALSE;
}

// Function to check for and initiate disconnect from a Host DISCONNECT command

BOOL CheckForDisconnect()
{
	if (blnARQDisconnect)
	{
		WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.CheckForDisconnect]  ARQ Disconnect ...Sending DISC (repeat)");
		
		QueueCommandToHost("STATUS INITIATING ARQ DISCONNECT");

		intFrameRepeatInterval = 2000;
		intRepeatCount = 1;
		blnARQDisconnect = FALSE;
		blnDISCRepeating = TRUE;
		blnEnbARQRpt = FALSE;

		// We could get here while sending an ACK (if host received a diconnect (bye) resuest
		// if so, don't send the DISC. ISS should go to Quiet, and we will repeat DISC

		if (SoundIsPlaying)
			return TRUE;

		EncodeAndSend4FSKControl(DISCFRAME, bytSessionID, LeaderLength);
		return TRUE;
	}
	return FALSE;
}

// subroutine to implement Host Command BREAK

void Break()
{
	time_t dttStartWait  = Now;

	if (ProtocolState != IRS)
	{
		WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.Break] |BREAK command received in ProtocolState: %s :", ARDOPStates[ProtocolState]);
		return;
	}
	
	WriteDebugLog(LOGDEBUG, "[ARDOPprotocol.Break] BREAK command received with AutoBreak = %d", AutoBreak);
	blnBREAKCmd = TRUE; // Set flag to process pending BREAK
}

// Function to abort an FEC or ARQ transmission 

void Abort()
{
	blnAbort = True;

	if (ProtocolState == IDLE || ProtocolState == IRS || ProtocolState == IRStoISS)
		GetNextARQFrame();
}

void ClearTuningStats()
{
	intLeaderDetects = 0;
	intLeaderSyncs = 0;
    intFrameSyncs = 0;
    intAccumFSKTracking = 0;
    intAccumPSKTracking = 0;
    intAccumQAMTracking = 0;
    intFSKSymbolCnt = 0;
    intPSKSymbolCnt = 0;
    intQAMSymbolCnt = 0;
    intGoodFSKFrameTypes = 0;
    intFailedFSKFrameTypes = 0;
    intGoodFSKFrameDataDecodes = 0;
    intFailedFSKFrameDataDecodes = 0;
    intGoodFSKSummationDecodes = 0;

    intGoodPSKFrameDataDecodes = 0;
    intGoodPSKSummationDecodes = 0;
    intFailedPSKFrameDataDecodes = 0;
    intGoodQAMFrameDataDecodes = 0;
    intGoodQAMSummationDecodes = 0;
    intFailedQAMFrameDataDecodes = 0;
    intAvgFSKQuality = 0;
    intAvgPSKQuality = 0;
    dblFSKTuningSNAvg = 0;
    dblLeaderSNAvg = 0;
    dblAvgPSKRefErr = 0;
    intPSKTrackAttempts = 0;
    intQAMTrackAttempts = 0;
    dblAvgDecodeDistance = 0;
    intDecodeDistanceCount = 0;
    intShiftDNs = 0;
    intShiftUPs = 0;
    dttStartSession = Now;
    intLinkTurnovers = 0;
    intEnvelopeCors = 0;
    dblAvgCorMaxToMaxProduct = 0;
	intConReqSN = 0;
	intConReqQuality = 0;
	intTimeouts = 0;
}

void ClearQualityStats()
{
	int4FSKQuality = 0;
    int4FSKQualityCnts = 0;
    int8FSKQuality = 0;
    int8FSKQualityCnts = 0;
    int16FSKQuality = 0;
    int16FSKQualityCnts = 0;
	intPSKQuality[0] = 0;
	intPSKQuality[1] = 0;
	intPSKQualityCnts[0] = 0;
	intPSKQualityCnts[1] = 0;	// Counts for 4PSK, 8PSK modulation modes 
    intFSKSymbolsDecoded = 0;
    intPSKSymbolsDecoded = 0;

	intQAMQuality = 0;
	intQAMQualityCnts = 0;
    intQAMSymbolsDecoded = 0;
}

// Sub to Write Tuning Stats to the Debug Log 

void LogStats()
{
	int intTotFSKDecodes = intGoodFSKFrameDataDecodes + intFailedFSKFrameDataDecodes;
	int intTotPSKDecodes = intGoodPSKFrameDataDecodes + intFailedPSKFrameDataDecodes;
	int i;

	Statsprintf("************************* ARQ session stats with %s  %d minutes ****************************", strRemoteCallsign, (Now - dttStartSession) /60000); 
	Statsprintf("     LeaderDetects= %d   AvgLeader S+N:N(3KHz noise BW)= %f dB  LeaderSyncs= %d", intLeaderDetects, dblLeaderSNAvg - 23.8, intLeaderSyncs);
	Statsprintf("     AvgCorrelationMax:MaxProd= %f over %d  correlations", dblAvgCorMaxToMaxProduct, intEnvelopeCors);
	Statsprintf("     FrameSyncs=%d  Good Frame Type Decodes=%d  Failed Frame Type Decodes =%d Timeouts =%d", intFrameSyncs, intGoodFSKFrameTypes, intFailedFSKFrameTypes, intTimeouts);
	Statsprintf("     Avg Frame Type decode distance= %f over %d decodes", dblAvgDecodeDistance, intDecodeDistanceCount);

	if (intGoodFSKFrameDataDecodes + intFailedFSKFrameDataDecodes + intGoodFSKSummationDecodes > 0)
	{
		Statsprintf(" ");
		Statsprintf("  FSK:");
       	Statsprintf("     Good FSK Data Frame Decodes= %d  RecoveredFSKCarriers with Summation=%d  Failed FSK Data Frame Decodes=%d", intGoodFSKFrameDataDecodes, intGoodFSKSummationDecodes, intFailedFSKFrameDataDecodes);
		Statsprintf("     AccumFSKTracking= %d   over %d symbols   Good Data Frame Decodes= %d   Failed Data Frame Decodes=%d", intAccumFSKTracking, intFSKSymbolCnt, intGoodFSKFrameDataDecodes, intFailedFSKFrameDataDecodes);
	}
	if (intGoodPSKFrameDataDecodes + intFailedPSKFrameDataDecodes + intGoodPSKSummationDecodes > 0)
	{
		Statsprintf(" ");
		Statsprintf("  PSK:");
		Statsprintf("     Good PSK Data Frame Decodes=%d  RecoveredPSKCarriers with Summation=%d  Failed PSK Data Frame Decodes=%d", intGoodPSKFrameDataDecodes, intGoodPSKSummationDecodes, intFailedPSKFrameDataDecodes);
		Statsprintf("     AccumPSKTracking=%d  %d attempts over %d total PSK Symbols",	intAccumPSKTracking, intPSKTrackAttempts, intPSKSymbolCnt);
	
		Statsprintf(" ");
	}
	if (intGoodQAMFrameDataDecodes + intFailedQAMFrameDataDecodes + intGoodQAMSummationDecodes > 0)
	{
		Statsprintf(" ");
		Statsprintf("  QAM:");
		Statsprintf("     Good QAM Data Frame Decodes=%d  RecoveredQAMCarriers with Summation=%d  Failed QAM Data Frame Decodes=%d", intGoodQAMFrameDataDecodes, intGoodQAMSummationDecodes, intFailedQAMFrameDataDecodes);
		Statsprintf("     AccumQAMTracking=%d  %d attempts over %d total QAM Symbols",	intAccumQAMTracking, intQAMTrackAttempts, intQAMSymbolCnt);
	
		Statsprintf(" ");
	}
   
	Statsprintf("  Squelch= %d BusyDet= %d Mode Shift UPs= %d   Mode Shift DOWNs= %d  Link Turnovers= %d",
		Squelch, BusyDet, intShiftUPs, intShiftDNs, intLinkTurnovers);
	Statsprintf(" ");
	Statsprintf("  Received Frame Quality:");

	if (int4FSKQualityCnts > 0)
		Statsprintf("     Avg 4FSK Quality=%d on %d frame(s)",  int4FSKQuality / int4FSKQualityCnts, int4FSKQualityCnts);

	if (int8FSKQualityCnts > 0)
		Statsprintf("     Avg 8FSK Quality=%d on %d frame(s)",  int8FSKQuality / int8FSKQualityCnts, int8FSKQualityCnts);

	if (int16FSKQualityCnts > 0)
		Statsprintf("     Avg 16FSK Quality=%d on %d frame(s)",  int16FSKQuality / int16FSKQualityCnts, int16FSKQualityCnts);

	if (intPSKQualityCnts[0] > 0)
		Statsprintf("     Avg 4PSK Quality=%d on %d frame(s)",  intPSKQuality[0] / intPSKQualityCnts[0], intPSKQualityCnts[0]);

	if (intPSKQualityCnts[1] > 0)
		Statsprintf("     Avg 8PSK Quality=%d on %d frame(s)",  intPSKQuality[1] / intPSKQualityCnts[1], intPSKQualityCnts[1]);

	if (intQAMQualityCnts > 0)
		Statsprintf("     Avg QAM Quality=%d on %d frame(s)",  intQAMQuality / intQAMQualityCnts, intQAMQualityCnts);

	// Experimental logging of Frame Type ACK and NAK counts

	Statsprintf("");
	Statsprintf("Type               ACKS  NAKS");

	for (i = 0; i < bytFrameTypesForBWLength; i++)
	{
		Statsprintf("%-17s %5d %5d", Name(bytFrameTypesForBW[i]), ModeHasWorked[i], ModeNAKS[i]);
	}


	Statsprintf("************************************************************************************************");

	CloseStatsLog();
	CloseDebugLog();		// Flush debug log
}


