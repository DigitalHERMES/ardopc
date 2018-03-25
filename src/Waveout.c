//
//	Passes audio samples to the sound interface

//	Windows uses WaveOut

//	Nucleo uses DMA

//	Linux will use ALSA

//	This is the Windows Version

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define _CRT_SECURE_NO_DEPRECATE
#define _USE_32BIT_TIME_T

#include <windows.h>
#include <mmsystem.h>


#ifdef USE_DIREWOLF
#include "direwolf/fsk_demod_state.h"
#include "direwolf/demod_afsk.h"
#endif

#pragma comment(lib, "winmm.lib")
void printtick(char * msg);
void PollReceivedSamples();

HANDLE OpenCOMPort(VOID * pPort, int speed, BOOL SetDTR, BOOL SetRTS, BOOL Quiet, int Stopbits);
VOID COMSetDTR(HANDLE fd);
VOID COMClearDTR(HANDLE fd);
VOID COMSetRTS(HANDLE fd);
VOID COMClearRTS(HANDLE fd);
VOID processargs(int argc, char * argv[]);


#include <math.h>

#include "ARDOPC.h"

void GetSoundDevices();


#ifdef LOGTOHOST

// Log output sent to host instead of File

#define LOGBUFFERSIZE 2048

char LogToHostBuffer[LOGBUFFERSIZE];
int LogToHostBufferLen;

#endif

// Windows works with signed samples +- 32767
// STM32 DAC uses unsigned 0 - 4095

// Currently use 1200 samples for TX but 480 for RX to reduce latency

short buffer[2][SendSize];		// Two Transfer/DMA buffers of 0.1 Sec
short inbuffer[5][ReceiveSize];	// Input Transfer/ buffers of 0.1 Sec

BOOL Loopback = FALSE;
//BOOL Loopback = TRUE;

char CaptureDevice[80] = "0"; //"2";
char PlaybackDevice[80] = "0"; //"1";

char * CaptureDevices = NULL;
char * PlaybackDevices = NULL;

int CaptureCount = 0;
int PlaybackCount = 0;

int CaptureIndex = -1;		// Card number
int PlayBackIndex = -1;


char CaptureNames[16][MAXPNAMELEN + 2]= {""};
char PlaybackNames[16][MAXPNAMELEN + 2]= {""};

WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, 12000, 12000, 2, 16, 0 };

HWAVEOUT hWaveOut = 0;
HWAVEIN hWaveIn = 0;

WAVEHDR header[2] =
{
	{(char *)buffer[0], 0, 0, 0, 0, 0, 0, 0},
	{(char *)buffer[1], 0, 0, 0, 0, 0, 0, 0}
};

WAVEHDR inheader[5] =
{
	{(char *)inbuffer[0], 0, 0, 0, 0, 0, 0, 0},
	{(char *)inbuffer[1], 0, 0, 0, 0, 0, 0, 0},
	{(char *)inbuffer[2], 0, 0, 0, 0, 0, 0, 0},
	{(char *)inbuffer[3], 0, 0, 0, 0, 0, 0, 0},
	{(char *)inbuffer[4], 0, 0, 0, 0, 0, 0, 0}
};

WAVEOUTCAPS pwoc;
WAVEINCAPS pwic;

unsigned int RTC = 0;

void InitSound(BOOL Quiet);
void HostPoll();
void TCPHostPoll();
void SerialHostPoll();
BOOL WriteCOMBlock(HANDLE fd, char * Block, int BytesToWrite);

int Ticks;

LARGE_INTEGER Frequency;
LARGE_INTEGER StartTicks;
LARGE_INTEGER NewTicks;

int LastNow;

extern void Generate50BaudTwoToneLeaderTemplate();
extern BOOL blnDISCRepeating;

#define TARGET_RESOLUTION 1         // 1-millisecond target resolution

	
VOID __cdecl Debugprintf(const char * format, ...)
{
	char Mess[10000];
	va_list(arglist);

	va_start(arglist, format);
	vsprintf(Mess, format, arglist);
	strcat(Mess, "\r\n");
	WriteDebugLog(6, Mess);

	return;
}

