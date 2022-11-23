//	ARDOP TNC Host Serial Interface

//	Based on SCS Extended Hodtmode, as used in 
//	SCS PTC and Dragon controllers.

//	Operates in two modes, SCS Emulation and ARDOP Native.
//	SCS Emulation allows programs writter for the SCS 
//	controllers to use ARDOP
//	ARDOP Native supports full ARDOP command set

//	Native Mode uses Stream 32 for commands, 33 for Data 
//	and 34 for Monitor/Logging.


#include "ARDOPC.h"

#define LOGTOHOST

#ifndef WIN32

#define strtok_s strtok_r
#define _strupr strupr
int _memicmp(unsigned char *a, unsigned char *b, int n);
char * strupr(char* s);

#endif

VOID ProcessSCSPacket(UCHAR * rxbuffer, unsigned int Length);
VOID EmCRCStuffAndSend(UCHAR * Msg, int Len);
void ProcessRIGPacket(int Command, char * Buffer, int Len);
VOID PutString(UCHAR * Msg);
int PutChar(UCHAR c);
int SerialSendData(UCHAR * Message,int MsgLen);
void CatWrite(char * Buffer, int Len);
int RadioPoll();
void ProcessCommandFromHost(char * strCMD);
BOOL GetNextARQFrame();
VOID ProcessKISSBytes(UCHAR * RXBUFFER, int Read);
BOOL CheckKISS(UCHAR * SCSReply);
UCHAR * PacketSessionPoll(UCHAR * NextChan);
BOOL CheckForPktData(int Channel);
VOID ProcessPktData(int Channel, UCHAR * Buffer, int Len);
BOOL ProcessPktCommand(int Channel, char *Buffer, int Len);

#ifdef LOGTOHOST

// Log output sent to host instead of File

#define LOGBUFFERSIZE 2048

char LogToHostBuffer[LOGBUFFERSIZE];
int LogToHostBufferLen = 0;

#endif

int bytDataToSendLength = 0;

extern UCHAR bytDataToSend[];			// A bit more than OFDM window (10160)

// Outbound data buffer 

char ReportCall[10];

UCHAR bytDataforHost[127 * 80];		// has to be at least OFDM Window size
int bytesforHost = 0;

UCHAR bytEchoData[127 * 80];		// has to be at least OFDM Window size
int bytesEchoed = 0;

UCHAR DelayedEcho = 0;

UCHAR SCSReply[256 + 10];		// Host Mode reply buffer
int Toggle;

char CommandToHostBuffer[512];	// Async Commands to Host (BUFFER etc)
int CommandToHostBufferLen;

char CMDReplyBuffer[256];		// Used by Command Handler. Null Terminated


extern char Callsign[10];
extern BOOL blnBusyStatus;

extern int intNumCar;
extern int intBaud;
extern int intDataLen;
extern int intRSLen;
extern int intSampleLen;
extern int intDataPtr;
extern int intSampPerSym;
extern int intDataBytesPerCar;

unsigned char CatRXbuffer[256];
int CatRXLen = 0;

BOOL Term4Mode;
BOOL DEDMode = 0;		// Used by RMS Express for Packet
BOOL PACMode = 0;

BOOL HostMode = 0;		// Host or Term

BOOL PTCMode = FALSE;	// Running in PTC compatibility mode?

volatile int RXBPtrSCS;

int change = 0;			// Flag for connect/disconnect reports
int SCSState = 0;

int DataChannel = 31;
int ReplyLen;

extern float dblOffsetHz;
extern int intSessionBW;

extern int SerialWatchDog;

void SCSSendCommandToHost(char * Cmd)
{
	if (HostMode & !PTCMode)	// ARDOP Native
	{
		char * ptr = &CommandToHostBuffer[CommandToHostBufferLen];
		int len = strlen(Cmd);

		WriteDebugLog(LOGDEBUG, "Command to Host %s", Cmd);

		if (CommandToHostBufferLen + len > 500)
			return;			// ignore if full

		// Add headers and queue for host

		strcpy(ptr, Cmd);
		ptr += len;
		*ptr++ = '\r';

		CommandToHostBufferLen += (len + 1);

		if (CommandToHostBufferLen > 512)
			CommandToHostBufferLen = 0;

		return;
	}

	if (memcmp(Cmd, "STATUS ", 7) == 0)
	{
		if (memcmp(&Cmd[7], "CONNECT TO", 10) == 0)
		{
			if (HostMode)
			{
				memcpy(ReportCall, &Cmd[18], 10);
				strlop(ReportCall, ' ');
				change = 1;
				SCSState = 0;
			}
			else
				PutString("Disconnected\r");
		}
	}
	if (memcmp(Cmd, "CONNECTED ", 10) == 0)
	{
		if (HostMode)
		{
			memcpy(ReportCall, &Cmd[10], 10);
			strlop(ReportCall, ' ');
			change = 1;
			SCSState = 1;
		}
		else
		{
			PutString(Cmd);
			PutString("\r");
		}
		}

	if (memcmp(Cmd, "DISCON", 6) == 0)
	{
		if (HostMode)
		{
			change = 1;
			SCSState = 0;
		}
		else
			PutString("Disconnected\r");
	}
	WriteDebugLog(LOGDEBUG, "Command to Host %s", Cmd);
}


