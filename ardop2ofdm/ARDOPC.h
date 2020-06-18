

#ifndef ARDOPCHEADERDEFINED
#define ARDOPCHEADERDEFINED

#ifdef CONST
#undef CONST
#endif
#define CONST const	// for building sample arrays

extern const char ProductName[];
extern const char ProductVersion[];

//#define USE_SOUNDMODEM

#define UseGUI			// Enable GUI Front End Support

#ifndef TEENSY
#ifdef UseGUI

// Constellation and Waterfall for GUI interface

#define PLOTCONSTELLATION
#define PLOTWATERFALL
#define PLOTSPECTRUM
#define ConstellationHeight 90
#define ConstellationWidth 90
#define WaterfallWidth 205
#define WaterfallHeight 64
#define SpectrumWidth 205
#define SpectrumHeight 64

#define PLOTRADIUS 42
#define WHITE 0
#define Tomato 1
#define Gold 2
#define Lime 3	
#define Yellow 4
#define Orange 5
#define Khaki 6
#define Cyan 7
#define DeepSkyBlue 8
#define RoyalBlue 9
#define Navy 10
#define Black 11 
#define Goldenrod 12
#define Fuchsia 13

#endif
#endif



//	Sound interface buffer size

#define SendSize 1200		// 100 mS for now
#define ReceiveSize 240	// try 100 mS for now
#define NumberofinBuffers 4

#define MAXCAR 43			// Max OFDM Carriers

#define DATABUFFERSIZE 11000

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#ifndef WIN32
#define max(x, y) ((x) > (y) ? (x) : (y))
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifdef WIN32

typedef void *HANDLE;
#else
#define HANDLE int
#endif

void txSleep(int mS);

unsigned int getTicks();

#ifdef WIN32
#define round(x) floorf(x + 0.5f);
#endif


#define Now getTicks()

// DebugLog Severity Levels 

#define LOGEMERGENCY 0 
#define LOGALERT 1
#define LOGCRIT 2 
#define LOGERROR 3 
#define LOGWARNING 4
#define LOGNOTICE 5
#define LOGINFO 6
#define LOGDEBUG 7

#include <time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifdef M_PI
#undef M_PI
#endif

#define M_PI       3.1415926f

#ifndef TEENSY
#ifndef WIN32
#define LINUX
#endif
#endif

#ifdef __ARM_ARCH
#ifndef TEENSY
#define ARMLINUX
#endif
#endif

#include "ecc.h"				// RS Constants

typedef int BOOL;
typedef unsigned char UCHAR;

#define VOID void

#define FALSE 0
#define TRUE 1

#define False 0
#define True 1

// TEENSY Interface board equates

#ifdef TEENSY
#ifdef PIBOARD
#define ISSLED LED0
#else
#define ISSLED LED1
#endif
#define IRSLED LED1
#define TRAFFICLED LED2
#else
#define ISSLED 1
#define IRSLED 2
#define TRAFFICLED 3
#define PKTLED 4
#endif

BOOL KeyPTT(BOOL State);

UCHAR FrameCode(char * strFrameName);
BOOL FrameInfo(UCHAR bytFrameType, int * blnOdd, int * intNumCar, char * strMod,
   int * intBaud, int * intDataLen, int * intRSLen, UCHAR * bytQualThres, char * strType);