BOOL CtrlHandler(DWORD fdwCtrlType)
{
  switch( fdwCtrlType )
  {
    // Handle the CTRL-C signal.
    case CTRL_C_EVENT:
      printf( "Ctrl-C event\n\n" );
	  blnClosing = TRUE;
      Beep( 750, 300 );
	  Sleep(1000);
      return( TRUE );

    // CTRL-CLOSE: confirm that the user wants to exit.
 
	case CTRL_CLOSE_EVENT:

	  blnClosing = TRUE;
     printf( "Ctrl-Close event\n\n" );
	 Sleep(20000);
       Beep( 750, 300 );
	   return( TRUE );

    // Pass other signals to the next handler.
    case CTRL_BREAK_EVENT:
      Beep( 900, 200 );
      printf( "Ctrl-Break event\n\n" );
	  blnClosing = TRUE;
      Beep( 750, 300 );
     return FALSE;

    case CTRL_LOGOFF_EVENT:
      Beep( 1000, 200 );
      printf( "Ctrl-Logoff event\n\n" );
      return FALSE;

    case CTRL_SHUTDOWN_EVENT:
      Beep( 750, 500 );
      printf( "Ctrl-Shutdown event\n\n" );
	  blnClosing = TRUE;
      Beep( 750, 300 );
    return FALSE;

    default:
      return FALSE;
  }
}



void main(int argc, char * argv[])
{
	TIMECAPS tc;
	unsigned int     wTimerRes;
	DWORD	t, lastt = 0;
	int i = 0;

	SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);

	if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR) 
	{
	    // Error; application can't continue.
	}

	wTimerRes = min(max(tc.wPeriodMin, TARGET_RESOLUTION), tc.wPeriodMax);
	timeBeginPeriod(wTimerRes); 

	t = timeGetTime();

	WriteDebugLog(LOGALERT, "ARDOPC Version %s", ProductVersion);

	processargs(argc, argv);

	if (HostPort[0])
	{		
		char *pkt = strlop(HostPort, '/');

		if (_memicmp(HostPort, "COM", 3) == 0)
		{
			SerialMode = 1;
			port = atoi(HostPort + 3);
		}
		else
			port = atoi(HostPort);

		if (pkt)
			pktport = atoi(pkt);
	}

	_strupr(CaptureDevice);
	_strupr(PlaybackDevice);

	if (PTTPort[0])
	{
		char * Baud = strlop(PTTPort, ':');
		if (Baud)
			PTTBAUD = atoi(Baud);

		hPTTDevice = OpenCOMPort(PTTPort, PTTBAUD, FALSE, FALSE, FALSE, 0);
	}

	if (CATPort[0])
	{
		char * Baud = strlop(CATPort, ':');
		if (strcmp(CATPort, PTTPort) == 0)
		{
			hCATDevice = hPTTDevice;
		}
		else
		{
			if (Baud)
			CATBAUD = atoi(Baud);
			hCATDevice = OpenCOMPort(CATPort, CATBAUD, FALSE, FALSE, FALSE, 0);
		}
	}

	if (hCATDevice)
	{
		WriteDebugLog(LOGALERT, "CAT Control on port %s", CATPort); 
		COMSetRTS(hPTTDevice);
		COMSetDTR(hPTTDevice);
		if (PTTOffCmdLen)
		{
			WriteDebugLog(LOGALERT, "PTT using CAT Port", CATPort); 
			RadioControl = TRUE;
		}
	}
	else
	{
		// Warn of -u and -k defined but no CAT Port

		if (PTTOffCmdLen)
			WriteDebugLog(LOGALERT, "Warning PTT Off string defined but no CAT port", CATPort); 
	}

	if (hPTTDevice)
	{
		WriteDebugLog(LOGALERT, "Using RTS on port %s for PTT", PTTPort); 
		COMClearRTS(hPTTDevice);
		COMClearDTR(hPTTDevice);
		RadioControl = TRUE;
	}	



	QueryPerformanceFrequency(&Frequency);
	Frequency.QuadPart /= 1000;			// Microsecs
	QueryPerformanceCounter(&StartTicks);

	GetSoundDevices();
	
	if(!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
		printf("Failed to set High Priority (%d)\n"), GetLastError();



	ardopmain();
}