void SCSQueueCommandToHost(char * Cmd)
{
	SendCommandToHost(Cmd);		// no queuing now
}


void SCSSendReplyToHost(char * Cmd)
{
	// Used by command handler

	if (HostMode)
		if (PTCMode)
			return;				// shouldnt have commands jn PTC Mode
		else
			strcpy(CMDReplyBuffer, Cmd);

	else
		PutString(Cmd);
}

void SCSSendCommandToHostQuiet(char * Cmd)		// Higher Debug Level for PTT
{
	// if possible convert to equivalent PTC message

	if (memcmp(Cmd, "STATUS CONNECT TO", 20) == 0)
	{
		change = 1;
		SCSState = 0;
	}

	WriteDebugLog(LOGDEBUG, "Command to Host %s", Cmd);
}


void SCSAddTagToDataAndSendToHost(UCHAR * Msg, char * Type, int Len)
{
	if (!HostMode)
	{
		Msg[Len] = 0;
		PutString(Msg);
		return;
	}

	if (PTCMode)
	{
		// only pass ARQ Data to Host

		if (strcmp(Type, "ARQ") == 0)
		{
			memcpy(&bytDataforHost[bytesforHost], Msg, Len);
			bytesforHost += Len;
		}

		return;
	}

	// In ARDOP Native add header (Type and Len)

	Len += 3;		// Tag

	bytDataforHost[bytesforHost++] = Len >> 8;		//' MS byte of count  (Includes strDataType but does not include the two trailing CRC bytes)
	bytDataforHost[bytesforHost++] = Len  & 0xFF;	// LS Byte
	memcpy(&bytDataforHost[bytesforHost], Type, 3);
	memcpy(&bytDataforHost[bytesforHost + 3], Msg, Len - 3);
	bytesforHost += Len;
}

BOOL CheckStatusChange()
{
	if (change == 1)
	{
		change = 0;

		if (SCSState == 1)
		{
			// Connected

			SCSReply[2] = DataChannel;
			SCSReply[3] = 3;
			ReplyLen  = sprintf(&SCSReply[4], "(%d) CONNECTED to %s", DataChannel, ReportCall);
			ReplyLen += 5;
			EmCRCStuffAndSend(SCSReply, ReplyLen);

			return TRUE;
		}
		// Disconnected
		
		SCSReply[2] = DataChannel;
		SCSReply[3] = 3;
		ReplyLen  = sprintf(&SCSReply[4], "(%d) DISCONNECTED fm %s", DataChannel, ReportCall);
		ReplyLen += 5;		// Include Null
		EmCRCStuffAndSend(SCSReply, ReplyLen);

		return TRUE;
	}

	return FALSE;

}


BOOL CheckForControl()
{
	int Length;

	if (CommandToHostBufferLen == 0)
		return FALSE;

	if (CommandToHostBufferLen > 256)
		Length = 256;
	else
		Length = CommandToHostBufferLen;

	memcpy(&SCSReply[5], CommandToHostBuffer, Length);

	CommandToHostBufferLen -= Length;

	if (CommandToHostBufferLen)
		memmove(CommandToHostBuffer, &CommandToHostBuffer[Length], CommandToHostBufferLen);

	SCSReply[2] = 32;
	SCSReply[3] = 7;
	SCSReply[4] = Length - 1;

	ReplyLen = Length + 5;
	EmCRCStuffAndSend(SCSReply, ReplyLen);

	return TRUE;
}

BOOL CheckForData()
{
	int Length;

	if (bytesEchoed)
	{
		// Echo back acked data

		if (bytesEchoed > 256)
			Length = 256;
		else
			Length = bytesEchoed;

		memcpy(&SCSReply[5], bytEchoData, Length);

		bytesEchoed -= Length;

		if (bytesEchoed)
			memmove(bytEchoData, &bytEchoData[Length], bytesEchoed);

		SCSReply[2] = DataChannel;
		SCSReply[3] = 8;		// PTC Special - delayed echoed data
		SCSReply[4] = Length - 1;

		ReplyLen = Length + 5;
		EmCRCStuffAndSend(SCSReply, ReplyLen);

		return TRUE;
	}

	if (bytesforHost == 0)
		return FALSE;

	if (bytesforHost > 256)
		Length = 256;
	else
		Length = bytesforHost;

	memcpy(&SCSReply[5], bytDataforHost, Length);

	bytesforHost -= Length;

	if (bytesforHost)
		memmove(bytDataforHost, &bytDataforHost[Length], bytesforHost);

	if (PTCMode)
		SCSReply[2] = DataChannel;
	else
		SCSReply[2] = 33;

	SCSReply[3] = 7;
	SCSReply[4] = Length - 1;

	ReplyLen = Length + 5;
	EmCRCStuffAndSend(SCSReply, ReplyLen);

	return TRUE;
}


#ifdef LOGTOHOST