void ClearDataToSend();
int EncodeFSKData(UCHAR bytFrameType, UCHAR * bytDataToSend, int Length, unsigned char * bytEncodedBytes);
int EncodePSKData(UCHAR bytFrameType, UCHAR * bytDataToSend, int Length, unsigned char * bytEncodedBytes);
int EncodeOFDMData(UCHAR bytFrameType, UCHAR * bytDataToSend, int Length, unsigned char * bytEncodedBytes);
int Encode4FSKIDFrame(char * Callsign, char * Square, unsigned char * bytreturn, UCHAR SessionID);
int EncodeDATAACK(int intQuality, UCHAR bytSessionID, UCHAR * bytreturn);
int EncodeDATANAK(int intQuality , UCHAR bytSessionID, UCHAR * bytreturn);
void Mod4FSKDataAndPlay(unsigned char * bytEncodedBytes, int Len, int intLeaderLen);
void ModPSKDataAndPlay(unsigned char * bytEncodedBytes, int Len, int intLeaderLen);
BOOL IsDataFrame(UCHAR intFrameType);
BOOL CheckValidCallsignSyntax(char * strTargetCallsign);
void StartCodec(char * strFault);
void StopCodec(char * strFault);
BOOL SendARQConnectRequest(char * strMycall, char * strTargetCall);
void AddDataToDataToSend(UCHAR * bytNewData, int Len);
BOOL StartFEC(UCHAR * bytData, int Len, char * strDataMode, int intRepeats, BOOL blnSendID);
void SendID(BOOL blnEnableCWID);
BOOL CheckGSSyntax(char * GS);
//void SetARDOPProtocolState(int value);
unsigned int GenCRC16(unsigned char * Data, unsigned short length);
void SendCommandToHost(char * Cmd);
void TCPSendCommandToHost(char * Cmd);
void SCSSendCommandToHost(char * Cmd);
void SendCommandToHostQuiet(char * Cmd);
void TCPSendCommandToHostQuiet(char * Cmd);
void SCSSendCommandToHostQuiet(char * Cmd);
void UpdateBusyDetector(short * bytNewSamples);
int UpdatePhaseConstellation(short * intPhases, short * intMags, int intPSKPhase, BOOL blnQAM, BOOL OFDM);
void SetARDOPProtocolState(int value);
BOOL BusyDetect3(float * dblMag, int intStart, int intStop);
void SendLogToHost(char * Msg, int len);
VOID Gearshift_2(int intAckNakValue, BOOL blnInit);

void displayState(const char * State);
void displayCall(int dirn, char * call);

void SampleSink(short Sample);
void SoundFlush();
void StopCapture();
void StartCapture();
void DiscardOldSamples();
void ClearAllMixedSamples();

void SetFilter(void * Filter());

void AddTrailer();
void CWID(char * strID, short * intSamples, BOOL blnPlay);
void sendCWID(char * Call, BOOL Play);
UCHAR ComputeTypeParity(UCHAR bytFrameType);
void GenCRC16FrameType(char * Data, int Length, UCHAR bytFrameType);
BOOL CheckCRC16FrameType(unsigned char * Data, int Length, UCHAR bytFrameType);
char * strlop(char * buf, char delim);
void QueueCommandToHost(char * Cmd);
void SCSQueueCommandToHost(char * Cmd);
void TCPQueueCommandToHost(char * Cmd);
void SendReplyToHost(char * strText);
void TCPSendReplyToHost(char * strText);
void SCSSendReplyToHost(char * strText);
void LogStats();
int GetNextFrameData(int * intUpDn, UCHAR * bytFrameTypeToSend, UCHAR * strMod, BOOL blnInitialize);
void SendData();
int ComputeInterFrameInterval(int intRequestedIntervalMS);
VOID EncodeAndSend4FSKControl(UCHAR bytFrameType, UCHAR bytSessionID, int LeaderLength);
VOID WriteExceptionLog(const char * format, ...);
void SaveQueueOnBreak();
VOID Statsprintf(const char * format, ...);
VOID CloseDebugLog();
VOID CloseStatsLog();
void Abort();
void SetLED(int LED, int State);
VOID ClearBusy();
VOID CloseCOMPort(HANDLE fd);
VOID COMClearRTS(HANDLE fd);
VOID COMClearDTR(HANDLE fd);

//#ifdef WIN32
void ProcessNewSamples(short * Samples, int nSamples);
VOID Debugprintf(const char * format, ...);
VOID WriteDebugLog(int LogLevel, const char * format, ...);
void ardopmain();
BOOL GetNextFECFrame();
void GenerateFSKTemplates();
void printtick(char * msg);
void InitValidFrameTypes();
//#endif

extern void Generate50BaudTwoToneLeaderTemplate();
extern BOOL blnDISCRepeating;

BOOL DemodDecode4FSKID(UCHAR bytFrameType, char * strCallID, char * strGridSquare);
void DeCompressCallsign(char * bytCallsign, char * returned);
void DeCompressGridSquare(char * bytGS, char * returned);