unsigned int getTicks()
{
	return timeGetTime();
//		QueryPerformanceCounter(&NewTicks);
//		return (int)(NewTicks.QuadPart - StartTicks.QuadPart) / Frequency.QuadPart;
}

void printtick(char * msg)
{
	QueryPerformanceCounter(&NewTicks);
	WriteDebugLog(LOGCRIT, "%s %i\r", msg, Now - LastNow);
	LastNow = Now;
}

void txSleep(int mS)
{
	// called while waiting for next TX buffer. Run background processes

	PollReceivedSamples();			// discard any received samples
	if (SerialMode)
		SerialHostPoll();
	else
		TCPHostPoll();

	Sleep(mS);
}

int PriorSize = 0;

int Index = 0;				// DMA TX Buffer being used 0 or 1
int inIndex = 0;			// DMA Buffer being used


FILE * wavfp1;

BOOL DMARunning = FALSE;		// Used to start DMA on first write

short * SendtoCard(unsigned short * buf, int n)
{
	header[Index].dwBufferLength = n * 2;

	waveOutPrepareHeader(hWaveOut, &header[Index], sizeof(WAVEHDR));
	waveOutWrite(hWaveOut, &header[Index], sizeof(WAVEHDR));

	// wait till previous buffer is complete

	while (!(header[!Index].dwFlags & WHDR_DONE))
	{
		txSleep(10);				// Run buckground while waiting 
	}

	waveOutUnprepareHeader(hWaveOut, &header[!Index], sizeof(WAVEHDR));
	Index = !Index;

	return &buffer[Index][0];
}


//		// This generates a nice musical pattern for sound interface testing
//    for (t = 0; t < sizeof(buffer); ++t)
//        buffer[t] =((((t * (t >> 8 | t >> 9) & 46 & t >> 8)) ^ (t & t >> 13 | t >> 6)) & 0xFF);

void GetSoundDevices()
{
	int i;

	WriteDebugLog(LOGALERT, "Capture Devices");

	CaptureCount = waveInGetNumDevs();

	CaptureDevices = malloc((MAXPNAMELEN + 2) * CaptureCount);
	CaptureDevices[0] = 0;
	
	for (i = 0; i < CaptureCount; i++)
	{
		waveInOpen(&hWaveIn, i, &wfx, 0, 0, CALLBACK_NULL); //WAVE_MAPPER
		waveInGetDevCaps((UINT_PTR)hWaveIn, &pwic, sizeof(WAVEINCAPS));

		if (CaptureDevices)
			strcat(CaptureDevices, ",");
		strcat(CaptureDevices, pwic.szPname);
		WriteDebugLog(LOGALERT, "%d %s", i, pwic.szPname);
		memcpy(&CaptureNames[i][0], pwic.szPname, MAXPNAMELEN);
		_strupr(&CaptureNames[i][0]);
	}

	WriteDebugLog(LOGALERT, "Playback Devices");

	PlaybackCount = waveOutGetNumDevs();

	PlaybackDevices = malloc((MAXPNAMELEN + 2) * PlaybackCount);
	PlaybackDevices[0] = 0;

	for (i = 0; i < PlaybackCount; i++)
	{
		waveOutOpen(&hWaveOut, i, &wfx, 0, 0, CALLBACK_NULL); //WAVE_MAPPER
		waveOutGetDevCaps((UINT_PTR)hWaveOut, &pwoc, sizeof(WAVEOUTCAPS));

		if (PlaybackDevices[0])
			strcat(PlaybackDevices, ",");
		strcat(PlaybackDevices, pwoc.szPname);
		WriteDebugLog(LOGALERT, "%i %s", i, pwoc.szPname);
		memcpy(&PlaybackNames[i][0], pwoc.szPname, MAXPNAMELEN);
		_strupr(&PlaybackNames[i][0]);
		waveOutClose(hWaveOut);
	}
}