BOOL CheckForLog()
{
	int Length;
	char * ptr;

	if (LogToHostBufferLen == 0)
		return FALSE;

	// Return first message from buffer

	// Terminated with LF, but beware of LF in timestamp

	ptr = memchr(&LogToHostBuffer[4], 10, LogToHostBufferLen -4);

	if (ptr == NULL)		// shouldn't happen!
	{
		LogToHostBufferLen = 0;
		return FALSE;
	}

	Length = 1 + ptr - &LogToHostBuffer[0];

	if (Length > 256)
		Length = 256;

	memcpy(&SCSReply[5], LogToHostBuffer, Length);
	LogToHostBufferLen -= Length;

	if (LogToHostBufferLen)
		memmove(LogToHostBuffer, &LogToHostBuffer[Length], LogToHostBufferLen);
	
	SCSReply[2] = 248;
	SCSReply[3] = 7;
	SCSReply[4] = Length - 1;

	ReplyLen = Length + 5;
	EmCRCStuffAndSend(SCSReply, ReplyLen);

	return TRUE;
}

#endif


// SCS Emulator

extern unsigned short CRCTAB[256];

unsigned short int xcompute_crc(unsigned char *buf,int len)
{
	unsigned short fcs = 0xffff; 
	int i;

	for(i = 0; i < len; i++) 
		fcs = (fcs >>8 ) ^ CRCTAB[(fcs ^ buf[i]) & 0xff]; 

	return fcs;
}

// SCS Mode Stuff

unsigned short int compute_crc(unsigned char *buf,int len);
VOID EmCRCStuffAndSend(UCHAR * Msg, int Len);

int EmUnstuff(UCHAR * MsgIn, UCHAR * MsgOut, int len)
{
	int i, j=0;

	for (i=0; i<len; i++, j++)
	{
		MsgOut[j] = MsgIn[i];
		if (MsgIn[i] == 170)			// Remove next byte
		{
			i++;
			if (MsgIn[i] != 0)
				if (i != len) return -1;
		}
	}

	return j;
}

UCHAR PacketMon[360];

int PacketMonMore = 0;
int PacketMonLength = 0;