int RSEncode(UCHAR * bytToRS, UCHAR * RSBytes, int DataLen, int RSLen);
BOOL RSDecode(UCHAR * bytRcv, int Length, int CheckLen, BOOL * blnRSOK);

void ProcessRcvdFECDataFrame(int intFrameType, UCHAR * bytData, BOOL blnFrameDecodedOK);
void ProcessUnconnectedConReqFrame(int intFrameType, UCHAR * bytData);
void ProcessRcvdARQFrame(UCHAR intFrameType, UCHAR * bytData, int DataLen, BOOL blnFrameDecodedOK);
void InitializeConnection();

void AddTagToDataAndSendToHost(UCHAR * Msg, char * Type, int Len);
void TCPAddTagToDataAndSendToHost(UCHAR * Msg, char * Type, int Len);
void SCSAddTagToDataAndSendToHost(UCHAR * Msg, char * Type, int Len);

void RemoveDataFromQueue(int Len);
void RemodulateLastFrame();

void GetSemaphore();
void FreeSemaphore();
const char * Name(UCHAR bytID);
const char * shortName(UCHAR bytID);
void InitSound();
void initFilter(int Width, int centerFreq);
void FourierTransform(int NumSamples, short * RealIn, float * RealOut, float * ImagOut, int InverseTransform);
VOID ClosePacketSessions();
VOID LostHost();
VOID ProcessPacketHostBytes(UCHAR * RXBuffer, int Len);
int ReadCOMBlock(HANDLE fd, char * Block, int MaxLength);
VOID ProcessDEDModeFrame(UCHAR * rxbuffer, unsigned int Length);
BOOL CheckForPktMon();
BOOL CheckForPktData();
void ModOFDMDataAndPlay(unsigned char * bytEncodedBytes, int Len, int intLeaderLen);
void GetOFDMFrameInfo(int OFDMMode, int * intDataLen, int * intRSLen, int * Mode, int * Symbols);
void ClearOFDMVariables();
VOID EncodeAndSendOFDMACK(UCHAR bytSessionID, int LeaderLength);
int ProcessOFDMAck(int AckType);
void ProcessOFDMNak(int AckType);

int SendtoGUI(char Type, unsigned char * Msg, int Len);	
void DrawRXFrame(int State, const char * Frame);
void DrawTXFrame(const char * Frame);
void mySetPixel(unsigned char x, unsigned char y, unsigned int Colour);
void clearDisplay();
void DrawDecode(char * Decode);


extern int WaterfallActive;
extern int SpectrumActive;
extern unsigned int PKTLEDTimer;

extern char stcLastPingstrSender[10];
extern char stcLastPingstrTarget[10];
extern int stcLastPingintRcvdSN;
extern int stcLastPingintQuality;
extern time_t stcLastPingdttTimeReceived;

enum _ReceiveState		// used for initial receive testing...later put in correct protocol states
{
	SearchingForLeader,
	AcquireSymbolSync,
	AcquireFrameSync,
	AcquireFrameType,
	DecodeFrameType,
	AcquireFrame,
	DecodeFramestate
};

extern enum _ReceiveState State;

enum _ARQBandwidth
{
	XB200,
	XB500,
	XB2500,
	UNDEFINED
};

extern enum _ARQBandwidth ARQBandwidth;
extern const char ARQBandwidths[9][12];

enum _ARDOPState
{
	OFFLINE,
	DISC,
	ISS,
	IRS,
	IDLE,     // ISS in quiet state ...no transmissions)
	IRStoISS, // IRS during transition to ISS waiting for ISS's ACK from IRS's BREAK
 	FECSend,
	FECRcv
};

extern enum _ARDOPState ProtocolState;

extern const char ARDOPStates[8][9];



// Enum of ARQ Substates

enum _ARQSubStates
{
	None,
	ISSConReq,
	ISSConAck,
	ISSData,
	ISSId,
	IRSConAck,
	IRSData,
	IRSBreak,
	IRSfromISS,
	DISCArqEnd
};

extern enum _ARQSubStates ARQState;

enum _ProtocolMode
{
	Undef,
	FEC,
	ARQ
};