void InitSound(BOOL Report)
{
	int i, ret;

	header[0].dwFlags = WHDR_DONE;
	header[1].dwFlags = WHDR_DONE;

	if (strlen(PlaybackDevice) <= 2)
		PlayBackIndex = atoi(PlaybackDevice);
	else
	{
		// Name instead of number. Look for a substring match

		for (i = 0; i < PlaybackCount; i++)
		{
			if (strstr(&PlaybackNames[i][0], PlaybackDevice))
			{
				PlayBackIndex = i;
				break;
			}
		}
	}

    ret = waveOutOpen(&hWaveOut, PlayBackIndex, &wfx, 0, 0, CALLBACK_NULL); //WAVE_MAPPER

	if (ret)
		WriteDebugLog(LOGALERT, "Failed to open WaveOut Device %s Error %d", PlaybackDevice, ret);
	else
	{
		ret = waveOutGetDevCaps((UINT_PTR)hWaveOut, &pwoc, sizeof(WAVEOUTCAPS));
		if (Report)
			WriteDebugLog(LOGALERT, "Opened WaveOut Device %s", pwoc.szPname);
	}

	if (strlen(CaptureDevice) <= 2)
		CaptureIndex = atoi(CaptureDevice);
	else
	{
		// Name instead of number. Look for a substring match

		for (i = 0; i < CaptureCount; i++)
		{
			if (strstr(&CaptureNames[i][0], CaptureDevice))
			{
				CaptureIndex = i;
				break;
			}
		}
	}

    ret = waveInOpen(&hWaveIn, CaptureIndex, &wfx, 0, 0, CALLBACK_NULL); //WAVE_MAPPER
	if (ret)
		WriteDebugLog(LOGALERT, "Failed to open WaveIn Device %s Error %d", CaptureDevice, ret);
	else
	{
		ret = waveInGetDevCaps((UINT_PTR)hWaveIn, &pwic, sizeof(WAVEINCAPS));
		if (Report)
			WriteDebugLog(LOGALERT, "Opened WaveIn Device %s", pwic.szPname);
	}

//	wavfp1 = fopen("s:\\textxxx.wav", "wb");

	for (i = 0; i < NumberofinBuffers; i++)
	{
		inheader[i].dwBufferLength = ReceiveSize * 2;

		ret = waveInPrepareHeader(hWaveIn, &inheader[i], sizeof(WAVEHDR));
		ret = waveInAddBuffer(hWaveIn, &inheader[i], sizeof(WAVEHDR));
	}

	ret = waveInStart(hWaveIn);
}

int min = 0, max = 0, leveltimer = 0;

void PollReceivedSamples()
{
	// Process any captured samples
	// Ideally call at least every 100 mS, more than 200 will loose data

	if (inheader[inIndex].dwFlags & WHDR_DONE)
	{
		short * ptr = &inbuffer[inIndex][0];
		int i;

		for (i = 0; i < ReceiveSize; i++)
		{
			if (*(ptr) < min)
				min = *ptr;
			else if (*(ptr) > max)
				max = *ptr;
			ptr++;
		}
		leveltimer++;

		if (leveltimer > 1000)
		{
			char HostCmd[64];
			leveltimer = 0;
			sprintf(HostCmd, "INPUTPEAKS %d %d", min, max);
			QueueCommandToHost(HostCmd);

			WriteDebugLog(LOGDEBUG, "Input peaks = %d, %d", min, max);
			min = max = 0;
		}

//		WriteDebugLog(LOGDEBUG, "Process %d %d", inIndex, inheader[inIndex].dwBytesRecorded/2);
		if (Capturing && Loopback == FALSE)
			ProcessNewSamples(&inbuffer[inIndex][0], inheader[inIndex].dwBytesRecorded/2);

		waveInUnprepareHeader(hWaveIn, &inheader[inIndex], sizeof(WAVEHDR));
		inheader[inIndex].dwFlags = 0;
		waveInPrepareHeader(hWaveIn, &inheader[inIndex], sizeof(WAVEHDR));
		waveInAddBuffer(hWaveIn, &inheader[inIndex], sizeof(WAVEHDR));

		inIndex++;
		
		if (inIndex == NumberofinBuffers)
			inIndex = 0;
	}
}