VOID ProcessSCSHostFrame(UCHAR *  Buffer, int Length)
{
	int Channel = Buffer[0];
	int Command = Buffer[1] & 0x3f;
	int Len = Buffer[2];
	int len;
	int RXToggle = Buffer[1] & 0x80;

//	WriteDebugLog(LOGINFO, "SCS Host Frame Toggle %x RXToggle %x Chan %d Cmd %d Len %d",
//		Toggle, RXToggle, Channel, Command, Len);

	if (Toggle == RXToggle && (Buffer[1] & 0x40) == 0)
	{
		// Repeat Condition

		EmCRCStuffAndSend(SCSReply, ReplyLen);
		WriteDebugLog(LOGINFO, "Same Toggle - Repeating last packet");

		return;
	}

	Toggle = RXToggle;

	if (Channel == 255)
	{
		UCHAR * NextChan = &SCSReply[4];
		
		// General Poll

		// if Teensy, kick link watchdog

#ifdef TEENSY
		SerialWatchDog = 0;
#endif
		// See if any channels have anything available

		// Although spec say Dragon only sends log data in response
		// to poll of chan 248 it seems to send in response to gen poll


		// Give priority to packet monitor data, as
		// we only have one buffer

		if (PacketMonLength)
		{
			// If length is less than 257, return whole 
			// lot in a Type 4 message

			// if not, first 256 as a type 5 Message
			// Next poll return rest in type 6

			int Length = PacketMonLength;
			
			if (Length > 256)
				Length = 256;
	
			if (PacketMonMore)
				memcpy(&SCSReply[5], &PacketMon[256], Length);
			else
				memcpy(&SCSReply[5], PacketMon, Length);
				
			SCSReply[2] = 0;		// Channel 0

			if (PacketMonLength > 256)
			{
				PacketMonMore = 1;
				PacketMonLength -= 256;
				SCSReply[3] = 5;
			}
			else
			{
				if (PacketMonMore)
					SCSReply[3] = 6;
				else
					SCSReply[3] = 4;

				PacketMonLength = PacketMonMore = 0;
			}

			SCSReply[4] = Length - 1;

			EmCRCStuffAndSend(SCSReply, Length + 5);
			return;
		}

		// Send KISS data in response to gen poll

		if (CheckKISS(SCSReply))	// only used in Native mode
		{
			// got a message

			return;
		}


		if (newStatus)
		{
			newStatus = FALSE;
			*(NextChan++) = 255;
		}

		if (CatRXLen == 0)
		{
#ifndef TEENSY
			if (hCATDevice)
			{
				CatRXLen = ReadCOMBlock(hCATDevice, CatRXbuffer, 256);
			}
#endif
		}

		if (CatRXLen)		// Cat data available?
		{
			*(NextChan++) = 254;	// 253 + 1
		}

		if (CommandToHostBufferLen)	// only used in Native mode
			*(NextChan++) = 33;		// Native mode cmd channel

		if (bytesforHost || bytesEchoed || change)
			if (PTCMode)
				*(NextChan++) = DataChannel; // Something for this channel
			else
				*(NextChan++) = 34;		// Native mode data channel

		NextChan = PacketSessionPoll(NextChan);	// See if anythinkg from packet Sessions

#ifdef LOGTOHOST
//		if (LogToHostBufferLen)	// only used in Native mode
//			*(NextChan++) = 35;		// Native mode log channel
#endif

		*(NextChan++) = 0;

		SCSReply[2] = 255;
		SCSReply[3] = 1;

		ReplyLen = NextChan - &SCSReply[0];

#ifdef LOGTOHOST

		if (ReplyLen == 5)		// nothing doing
		{
			// if we have log data send it

			if (LogToHostBufferLen)	// only used in Native mode
			{
				//	Send next message
				if (CheckForLog())
				{
					// got a message

					return;
				}
			}
		}
#endif
		EmCRCStuffAndSend(SCSReply, ReplyLen);
		return;
	}

	if (Channel == 254)			// Status
	{
		// Extended Status Poll

		SCSReply[2] = 254;
		SCSReply[3] = 7;		// Status
		SCSReply[4] = 3;		// Len -1

//		if (ProtocolState == IDLE || ProtocolState == IRS || ProtocolState == ISS)
		if (ProtocolState != DISC)
		{
			// connected states

			SCSReply[5] = 0xa2;		// ARQ, Traffic

			if (ProtocolState != IRS)
				SCSReply[5] |= 0x8;	// Send Bit

			if (intSessionBW == 200)
				SCSReply[6] = 1;		// P1
			else if (intSessionBW == 500)
				SCSReply[6] = 2;		// P2
			else if (intSessionBW == 1000)
				SCSReply[6] = 3;		// P2
			else
				SCSReply[6] = 4;		// P4


			// Speedlevel Mapping from RMS Express

			// P1 {100, 200}, _
            // P2 {200, 400, 600, 800}, _
            // P3 {200, 600, 1400, 2800, 3200, 3600}, _
            // P4 {47, 85, 150, 300, 450, 1100, 2200, 3300, 4400, 5500}}

			if (intSessionBW == 200)
				SCSReply[7] = 1;
			else if (intSessionBW == 500)
				SCSReply[7] = 2;		// P2
			else if (intSessionBW == 1000)
				SCSReply[7] = 3;		// P2
			else
				SCSReply[7] = 6;		// P4

			SCSReply[8] = dblOffsetHz;	// Freq Error
		}
		else
		{
			if (blnBusyStatus)
				SCSReply[5] = 0xf0;
			else
				SCSReply[5] = 0;

			SCSReply[6] = 0;
			SCSReply[7] = 0;
			SCSReply[8] = 0;
		}

		ReplyLen = 9;
		EmCRCStuffAndSend(SCSReply, 9);
		return;
	}

	if (Channel == 253)			// Rig Control
	{
		if (Command == 1 && Buffer[3] == 'G')	
		{
			// Poll for Rig Data

			memcpy(&SCSReply[5], CatRXbuffer, CatRXLen);
			SCSReply[2] = Channel;
			SCSReply[3] = 7;
			SCSReply[4] = CatRXLen - 1;

			ReplyLen = CatRXLen + 5;
			EmCRCStuffAndSend(SCSReply, ReplyLen);
			CatRXLen = 0;
			return;
		}

		Len = Buffer[2] + 1;
		Buffer[Len + 3] = 0;

		// RMS Express sends #TRX: Commands, eg #TRX TY I 9600 $00 TTL
		// BPQ sends Data Frames with the raw contents

		ProcessRIGPacket(Command, &Buffer[3], Len);
		goto AckIt;
	}

	if (Channel == 250)			// KISS Interface
	{
		if (Command == 1 && Buffer[3] == 'G')	
		{
			// Poll for KISS Data

			memcpy(&SCSReply[5], CatRXbuffer, CatRXLen);
			SCSReply[2] = Channel;
			SCSReply[3] = 7;
			SCSReply[4] = CatRXLen - 1;

			ReplyLen = CatRXLen + 5;
			EmCRCStuffAndSend(SCSReply, ReplyLen);
			CatRXLen = 0;
			return;
		}

		Len = Buffer[2] + 1;
		Buffer[Len + 3] = 0;

		ProcessKISSBytes(&Buffer[3], Len);

		goto AckIt;
	}



	if (Command == 0)
	{
		// Data Frame

//		WriteDebugLog(LOGDEBUG, "Data Frame Channel %d", Channel);

		if (Channel < 11)			// Packet Data
		{
			ProcessPktData(Channel, &Buffer[3], Buffer[2] + 1);
			return;
		}

		else if (Channel == 31)			// PTC Mode Data
			AddDataToDataToSend(&Buffer[3], Buffer[2] + 1);

		else if (Channel == 32)		// Native Mode Command
		{
			Buffer[Len + 3] = 0;
			CMDReplyBuffer[0] = 0;	// in case no reply

			ProcessCommandFromHost(&Buffer[3]); // No C:

			// Command will have put reply in CMDReplyBuffer

			if (CMDReplyBuffer[0] == 0) // no response
				goto AckIt;
			
			SCSReply[2] = Channel;
			SCSReply[3] = 1;
			strcpy(&SCSReply[4], CMDReplyBuffer);
			ReplyLen = strlen(CMDReplyBuffer) + 7;
			EmCRCStuffAndSend(SCSReply, ReplyLen);
			WriteDebugLog(LOGDEBUG, "Sending CMD Reply %s", CMDReplyBuffer);
			return;
		}

		else if (Channel == 33)		// Native Mode Data

			AddDataToDataToSend(&Buffer[5], Buffer[2] - 1);
		
		goto AckIt;
	}

	// Command Frame

	if (Channel < 11 && Buffer[3] != 'G')
	{
		// Packet Channel Command

		ProcessPktCommand(Channel, &Buffer[3], Len);
		return;
	}

	switch (Buffer[3])
	{
	case 'J':				// JHOST

		HostMode = FALSE;
		WriteDebugLog(LOGINFO, "Exit Host Mode");
		return;

	case 'L':
/*
                            ' Status responses from the 'L' command will have 6 fields separated by spaces.
                            ' The field definitions are as follows:
                            ' Field 1: Number of link status messages not yet displayed
                            ' Field 2: NUmber of receive frames not yet displayed
                            ' Field 3: NUmber of send frames not yet delivered
                            ' Field 4: NUmber of transmitted frames not yet acknowledged
                            ' Field 5: NUmber of tries on the current operation
                            ' Field 6: Link state:
                            '   0 = Disconnected
                            '   1 = Link Setop
                            '   2 = Frame Reject
                            '   3 = Disconnect Request
                            '   4 = Information transfer
                            '   5 = Reject Frame Sent
                            '   6 = Waiting Acknowledgement
                            '   7 = Device Busy
                            '   8 = Remote Device Busy
                            '   9 = Both Devices Busy
                            '   10 = Waiting Acknowledgement and Device Busy
                            '   11 = Waiting Acknowledgement and Remote Busy
                            '   12 = Waiting Acknowledgement and Both Device Busy
                            '   13 = Reject Frame Sent and Device Busy
                            '   14 = Reject Frame Sent and Remote Busy
                            '   15 = Reject Frame Sent and Both Device Busy
                            '
							*/

			// I think only 3 is important as it is used for flow control
			// RMS Express stops at 20

		SCSReply[2] = Channel;
		SCSReply[3] = 1;
		len = sprintf(&SCSReply[4], "%d %d %d %d %d %d", 0, 0, (bytDataToSendLength > (DATABUFFERSIZE - 1000))?51:0, 0, 0, 4);
		ReplyLen = len + 5;
		EmCRCStuffAndSend(SCSReply, len + 5);
		return;


		return;

	case 'G':				// Specific Poll
	
		if (Channel > 0 && Channel < 11)
			if (CheckForPktData(Channel))
				return;				// It has sent reply
	
		if (PTCMode)
		{
			if (CheckStatusChange())
				return;					// It has sent reply

			if (CheckForData())
				return;					// It has sent reply
		}
		else
		{
			// Native mode

			if (Channel == 32)
				if (CheckForControl())
					return;				// It has sent reply

			if (Channel == 33)
				if (CheckForData())
					return;				// It has sent reply

#ifdef LOGTOHOST
			if (Channel == 34)
				if (CheckForLog())
					return;				// It has sent reply
#endif
		}

		// reply nothing doing

		SCSReply[2] = Channel;
		SCSReply[3] = 0;
		ReplyLen = 4;
		EmCRCStuffAndSend(SCSReply, 4);
		return;

	case 'C':				// Connect

		// Could be real, or just C to request status

		if (Channel == 0)
			goto AckIt;

		if (Length == 0)
		{
			// STATUS REQUEST - IF CONNECTED, GET CALL

			return;
		}
		Buffer[Length - 2] = 0;

		if (Buffer[5] == '%' || Buffer[5] == '!' ||Buffer[5] == ';' )
			Buffer++;		// Long Path / Robust

		SendARQConnectRequest(Callsign, &Buffer[5]);

	AckIt:

		SCSReply[2] = Channel;
		SCSReply[3] = 0;
		ReplyLen = 4;
		EmCRCStuffAndSend(SCSReply, 4);
		return;

	case 'D':

		// Disconnect

		if (ProtocolState == IDLE || ProtocolState == IRS || ProtocolState == ISS)
			blnARQDisconnect = TRUE;

		goto AckIt;
		
	case '%':

		// %X commands

		Buffer[Length - 2] = 0;
		WriteDebugLog(LOGDEBUG, "SCS Host Command %s", &Buffer[3]);

		switch (Buffer[4])
		{
		case 'V':					// Version

			SCSReply[2] = Channel;
			SCSReply[3] = 1;
			strcpy(&SCSReply[4], "4.8 1.32");
			ReplyLen = 13;
			EmCRCStuffAndSend(SCSReply, 13);

			return;


		case 'M':

			DelayedEcho = Buffer[5];
			
			SCSReply[2] = Channel;
			SCSReply[3] = 1;
			SCSReply[4] = 0;

			ReplyLen = 5;
			EmCRCStuffAndSend(SCSReply, 5);

			return;

		default:
						
			SCSReply[2] = Channel;
			SCSReply[3] = 1;
			SCSReply[4] = 0;

			ReplyLen = 5;
			EmCRCStuffAndSend(SCSReply, 5);

			return;
		}
	
	case '@':

		// Pobably just @B

		SCSReply[2] = Channel;
		SCSReply[3] = 1;
		len = sprintf(&SCSReply[4], "%d ", DATABUFFERSIZE - bytDataToSendLength);
		ReplyLen = len + 5;
		EmCRCStuffAndSend(SCSReply, len + 5);
		return;

	case '#':

		// Use # Construct to send ARDOP commands

		Buffer[Length - 2] = 0;
		WriteDebugLog(LOGDEBUG, "SCS Host Command %s", &Buffer[4]);

		ProcessCommandFromHost(&Buffer[4]);
		SCSReply[2] = Channel;
		SCSReply[3] = 1;
		SCSReply[4] = 0;

		ReplyLen = 5;
		EmCRCStuffAndSend(SCSReply, 5);

		return;

	default:
						
		SCSReply[2] = Channel;
		SCSReply[3] = 1;
		SCSReply[4] = 0;

		ReplyLen = 5;
		EmCRCStuffAndSend(SCSReply, 5);
	}
}