extern enum _ProtocolMode ProtocolMode;

extern const char ARDOPModes[3][6];

extern enum _ARQSubStates ARQState;

struct SEM
{
	unsigned int Flag;
	int Clashes;
	int	Gets;
	int Rels;
};

extern struct SEM Semaphore;

#define DataNAK		0x00
#define DataNAKLoQ	0x01
#define ConRejBusy	0x02
#define ConRejBW	0x03
#define ConAck		0x04
#define DISCFRAME	0x05
#define BREAK		0x06
#define END			0x07
#define IDLEFRAME	0x08
#define ConReq200	0x09
#define ConReq500	0x0A
#define ConReq2500	0x0B
#define IDFRAME		0x0C
#define PINGACK		0x0D
#define PING		0x0E
#define CQ_de		0x0F
	
	//	200 Hz Bandwidth 
	//	1 Car modes
	
#define D4PSK_200_50_E	0x10
#define D4PSK_200_50_O	0x11
#define D4PSK_200_100_E	0x12
#define D4PSK_200_100_O	0x13
#define D16QAM_200_100_E	0x14
#define D16QAM_200_100_O	0x15

	//	500 Hz bandwidth Data 
	//	1 Car 4FSK Data mode 500 Hz, 50 baud tones spaced @ 100 Hz 

#define D4FSK_500_50_E	0x1A
#define D4FSK_500_50_O	0x1B
#define D4PSK_500_50_E	0x1C
#define D4PSK_500_50_O	0x1D
#define D4PSK_500_100_E	0x1E
#define D4PSK_500_100_O	0x1F
	//	2 Car 16QAM Data Modes 100 baud
#define D16QAMR_500_100_E	0x20
#define D16QAMR_500_100_O	0x21
#define D16QAM_500_100_E	0x22
#define D16QAM_500_100_O	0x23

// OFDM modes

#define DOFDM_500_55_E		0x24
#define DOFDM_500_55_O		0x25

#define DOFDM_200_55_E		0x26
#define DOFDM_200_55_O		0x27

#define OConReq500	0x18
#define OConReq2500	0x19


	//	1 Khz Bandwidth Data Modes 
	//	2 Car 4FSK Data mode 1000 Hz, 50 baud tones spaced @ 100 Hz 
#define D4FSK_1000_50_E	0x28
#define D4FSK_1000_50_O	0x29

	//	2500 bandwidth modes
	//	10 Car PSK Data Modes 50 baud

#define D4PSKR_2500_50_E 0x2A
#define D4PSKR_2500_50_O 0x2B
#define D4PSK_2500_50_E	0x2C
#define D4PSK_2500_50_O	0x2D

	//	10 Car PSK Data Modes 100 baud

#define D4PSK_2500_100_E	0x2E
#define D4PSK_2500_100_O	0x2F

	//	10 Car 10 Car 16QAMRobust (duplicated carriers) 

#define D16QAMR_2500_100_E	0x30
#define D16QAMR_2500_100_O	0x31

	//	10 Car 16QAM Data modes 100 baud 

#define D16QAM_2500_100_E	0x32
#define D16QAM_2500_100_O	0x33

	// OFDM modes

#define DOFDM_2500_55_E	0x34
#define DOFDM_2500_55_O	0x35

#define PktFrameHeader 0x3A		// Variable length frame Header
#define PktFrameData 0x3B		// Variable length frame Data (Virtual Frsme Type)

#define OFDMACK	0x3D
#define DataACK	0x3E
#define DataACKHiQ 0x3F



extern CONST short int50BaudTwoToneLeaderTemplate[240];  // holds just 1 symbol (20 ms) of the leader
 
//The actual templates over 11 carriers for 16QAM in a 8-8 circular constellation.  First 4 symbols only 
// (only positive Phase values are in the table, sign reversal is used to get the negative phase values) This reduces the template size to 5280 integers
extern CONST short intQAM50bdCarTemplate[11][4][120]; 

extern CONST short intFSK50bdCarTemplate[12][240];		// Template for 4FSK carriers spaced at 50 Hz, 50 baud
extern CONST short intFSK100bdCarTemplate[4][120];

extern CONST short intOFDMTemplate[MAXCAR][8][216];

