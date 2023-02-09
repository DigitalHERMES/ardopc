// ARDOP TNC Host Interface


#define HANDLE int

#define _XOPEN_SOURCE 600
#define _GNU_SOURCE

#include <features.h>

#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <fcntl.h>
#include <syslog.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>

#include "ARDOPC.h"

HANDLE hDevice;

VOID ProcessSCSPacket(UCHAR * rxbuffer, unsigned int Length);
BOOL WriteCOMBlock(HANDLE fd, char * Block, int BytesToWrite);


//#include <pty.h>

HANDLE LinuxOpenPTY(char * Name)
{            
	// Open a Virtual COM Port

	HANDLE hDevice, slave;
	char slavedevice[80];
	int ret;
	u_long param=1;
	struct termios term;

#ifdef MACBPQ

	// Create a pty pair
	
	openpty(&hDevice, &slave, &slavedevice[0], NULL, NULL);
	close(slave);

#else
	 
	hDevice = posix_openpt(O_RDWR|O_NOCTTY);

	if (hDevice == -1 || grantpt (hDevice) == -1 || unlockpt (hDevice) == -1 ||
		 ptsname_r(hDevice, slavedevice, 80) != 0)
	{
		perror("Create PTY pair failed");
		return -1;
	} 

#endif

	printf("slave device: %s\n", slavedevice);
 
	if (tcgetattr(hDevice, &term) == -1)
	{
		perror("tty_speed: tcgetattr");
		return FALSE;
	}

	cfmakeraw(&term);

	if (tcsetattr(hDevice, TCSANOW, &term) == -1)
	{
		perror("tcsetattr");
		return -1;
	}

	ioctl(hDevice, FIONBIO, &param);

	chmod(slavedevice, S_IRUSR|S_IRGRP|S_IWUSR|S_IWGRP|S_IROTH|S_IWOTH);

	unlink (Name);
		
	ret = symlink (slavedevice, Name);
		
	if (ret == 0)
		printf ("symlink to %s created\n", Name);
	else
		printf ("symlink to %s failed\n", Name);	
	
	return hDevice;
}


int SerialSendData(UCHAR * Message,int MsgLen)
{
	// Linux uses normal IO for all ports

	WriteCOMBlock(hDevice, Message, MsgLen);
	return 0;
}

int xSerialGetData(UCHAR * Message, unsigned int BufLen, unsigned long * MsgLen)
{
	return 0;
}


BOOL SerialHostInit()
{
	hDevice = LinuxOpenPTY(HostPort);

	if (hDevice)
	{
		printf("PTY Handle %d\n", hDevice);
		// Set up buffer pointers
	}
	return TRUE;
}


int ReadCOMBlock(HANDLE fd, char * Block, int MaxLength)
{
	int Length;
	
	Length = read(fd, Block, MaxLength);

	if (Length < 0)
	{
		return 0;
	}

	return Length;
}


static struct speed_struct
{
	int	user_speed;
	speed_t termios_speed;
} speed_table[] = {
	{300,         B300},
	{600,         B600},
	{1200,        B1200},
	{2400,        B2400},
	{4800,        B4800},
	{9600,        B9600},
	{19200,       B19200},
	{38400,       B38400},
	{57600,       B57600},
	{115200,      B115200},
	{-1,          B0}
};


HANDLE OpenCOMPort(VOID * Port, int speed, BOOL SetDTR, BOOL SetRTS, BOOL Quiet, int Stopbits)
{;
	char buf[100];

	//	Linux Version.

	int fd;
	int hwflag = 0;
	u_long param=1;
	struct termios term;
	struct speed_struct *s;

	// As Serial ports under linux can have all sorts of odd names, the code assumes that
	// they are symlinked to a com1-com255 in the BPQ Directory (normally the one it is started from

	if ((fd = open(Port, O_RDWR | O_NDELAY)) == -1)
	{
		if (Quiet == 0)
		{
			perror("Com Open Failed");
			sprintf(buf," %s could not be opened", Port);
			Debugprintf(buf);
		}
		return 0;
	}

	// Validate Speed Param

	for (s = speed_table; s->user_speed != -1; s++)
		if (s->user_speed == speed)
			break;

   if (s->user_speed == -1)
   {
	   fprintf(stderr, "tty_speed: invalid speed %d", speed);
	   return FALSE;
   }

   if (tcgetattr(fd, &term) == -1)
   {
	   perror("tty_speed: tcgetattr");
	   return FALSE;
   }

   	cfmakeraw(&term);
	cfsetispeed(&term, s->termios_speed);
	cfsetospeed(&term, s->termios_speed);

	if (tcsetattr(fd, TCSANOW, &term) == -1)
	{
		perror("tty_speed: tcsetattr");
		return FALSE;
	}

	ioctl(fd, FIONBIO, &param);

	Debugprintf("LinBPQ Port %s fd %d", Port, fd);

	return fd;
}

BOOL WriteCOMBlock(HANDLE fd, char * Block, int BytesToWrite)
{
	//	Some systems seem to have a very small max write size
	
	int ToSend = BytesToWrite;
	int Sent = 0, ret;

	while (ToSend)
	{
		ret = write(fd, &Block[Sent], ToSend);

		if (ret >= ToSend)
			return TRUE;

		if (ret == -1)
		{
			if (errno != 11 && errno != 35)					// Would Block
				return FALSE;
	
			usleep(10000);
			ret = 0;
		}
						
		Sent += ret;
		ToSend -= ret;
	}
	return TRUE;
}

VOID CloseCOMPort(HANDLE fd)
{
	close(fd);
}

VOID COMSetDTR(HANDLE fd)
{
	int status;

	ioctl(fd, TIOCMGET, &status);
	status |= TIOCM_DTR;
    ioctl(fd, TIOCMSET, &status);
}

VOID COMClearDTR(HANDLE fd)
{
	int status;

	ioctl(fd, TIOCMGET, &status);
	status &= ~TIOCM_DTR;
    ioctl(fd, TIOCMSET, &status);
}

VOID COMSetRTS(HANDLE fd)
{
	int status;

	if (ioctl(fd, TIOCMGET, &status) == -1)
		perror("ARDOP PTT TIOCMGET");
	status |= TIOCM_RTS;
	if (ioctl(fd, TIOCMSET, &status) == -1)
		perror("ARDOP PTT TIOCMSET");
}

VOID COMClearRTS(HANDLE fd)
{
	int status;

	if (ioctl(fd, TIOCMGET, &status) == -1)
		perror("ARDOP PTT TIOCMGET");
	status &= ~TIOCM_RTS;
	if (ioctl(fd, TIOCMSET, &status) == -1)
		perror("ARDOP PTT TIOCMSET");
}


UCHAR RXBUFFER[500]; // Long enough for stuffed Host Mode frame

extern volatile int RXBPtr;

VOID SerialHostPoll()
{
	unsigned long Read = 0;

	Read = ReadCOMBlock(hDevice, &RXBUFFER[RXBPtr], 499 - RXBPtr);

	if (Read)
	{		
		RXBPtr += Read;
		ProcessSCSPacket(RXBUFFER, RXBPtr);
	}

//	if (UseKISS)
//		KISSTCPPoll();
}

VOID PutString(UCHAR * Msg)
{
	SerialSendData(Msg, strlen(Msg));
}

int PutChar(UCHAR c)
{
	SerialSendData(&c, 1);
	return 0;
}

void CatWrite(char * Buffer, int Len)
{
	WriteDebugLog(5, "CAT Write Len %d %s", Len, Buffer);
	if (hCATDevice)
		WriteCOMBlock(hCATDevice, Buffer, Len);
}