VOID ProcessSCSTextCommand(char * Command, int Len)
{
	// Command to SCS in non-Host mode.

	// if connected, queue for transmit

	if (ProtocolState == DISC
		|| (ProtocolState == ISS && ARQState == ISSConReq))
	{
		// Command Mode
	}
	else
	{
//		AddDataToDataToSend(Command, Len);
//		return;
	}

	Command[Len - 1] = 0;		// Remove 

	if (Command[0] == -64 && Command[1] == -1 && Command[2] == -64)
	{
		// Exit KISS

		WriteDebugLog(LOGDEBUG, "Exit KISS Command");
		goto SendPrompt;	

	}
	WriteDebugLog(LOGDEBUG, "SCS Command %s", Command);

	if (Len == 1)
		goto SendPrompt;		// Just a CR


	if (_memicmp(Command, "CB ", 3) == 0)
	{
		char SerialNo[] = "\r\nPRMS Express sends next without waiting";
	//	PutString(SerialNo);
		return;
	}

	if (_memicmp(Command, "JHOST4", 6) == 0)
	{
		HostMode = TRUE;
		DEDMode = FALSE;
		WriteDebugLog(LOGINFO, "Entering Host Mode");
		Toggle = 0;
		blnAbort = True;

		if (ProtocolState == IDLE || ProtocolState == IRS || ProtocolState == IRStoISS)
			GetNextARQFrame();
	
		return;
	}

	if (_memicmp(Command, "JHOST1", 6) == 0)
	{
		HostMode = TRUE;
		DEDMode = TRUE;
		WriteDebugLog(LOGINFO, "Entering DED Host Mode");
		blnAbort = True;

		goto SendPrompt;
//		return;
	}

	if (_memicmp(Command, "RESTART", 7) == 0)
	{
		PutString("\r\nOK");
		PTCMode = TRUE;
		blnListen = TRUE;
		WriteDebugLog(LOGINFO, "PTC Emulation Mode");
	}

	else if (_memicmp(Command, "ARDOP", 5) == 0)
	{
		PutString("\r\nOK");
		PTCMode = FALSE;
		blnListen = TRUE;
		ConsoleLogLevel = 6;
		WriteDebugLog(LOGINFO, "ARDOP Native Mode");
	}

	else if (_memicmp(Command, "TERM 4", 6) == 0)
		Term4Mode = TRUE;

	else if (_memicmp(Command, "T 0", 3) == 0)
		Term4Mode = FALSE;

	else if (strcmp(Command, "PAC") == 0)
		PACMode = TRUE;

	else if (_memicmp(Command, "QUIT", 4) == 0)
		PACMode = FALSE;

	else if (_memicmp(Command, "MYC", 3) == 0)
	{
		char * ptr = strchr(Command, ' ');
		char MYResp[80];
	
		Command[Len-1] = 0;		// Remove CR
		
		if (ptr && (strlen(ptr) > 2))
		{
			_strupr(ptr);
			strcpy(Callsign, ++ptr); 
		}

		sprintf(MYResp, "\rMycall: >%s<", Callsign);
		PutString(MYResp);
	}

	else if (_memicmp(Command, "SYS SERN", 8) == 0)
	{
		char SerialNo[] = "\r\nSerial number: 0100000000000000";
		PutString(SerialNo);
	}

	else if (_memicmp(Command, "LICENSE", 8) == 0)
	{
		PutString("\r\nLICENSE: 010000141714CB53 ABCDEFGHIJKL");
	}

	else if (_memicmp(Command, "PSKA", 4) == 0 ||
		(PACMode && _memicmp(Command, "TXL A", 5) == 0))
	{
		char cmdReply[80];
		int i;

		i = atoi(&Command[5]);

		if (i >= 0 && i <= 3000)	
		{
			TXLevel = i;
			sprintf(cmdReply, "\r\nPSKA: %d", i);
#ifdef HASPOTS
			AdjustTXLevel(TXLevel);
#endif
		}
		else
			sprintf(cmdReply, "Syntax Err: %s", Command);	
	
		PutString(cmdReply);
	}

	else if (_memicmp(Command, "FSKA", 4) == 0)
	{
		char cmdReply[80];
		int i;

		i = atoi(&Command[5]);

		if (i >= 0 && i <= 3000)	
		{
			RXLevel = i;
			sprintf(cmdReply, "\r\nFSKA: %d", i);
#ifdef HASPOTS
			AdjustRXLevel(RXLevel);
#endif
		}
		else
			sprintf(cmdReply, "Syntax Err: %s", Command);	
	
		PutString(cmdReply);
	}

	else if (_memicmp(Command, "PTCC", 4) == 0)
	{
		char SerialNo[] = "\r\nPTC-II COMPATIBILITY MODE: 0";
		PutString(SerialNo);
	}

	else if (_memicmp(Command, "MYL", 3) == 0)
	{
		char Resp[] = "\rXXXX";
		char * ptr = strchr(Command, ' ');
		int level;
		
		if (ptr)
			level = atoi(ptr);
		
		if (level == 1)
			ARQBandwidth = XB200;
		else if (level == 2)
			ARQBandwidth = XB500;
		else if (level == 3)
			ARQBandwidth = XB2500;
		else if (level == 4)
			ARQBandwidth = XB2500;

		PutString(Resp);
	}
	else if (PACMode)
	{
		if (_memicmp(Command, "BAUD 1200", 9) == 0) 
			pktMaxBandwidth = 1000;
		else if (_memicmp(Command, "BAUD 9600", 9) == 0) 
			pktMaxBandwidth = 2000;
		else if (_memicmp(Command, "MAX ", 4) == 0) 
			pktMaxFrame = atoi(&Command[4]);
		else if (_memicmp(Command, "PACL", 4) == 0)
			pktPacLen = atoi(&Command[4]);

	}
	else 
	{
		// Process as an ARDOP Command

		ProcessCommandFromHost(Command);
//
//		char SerialNo[] = "\rXXXX";
//		PutString(SerialNo);
	}

SendPrompt:	
	
	if (PACMode)
	{
		PutChar( 13);
		PutChar( 'p');
		PutChar( 'a');
		PutChar( 'c');
		PutChar( ':');
		PutChar( ' ');

		return;
	}

	if (Term4Mode)
	{
		PutChar( 13);
		PutChar( 4);
		PutChar( 13);
		PutChar( 'c');
		PutChar( 'm');
		PutChar( 'd');
		PutChar( ':');
		PutChar( ' ');
		PutChar( 1);
	}
	else
	{
		PutChar( 13);
		PutChar( 'c');
		PutChar( 'm');
		PutChar( 'd');
		PutChar( ':');
		PutChar( ' ');
	}


/*
	if (Term4Mode)
		PutChar( 4);

	PutChar( 13);
	PutChar( 'c');
	PutChar( 'm');
	PutChar( 'd');
	PutChar( ':');
	PutChar( ' ');
*/	
	return;
}