// Config Params
extern char GridSquare[9];
extern char Callsign[10];
extern BOOL wantCWID;
extern BOOL CWOnOff;
extern int LeaderLength;
extern int TrailerLength;
extern unsigned int ARQTimeout;
extern int TuningRange;
extern int TXLevel;
extern int RXLevel;
extern int autoRXLevel;
extern BOOL DebugLog;
extern int ARQConReqRepeats;
extern BOOL CommandTrace;
extern char strFECMode[];
extern char CaptureDevice[];
extern char PlaybackDevice[];
extern int port;
extern char HostPort[80];
extern int pktport;
extern BOOL RadioControl;
extern BOOL SlowCPU;
extern BOOL AccumulateStats;
extern BOOL Use600Modes;
extern BOOL UseOFDM;
extern BOOL EnableOFDM;
extern BOOL FSKOnly;
extern BOOL fastStart;
extern BOOL ConsoleLogLevel;
extern BOOL FileLogLevel;
extern BOOL EnablePingAck;
extern BOOL NegotiateBW;

extern int dttLastPINGSent;

extern BOOL blnPINGrepeating;
extern BOOL blnFramePending;
extern int intPINGRepeats;

extern BOOL gotGPIO;
extern BOOL useGPIO;

extern int pttGPIOPin;

extern HANDLE hCATDevice;		// port for Rig Control
extern char CATPort[80];
extern int CATBAUD;
extern int EnableHostCATRX;

extern HANDLE hPTTDevice;			// port for PTT
extern char PTTPort[80];			// Port for Hardware PTT - may be same as control port.
extern int PTTBAUD;

#define PTTRTS		1
#define PTTDTR		2
#define PTTCI_V		4

extern UCHAR PTTOnCmd[];
extern UCHAR PTTOnCmdLen;

extern UCHAR PTTOffCmd[];
extern UCHAR PTTOffCmdLen;

extern int PTTMode;				// PTT Control Flags.




extern char * CaptureDevices;
extern char * PlaybackDevices;

extern int dttCodecStarted;
extern int dttStartRTMeasure;

extern int intCalcLeader;        // the computed leader to use based on the reported Leader Length

extern const char strFrameType[64][18];
extern const char shortFrameType[64][12];
extern BOOL Capturing;
extern BOOL SoundIsPlaying;
extern int blnLastPTT;
extern BOOL blnAbort;
extern BOOL blnClosing;
extern BOOL blnCodecStarted;
extern BOOL blnInitializing;
extern BOOL blnARQDisconnect;
extern int DriveLevel;
extern int FECRepeats;
extern BOOL FECId;
extern int Squelch;
extern int BusyDet;
extern BOOL blnEnbARQRpt;
extern unsigned int dttNextPlay;

extern UCHAR bytDataToSend[];
extern int bytDataToSendLength;

extern BOOL blnListen;
extern BOOL Monitor;
extern BOOL AutoBreak;
extern BOOL BusyBlock;

extern int DecodeCompleteTime;

extern BOOL AccumulateStats;

extern unsigned char bytEncodedBytes[4500];
extern int EncLen;

extern char AuxCalls[10][10];
extern int AuxCallsLength;

extern int bytValidFrameTypesLength;
extern int bytValidFrameTypesLengthALL;
extern int bytValidFrameTypesLengthISS;

extern BOOL blnTimeoutTriggered;
extern int intFrameRepeatInterval;
extern int extraDelay;
extern BOOL PlayComplete;

extern const UCHAR bytValidFrameTypesALL[];
extern const UCHAR bytValidFrameTypesISS[];
extern const UCHAR * bytValidFrameTypes;

extern const char strAllDataModes[][16];
extern int strAllDataModesLen;

extern const short Rate[64];		// Data Rate (in bits/sec) by Frame Type


extern BOOL newStatus;

// RS Variables

extern int MaxCorrections;

// Stats counters

