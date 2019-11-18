// ARDOP TNC Serial Interface for Nucleo Board
//

#define TRUE 1
#define FALSE 0


void ProcessSCSPacket(unsigned char * rxbuffer, int Length);

unsigned char RXBUFFER[300];

extern volatile int RXBPtr;

int HostInit()
{
	return TRUE;
}

void HostPoll()
{
	if (RXBPtr)
	{
		RXBUFFER[RXBPtr] = 0;
		Debugprintf("Host RX %d %s", RXBPtr, RXBUFFER);
		ProcessSCSPacket(RXBUFFER, RXBPtr);
		Sleep(100);
	}
}

//	Don't want to run anything in interrupt context, so just save rx chars to RXBUFFER

void SerialSink(unsigned char c)
{
//	if (RXBPtr < 300)
//		RXBUFFER[RXBPtr++] = c;
}


void PutString(unsigned char * Msg)
{
	SerialSendData(Msg, strlen(Msg));
}

int PutChar(unsigned char c)
{
	SerialSendData(&c, 1);
	return 0;
}