void StopCapture()
{
	Capturing = FALSE;

//	waveInStop(hWaveIn);
//	WriteDebugLog(LOGDEBUG, "Stop Capture");
}

void StartCapture()
{
	Capturing = TRUE;
	DiscardOldSamples();
	ClearAllMixedSamples();
	State = SearchingForLeader;

//	WriteDebugLog(LOGDEBUG, "Start Capture");
}
void CloseSound()
{ 
	waveInClose(hWaveIn);
	waveOutClose(hWaveOut);
}

#include <stdarg.h>

FILE *logfile = NULL;

VOID CloseDebugLog()
{	
	if(logfile)
		fclose(logfile);
	logfile = NULL;
}

VOID WriteDebugLog(int LogLevel, const char * format, ...)
{
	char Mess[10000];
	va_list(arglist);
	char timebuf[128];
	UCHAR Value[100];
	SYSTEMTIME st;

	
	va_start(arglist, format);
#ifdef LOGTOHOST
	vsnprintf(&Mess[1], sizeof(Mess), format, arglist);
	strcat(Mess, "\r\n");
	Mess[0] = LogLevel + '0';
	SendLogToHost(Mess, strlen(Mess));
#else
	vsnprintf(Mess, sizeof(Mess), format, arglist);
	strcat(Mess, "\r\n");


	if (LogLevel <= ConsoleLogLevel)
		printf(Mess);

	if (!DebugLog)
		return;

	GetSystemTime(&st);
	
	if (logfile == NULL)
	{
		if (HostPort[0])
			sprintf(Value, "%s%s_%04d%02d%02d.log",
				"ARDOPDebug", HostPort, st.wYear, st.wMonth, st.wDay);
		else
			sprintf(Value, "%s%d_%04d%02d%02d.log",
				"ARDOPDebug", port, st.wYear, st.wMonth, st.wDay);
		
		if ((logfile = fopen(Value, "ab")) == NULL)
			return;
	}
	sprintf(timebuf, "%02d:%02d:%02d.%03d ",
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	fputs(timebuf, logfile);

	fputs(Mess, logfile);
#endif

	return;
}

VOID WriteExceptionLog(const char * format, ...)
{
	char Mess[10000];
	va_list(arglist);
	char timebuf[32];
	UCHAR Value[100];
	FILE *logfile = NULL;
	SYSTEMTIME st;
	
	va_start(arglist, format);
	vsnprintf(Mess, sizeof(Mess), format, arglist);
	strcat(Mess, "\r\n");

	printf(Mess);

	GetSystemTime(&st);

	if (HostPort[0])
		sprintf(Value, "%s%s_%04d%02d%02d.log",
				"ARDOPException", HostPort, st.wYear, st.wMonth, st.wDay);
	else	
		sprintf(Value, "%s%d_%04d%02d%02d.log",
				"ARDOPException", port, st.wYear, st.wMonth, st.wDay);
	
	if ((logfile = fopen(Value, "ab")) == NULL)
		return;

	sprintf(timebuf, "%02d:%02d:%02d.%03d ",
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	fputs(timebuf, logfile);

	fputs(Mess, logfile);
	fclose(logfile);

	return;
}

FILE *statslogfile = NULL;

VOID CloseStatsLog()
{
	fclose(statslogfile);
	statslogfile = NULL;
}
VOID Statsprintf(const char * format, ...)
{
	char Mess[10000];
	va_list(arglist);
	UCHAR Value[100];
	char timebuf[32];

	SYSTEMTIME st;

	va_start(arglist, format);
	vsnprintf(Mess, sizeof(Mess), format, arglist);
	strcat(Mess, "\r\n");
	
	if (statslogfile == NULL)
	{
		GetSystemTime(&st);
		if (HostPort[0])
			sprintf(Value, "%s%s_%04d%02d%02d.log",
				"ARDOPSession", HostPort, st.wYear, st.wMonth, st.wDay);
		else
			sprintf(Value, "%s%d_%04d%02d%02d.log",
				"ARDOPSession", port, st.wYear, st.wMonth, st.wDay);

		if ((statslogfile = fopen(Value, "ab")) == NULL)
			return;
		else
		{
			sprintf(timebuf, "%02d:%02d:%02d.%03d\r\n",
				st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
			fputs(timebuf, statslogfile);
		}
	}

	fputs(Mess, statslogfile);
	printf(Mess);

	return;
}

VOID WriteSamples(short * buffer, int len)
{
	fwrite(buffer, 1, len * 2, wavfp1);
}

unsigned short * SoundInit()
{
	Index = 0;
	return &buffer[0][0];
}
	
//	Called at end of transmission

extern int Number;				// Number of samples waiting to be sent

void SoundFlush()
{
	// Append Trailer then wait for TX to complete

	AddTrailer();			// add the trailer.

	if (Loopback)
		ProcessNewSamples(buffer[Index], Number);

	SendtoCard(buffer[Index], Number);

	//	Wait for all sound output to complete
	
	while (!(header[0].dwFlags & WHDR_DONE))
		txSleep(10);
	while (!(header[1].dwFlags & WHDR_DONE))
		txSleep(10);

	// I think we should turn round the link here. I dont see the point in
	// waiting for MainPoll

	SoundIsPlaying = FALSE;

	//'Debug.WriteLine("[tmrPoll.Tick] Play stop. Length = " & Format(Now.Subtract(dttTestStart).TotalMilliseconds, "#") & " ms")
          
//		WriteDebugLog(LOGDEBUG, "Play complete blnEnbARQRpt = %d", blnEnbARQRpt);

	if (blnEnbARQRpt > 0 || blnDISCRepeating)	// Start Repeat Timer if frame should be repeated
		dttNextPlay = Now + intFrameRepeatInterval;

//	WriteDebugLog(LOGDEBUG, "Now %d Now - dttNextPlay 1  = %d", Now, Now - dttNextPlay);

	KeyPTT(FALSE);		 // Unkey the Transmitter

	// Clear the capture buffers. I think this is only  needed when testing
	// with audio loopback.

//	memset(&buffer[0], 0, 2400);
//	memset(&buffer[1], 0, 2400);

	StartCapture();

		//' clear the transmit label 
        //        stcStatus.BackColor = SystemColors.Control
        //        stcStatus.ControlName = "lblXmtFrame" ' clear the transmit label
        //        queTNCStatus.Enqueue(stcStatus)
        //        stcStatus.ControlName = "lblRcvFrame" ' clear the Receive label
        //        queTNCStatus.Enqueue(stcStatus)
          

	return;
}


void StartCodec(char * strFault)
{
	strFault[0] = 0;
	InitSound(FALSE);

}

void StopCodec(char * strFault)
{
	CloseSound();
	strFault[0] = 0;
}

VOID RadioPTT(BOOL PTTState)
{
	if (PTTMode & PTTRTS)
		if (PTTState)
			COMSetRTS(hPTTDevice);
		else
			COMClearRTS(hPTTDevice);

	if (PTTMode & PTTDTR)
		if (PTTState)
			COMSetDTR(hPTTDevice);
		else
			COMClearDTR(hPTTDevice);

	if (PTTMode & PTTCI_V)
		if (PTTState)
			WriteCOMBlock(hCATDevice, PTTOnCmd, PTTOnCmdLen);
		else
			WriteCOMBlock(hCATDevice, PTTOffCmd, PTTOffCmdLen);

}

//  Function to send PTT TRUE or PTT FALSE comannad to Host or if local Radio control Keys radio PTT 

const char BoolString[2][6] = {"FALSE", "TRUE"};

BOOL KeyPTT(BOOL blnPTT)
{
	// Returns TRUE if successful False otherwise

	if (blnLastPTT &&  !blnPTT)
		dttStartRTMeasure = Now;	 // start a measurement on release of PTT.

	if (!RadioControl)
		if (blnPTT)
			SendCommandToHostQuiet("PTT TRUE");
		else
			SendCommandToHostQuiet("PTT FALSE");

	else
		RadioPTT(blnPTT);

	WriteDebugLog(LOGDEBUG, "[Main.KeyPTT]  PTT-%s", BoolString[blnPTT]);

	blnLastPTT = blnPTT;
	return TRUE;
}

void PlatformSleep()
{
	//	Sleep to avoid using all cpu

	Sleep(10);
}

void displayState(const char * State)
{
	// Dummy for i2c display
}

void SetLED(int LED, int State)
{
}

void displayCall(int dirn, char * call)
{
	// Dummy for i2c display
}

HANDLE OpenCOMPort(VOID * pPort, int speed, BOOL SetDTR, BOOL SetRTS, BOOL Quiet, int Stopbits)
{
	char szPort[80];
	BOOL fRetVal ;
	COMMTIMEOUTS  CommTimeOuts ;
	int	Err;
	char buf[100];
	HANDLE fd;
	DCB dcb;

	// if Port Name starts COM, convert to \\.\COM or ports above 10 wont work

	if ((unsigned int)pPort < 256)			// just a com port number
		sprintf( szPort, "\\\\.\\COM%d", pPort);

	else if (_memicmp(pPort, "COM", 3) == 0)
	{
		char * pp = (char *)pPort;
		int p = atoi(&pp[3]);
		sprintf( szPort, "\\\\.\\COM%d", p);
	}
	else
		strcpy(szPort, pPort);

	// open COMM device

	fd = CreateFile( szPort, GENERIC_READ | GENERIC_WRITE,
                  0,                    // exclusive access
                  NULL,                 // no security attrs
                  OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL,
                  NULL );

	if (fd == (HANDLE) -1)
	{
		if (Quiet == 0)
		{
			if (pPort < (VOID *)256)
				sprintf(buf," COM%d could not be opened \r\n ", (unsigned int)pPort);
			else
				sprintf(buf," %s could not be opened \r\n ", pPort);

	//		WritetoConsoleLocal(buf);
			OutputDebugString(buf);
		}
		return (FALSE);
	}

	Err = GetFileType(fd);

	// setup device buffers

	SetupComm(fd, 4096, 4096 ) ;

	// purge any information in the buffer

	PurgeComm(fd, PURGE_TXABORT | PURGE_RXABORT |
                                      PURGE_TXCLEAR | PURGE_RXCLEAR ) ;

	// set up for overlapped I/O

	CommTimeOuts.ReadIntervalTimeout = 0xFFFFFFFF ;
	CommTimeOuts.ReadTotalTimeoutMultiplier = 0 ;
	CommTimeOuts.ReadTotalTimeoutConstant = 0 ;
	CommTimeOuts.WriteTotalTimeoutMultiplier = 0 ;
//     CommTimeOuts.WriteTotalTimeoutConstant = 0 ;
	CommTimeOuts.WriteTotalTimeoutConstant = 500 ;
	SetCommTimeouts(fd, &CommTimeOuts ) ;

   dcb.DCBlength = sizeof( DCB ) ;

   GetCommState(fd, &dcb ) ;

   dcb.BaudRate = speed;
   dcb.ByteSize = 8;
   dcb.Parity = 0;
   dcb.StopBits = TWOSTOPBITS;
   dcb.StopBits = Stopbits;

	// setup hardware flow control

	dcb.fOutxDsrFlow = 0;
	dcb.fDtrControl = DTR_CONTROL_DISABLE ;

	dcb.fOutxCtsFlow = 0;
	dcb.fRtsControl = RTS_CONTROL_DISABLE ;

	// setup software flow control

   dcb.fInX = dcb.fOutX = 0;
   dcb.XonChar = 0;
   dcb.XoffChar = 0;
   dcb.XonLim = 100 ;
   dcb.XoffLim = 100 ;

   // other various settings

   dcb.fBinary = TRUE ;
   dcb.fParity = FALSE;

   fRetVal = SetCommState(fd, &dcb);

	if (fRetVal)
	{
		if (SetDTR)
			EscapeCommFunction(fd, SETDTR);
		if (SetRTS)
			EscapeCommFunction(fd, SETRTS);
	}
	else
	{
		if ((unsigned int)pPort < 256)
			sprintf(buf,"COM%d Setup Failed %d ", pPort, GetLastError());
		else
			sprintf(buf,"%s Setup Failed %d ", pPort, GetLastError());

		printf(buf);
		OutputDebugString(buf);
		CloseHandle(fd);
		return 0;
	}

	return fd;

}

int ReadCOMBlock(HANDLE fd, char * Block, int MaxLength )
{
	BOOL       fReadStat ;
	COMSTAT    ComStat ;
	DWORD      dwErrorFlags;
	DWORD      dwLength;

	// only try to read number of bytes in queue

	ClearCommError(fd, &dwErrorFlags, &ComStat);

	dwLength = min((DWORD) MaxLength, ComStat.cbInQue);

	if (dwLength > 0)
	{
		fReadStat = ReadFile(fd, Block, dwLength, &dwLength, NULL) ;

		if (!fReadStat)
		{
		    dwLength = 0 ;
			ClearCommError(fd, &dwErrorFlags, &ComStat ) ;
		}
	}

   return dwLength;
}

BOOL WriteCOMBlock(HANDLE fd, char * Block, int BytesToWrite)
{
	BOOL        fWriteStat;
	DWORD       BytesWritten;
	DWORD       ErrorFlags;
	COMSTAT     ComStat;

	fWriteStat = WriteFile(fd, Block, BytesToWrite,
	                       &BytesWritten, NULL );

	if ((!fWriteStat) || (BytesToWrite != BytesWritten))
	{
		int Err = GetLastError();
		ClearCommError(fd, &ErrorFlags, &ComStat);
		return FALSE;
	}
	return TRUE;
}



VOID CloseCOMPort(HANDLE fd)
{
	SetCommMask(fd, 0);

	// drop DTR

	COMClearDTR(fd);

	// purge any outstanding reads/writes and close device handle

	PurgeComm(fd, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR ) ;

	CloseHandle(fd);
}


VOID COMSetDTR(HANDLE fd)
{
	EscapeCommFunction(fd, SETDTR);
}

VOID COMClearDTR(HANDLE fd)
{
	EscapeCommFunction(fd, CLRDTR);
}

VOID COMSetRTS(HANDLE fd)
{
	EscapeCommFunction(fd, SETRTS);
}

VOID COMClearRTS(HANDLE fd)
{
	EscapeCommFunction(fd, CLRRTS);
}

void CatWrite(char * Buffer, int Len)
{
	if (hCATDevice)
		WriteCOMBlock(hCATDevice, Buffer, Len);
}

extern unsigned char CatRXbuffer[256];
extern int CatRXLen;

int RadioPoll()
{
	return CatRXLen;
}

void mySetPixel(unsigned short x, unsigned short y, unsigned short Colour)
{}
void clearDisplay()
{}
void updateDisplay()
{}
void DrawAxes(int Qual, char * Mode)
{}
void DrawDecode(char * Decode)
{}