extern int SessBytesSent;
extern int SessBytesReceived;
extern int intLeaderDetects;
extern int intLeaderSyncs;
extern int intAccumLeaderTracking;
extern float dblFSKTuningSNAvg;
extern int intGoodFSKFrameTypes;
extern int intFailedFSKFrameTypes;
extern int intAccumFSKTracking;
extern int intFSKSymbolCnt;
extern int intGoodFSKFrameDataDecodes;
extern int intFailedFSKFrameDataDecodes;
extern int intAvgFSKQuality;
extern int intFrameSyncs;
extern int intGoodPSKSummationDecodes;
extern int intGoodFSKSummationDecodes;
extern int intGoodOFDMSummationDecodes;
extern float dblLeaderSNAvg;
extern int intAccumPSKLeaderTracking;
extern float dblAvgPSKRefErr;
extern int intPSKTrackAttempts;
extern int intAccumPSKTracking;
extern int intQAMTrackAttempts;
extern int intAccumQAMTracking;
extern int intOFDMTrackAttempts;
extern int intAccumOFDMTracking;
extern int intPSKSymbolCnt;
extern int intQAMSymbolCnt;
extern int intOFDMSymbolCnt;
extern int intGoodPSKFrameDataDecodes;
extern int intFailedPSKFrameDataDecodes;
extern int intAvgPSKQuality;
extern int intGoodOFDMFrameDataDecodes;
extern int intFailedOFDMFrameDataDecodes;
extern int intAvgOFDMQuality;
extern float dblAvgDecodeDistance;
extern int intDecodeDistanceCount;
extern int intShiftUPs;
extern int intShiftDNs;
extern unsigned int dttStartSession;
extern int intLinkTurnovers;
extern int intEnvelopeCors;
extern float dblAvgCorMaxToMaxProduct;
extern int intConReqSN;
extern int intConReqQuality;



extern int int4FSKQuality;
extern int int4FSKQualityCnts;
extern int int8FSKQuality;
extern int int8FSKQualityCnts;
extern int int16FSKQuality;
extern int int16FSKQualityCnts;
extern int intFSKSymbolsDecoded;
extern int intPSKQuality[2];
extern int intPSKQualityCnts[2];
extern int intPSKSymbolsDecoded; 

extern int intOFDMQuality[8];
extern int intOFDMQualityCnts[8];
extern int intOFDMSymbolsDecoded; 

extern int intQAMQuality;
extern int intQAMQualityCnts;
extern int intQAMSymbolsDecoded;
extern int intQAMSymbolCnt;
extern int intOFDMSymbolCnt;
extern int intGoodQAMFrameDataDecodes;
extern int intFailedQAMFrameDataDecodes;
extern int intGoodQAMSummationDecodes;

extern int dttLastBusyOn;
extern int dttLastBusyOff;
extern int dttLastLeaderDetect;

extern int LastBusyOn;
extern int LastBusyOff;
extern int dttLastLeaderDetect;

extern int pktDataLen;
extern int pktRSLen;
extern const char pktMod[16][12];
extern int pktMode;
extern int pktModeLen;
extern const int pktBW[16];
extern const int pktCarriers[16];
extern const int defaultPacLen[16];
extern const BOOL pktFSK[16];

extern int pktMaxFrame;
extern int pktMaxBandwidth;
extern int pktPacLen;
extern int initMode;		 // 0 - 4PSK 1 - 8PSK 2 = 16QAM

extern UCHAR UnackedOFDMBlocks[128];
extern int NextOFDMBlock;



extern BOOL SerialMode;			// Set if using SCS Mode, Unset ofr TCP Mode

// Has to follow enum defs

BOOL EncodeARQConRequest(char * strMyCallsign, char * strTargetCallsign, enum _ARQBandwidth ARQBandwidth, UCHAR * bytReturn);


// OFDM Modes

#define PSK2	0
#define PSK4	1
#define PSK8	2
#define QAM16	3
#define PSK16	4			// Experimental - is it better than 16QAM?
#define QAM32	5
#define PSK4S	6			// Special shorter frame for short messages

extern int OFDMMode;			// OFDM can use various modulation modes and redundancy levels
extern int LastSentOFDMMode;	// For retries
extern int LastSentOFDMType;	// For retries

extern int SavedOFDMMode;		// used if we switch to a more robust mode cos we don't have much to send
extern int SavedFrameType;


extern const char OFDMModes[8][6];

#endif