VOID ProcessSCSPacket(UCHAR * rxbuffer, unsigned int Length)
{
	unsigned short crc;
	char UnstuffBuffer[500] = "";

	// DED mode doesn't have an end of frame delimiter. We need to know if we have a full frame

	// Fortunately this is a polled protocol, so we only get one frame at a time

	// If first char != 170, then probably a Terminal Mode Frame. Wait for CR on end

	// If first char is 170, we could check rhe length field, but that could be corrupt, as
	// we haen't checked CRC. All I can think of is to check the CRC and if it is ok, assume frame is
	// complete. If CRC is duff, we will eventually time out and get a retry. The retry code
	// can clear the RC buffer

	if (HostMode & DEDMode)
	{
		// original DED Mode. 

		if (rxbuffer[0] > 1)
		{
			// ?? Char Mode
			
			HostMode = 0;
			DEDMode = 0;
			RXBPtrSCS = 0;
			return;
		}
		if (rxbuffer[2]  > Length - 4)
		{
			// Dont have it all yet

			return;
		}

		ProcessDEDModeFrame(rxbuffer, Length);
		RXBPtrSCS = 0;
		return;		
	}		

Loop:
/*
	if (rxbuffer[0] == 0 && rxbuffer[1] == 1)
	{
		// Could be host command if in recovery

		WriteDebugLog(LOGINFO, "?? Host mode Recovery ??");

		
		if (rxbuffer[2] <= RXBPtrSCS - 4)
		{
			//  Host In text mode - recovery attempt

			RXBPtrSCS -= (rxbuffer[2] +4);		// Remove command
		
			if (RXBPtrSCS)
			{
				memmove(rxbuffer, rxbuffer + 9, RXBPtrSCS + 1);
				goto Loop;
			}
			return;		// All gone
		}
	}
	*/
	if (rxbuffer[0] != 170)
	{
		UCHAR *ptr;
		int cmdlen;

		
		// Char Mode Frame I think we need to see CR on end (and we could have more than one in buffer

		// If we think we are in host mode, then it could be noise - just discard.

		if (HostMode)
		{
			RXBPtrSCS = 0;
			return;
		}

		rxbuffer[Length] = 0;

		if (rxbuffer[0] == 0x1b)
		{
			// Just ignore (I think!)

			RXBPtrSCS--;
			if (RXBPtrSCS)
			{
				memmove(rxbuffer, rxbuffer+1, RXBPtrSCS + 1);
				Length--;
				goto Loop;
			}
			return;
		}

		if (rxbuffer[0] == 0x1e)
		{
			// Status POLL

			RXBPtrSCS--;
			if (RXBPtrSCS)
			{
				memmove(rxbuffer, rxbuffer+1, RXBPtrSCS + 1);
				Length--;
				goto Loop;
			}
			PutChar(30);
			PutChar(0x87);

			if (Term4Mode)
			{
				PutChar(13);
				PutChar(4);
				PutChar(13);
				PutChar('c');
				PutChar('m');
				PutChar('d');
				PutChar(':');
				PutChar(' ');
				PutChar(1);
			}
			else
			{
				PutChar(13);
				PutChar('c');
				PutChar('m');
				PutChar('d');
				PutChar(':');
				PutChar(' ');
			}
			return;
		}

		// We are in text mode, if buffer contains nulls clear it

		if (strlen(rxbuffer) < Length)
		{
			RXBPtrSCS = 0;
			WriteDebugLog(LOGDEBUG, "cancelling input %d %d", strlen(rxbuffer), Length);
			return;
		}
		ptr = strchr(rxbuffer, 13);

		if (ptr == 0)
		{
			if (Length > 290)
				RXBPtrSCS = 0;			// unreasonable length
		
			// Wait for rest of frame

			return;
		}
		ptr++;

		cmdlen = ptr - rxbuffer;

		// Complete Char Mode Frame

		RXBPtrSCS -= cmdlen;		// Ready for next frame
		Length -= cmdlen;

		ProcessSCSTextCommand(rxbuffer, cmdlen);

		if (RXBPtrSCS)
		{
			memmove(rxbuffer, ptr, RXBPtrSCS + 1);
			if (HostMode)
			{
				// now in host mode, so pass rest up a level
				
				ProcessSCSPacket(rxbuffer, RXBPtrSCS);
				return;
			}
			goto Loop;
		}
		return;
	}

	// Receiving a Host Mode frame

	if (HostMode == 0)
	{
		RXBPtrSCS = 0;
		return;
	}

	if (Length < 6)				// Minimum Frame Sise
		return;

	if (rxbuffer[2] == 170)
	{
		// Retransmit Request
	
		RXBPtrSCS = 0;
		return;				// Ignore for now
	}

	// Can't unstuff into same buffer - fails if partial msg received, and we unstuff twice

	Length = EmUnstuff(&rxbuffer[2], &UnstuffBuffer[2], Length - 2);

	if (Length == -1)
	{
		// Unstuff returned an errors (170 not followed by 0)

		WriteDebugLog(0, "ProcessSCSPacket Bad Unstuff Frame");

		RXBPtrSCS = 0;
		return;				// Ignore for now
	}
	crc = compute_crc(&UnstuffBuffer[2], Length);

	if (crc == 0xf0b8)		// Good CRC
	{
		RXBPtrSCS = 0;		// Ready for next frame
		ProcessSCSHostFrame(&UnstuffBuffer[2], Length);
		return;
	}

	// Bad CRC - assume incomplete frame, and wait for rest. If it was a full bad frame, timeout and retry will recover link.

//	WriteDebugLog(0, "ProcessSCSPacket Bad CRC");

	return;
}


VOID EmCRCStuffAndSend(UCHAR * Msg, int Len)
{
	unsigned short int crc;
	UCHAR StuffedMsg[500];
	int i, j;

	if (PTCMode == FALSE)		// only native  mode has toggle on input
	{
		Msg[3] &= 0x7F;
		Msg[3] |= Toggle;
//		Msg[3] ^= 0x80;		// reinveret toggle so we reply with received toggle
	}
	crc = compute_crc(&Msg[2], Len-2);
	crc ^= 0xffff;

	Msg[Len++] = (crc&0xff);
	Msg[Len++] = (crc>>8);

	for (i = j = 2; i < Len; i++)
	{
		StuffedMsg[j++] = Msg[i];
		if (Msg[i] == 170)
		{
			StuffedMsg[j++] = 0;
		}
	}

	if (j != i)
	{
		Len = j;
		memcpy(Msg, StuffedMsg, j);
	}

	Msg[0] = 170;
	Msg[1] = 170;

	SerialSendData(Msg, Len);
}

const unsigned short xCRCTAB[256] = {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf, 
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7, 
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e, 
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876, 
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd, 
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5, 
0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c, 
0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974, 
0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb, 
0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3, 
0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a, 
0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72, 
0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9, 
0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1, 
0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738, 
0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70, 
0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7, 
0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff, 
0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036, 
0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e, 
0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5, 
0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd, 
0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134, 
0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c, 
0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3, 
0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb, 
0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232, 
0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a, 
0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1, 
0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9, 
0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330, 
0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78 
}; 

// Rigcontrol Stuff

void ProcessRIGPacket(int Command, char * Buffer, int Len)
{
	// RMS Express sends #TRX: Commands

	// #TRX TY I 9600 $00 TTL
	// #TRX T FEFE00E0050015050700FD
	// #TRX T FEFE00E00601FD

	// BPQ sends Data Frames with the raw contents

	if (memcmp(Buffer, "#TRX T ", 7) == 0)
	{
		// RMS Express form

		char * ptr1 =  &Buffer[7];
		char * ptr2 = Buffer;
		char c;
		unsigned char val;

		Len -= 7;
		
		while (Len-- > 0)
		{
			c = *(ptr1++);
			
			if (c >= 0x30)
			{
				c -= 0x30;
				if (c > 9)
					c -= 7;		// a-f

				val = c << 4;
			}

			Len--;

			c = *(ptr1++);
			
			if (c >= 0x30)
			{
				c -= 0x30;
				if (c > 9)
					c -= 7;		// a-f

				val |= c;
			}
			*(ptr2++) = val;
		}

		CatWrite(Buffer, ptr2 - Buffer);
		return;
	}
	CatWrite(Buffer, Len);
	return;
}

