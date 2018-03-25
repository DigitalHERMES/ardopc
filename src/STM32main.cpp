
#include "mbed.h"
#include "ARDOPC.h"

#include <stdarg.h>

#include "stm32f4xx.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_iwdg.h""

//AnalogOut my_output(PA_4);

//short buffer[BUFFER_SIZE];

enum kiss_state_e {
	KS_SEARCHING,		/* Looking for FEND to start KISS frame. */
	KS_COLLECTING};		/* In process of collecting KISS frame. */


#define MAX_KISS_LEN 2048	/* Spec calls for at least 1024. */
				/* Might want to make it longer to accomodate */
				/* maximum packet length. */

#define MAX_NOISE_LEN 100


typedef struct kiss_frame_s {

	enum kiss_state_e state;

	unsigned char kiss_msg[MAX_KISS_LEN];
				/* Leading FEND is optional. */
				/* Contains escapes and ending FEND. */
	int kiss_len;

	unsigned char noise[MAX_NOISE_LEN];
	int noise_len;

} kiss_frame_t;



extern "C" {
	void InitValidFrameTypes();
	void GetNextFECFrame();
	void direwolfmain();
	void printtick(char * msg);
	void Debugprintf(const char * format, ...);
	void dw_printf(const char * format, ...);
	void Config_ADC_DMA(void);
	void Start_ADC_DMA(void);
	void ProcessNewSamples(short * Samples, int nSamples);
	void CheckTimers();
	void MainPoll();
	void HostPoll();
	//void SetARDOPProtocolState(int State);

	uint32_t DMA_GetCurrentMemoryTarget(DMA_Stream_TypeDef* DMAy_Streamx);
	void PollReceivedSamples();
	void SetLED(int blnPTT);
	void Sleep(int delay);
	void SerialSink(UCHAR c);
	void SerialSendData(unsigned char * Msg, int Len);
	void PollReceivedSamples();

	void init_I2C1(void);
	void initdisplay();
	void kiss_rec_byte(kiss_frame_t *kf, unsigned char ch, int debug, void (*sendfun)(int,unsigned char*,int));
//	void kiss_rec_byte(kiss_frame_t *kf, unsigned char ch, int debug);
	void kiss_send_rec_packet (int chan, unsigned char *fbuf,  int flen);
	void recv_process();
	int xmit_process();
}



InterruptIn mybutton(USER_BUTTON);
DigitalOut myled(LED1);
Ticker ti;

Serial serial(USBTX, USBRX);		// Host PTC Emulation
Serial serial3(PC_10, PC_11);		// Debug Port

float delay = 0.001; // 1 mS

volatile int iTick = 0;
volatile bool bTick = 0;
volatile int iClick = 0;
volatile bool bClick = 0;
volatile int ticks;
volatile int ADCInterrupts = 0;
extern volatile int adc_buffer_mem;

#define SAMPLES_PER_BLOCK 1200

int i = 0;

void SetLED(int blnPTT)
{
	myled = blnPTT;
}

void tick()
{
	bTick = true;
	ticks++;
}

void pressed()
{
	iClick++;
	bClick = true;
}


int lastchan = 0;


// USB Port is used for SCS Host mode link to host.

// Must use interrupts (or possibly DMA) as we can't wait while processing sound.

// HostMode has a maximum frame size of around 262 bytes, and as it is polled
// we only need room for 1 frame

#define SCSBufferSize 280
#define KISSBufferSize 1024

char tx_buffer[SCSBufferSize];

// Circular buffer pointers
// volatile makes read-modify-write atomic
volatile int tx_in=0;
volatile int tx_out=0;
volatile int tx_stopped = 1;

unsigned char rx_buffer[KISSBufferSize];

// Circular buffer pointers
// volatile makes read-modify-write atomic
volatile int rx_in=0;
volatile int rx_out=0;


char line[80];

void SerialSendData(unsigned char * Msg, int Len)
{
	int i;
	i = 0;

	while (i < Len)
	{
		tx_buffer[tx_in] = Msg[i++];
		tx_in = (tx_in + 1) % SCSBufferSize;
	}

	// disable ints to avoid possible race

    // Send first character to start tx interrupts, if stopped

	 __disable_irq();

    if (tx_stopped)
    {
        serial.putc(tx_buffer[tx_out]);
        tx_out = (tx_out + 1) % SCSBufferSize;
        tx_stopped = 0;
    }
	 __enable_irq();

    return;
}

void rxcallback()
{
    // Note: you need to actually read from the serial to clear the RX interrupt

	unsigned char c;
	c = serial.getc();

	rx_buffer[rx_in] = c;
	rx_in = (rx_in + 1) % KISSBufferSize;

}

void txcallback()
{
	// Loop to fill more than one character in UART's transmit FIFO buffer
	// Stop if buffer empty

	 while ((serial.writeable()) && (tx_in != tx_out))
	 {
		 serial.putc(tx_buffer[tx_out]);
		 tx_out = (tx_out + 1) % SCSBufferSize;
	 }

	 if (tx_in == tx_out)
		 tx_stopped = 1;

	 return;
}

//  Port 3 is used for debugging

// Must use interrupts (or possibly DMA) as we can't wait while processing sound.

//	Not sure how big it needs to be. Don't want to use too mach RAM

#define DebugBufferSize 1024

char tx3_buffer[DebugBufferSize];

// Circular buffer pointers
// volatile makes read-modify-write atomic
volatile int tx3_in=0;
volatile int tx3_out=0;
volatile int tx3_stopped = 1;


void Serial3SendData(unsigned char * Msg, int Len)
{
	int i;
	i = 0;

	while (i < Len)
	{
		tx3_buffer[tx3_in] = Msg[i++];
		tx3_in = (tx3_in + 1) % DebugBufferSize;
	}

	// disable ints to avoid possible race

    // Send first character to start tx interrupts, if stopped

	 __disable_irq();

    if (tx3_stopped)
    {
        serial3.putc(tx3_buffer[tx3_out]);
        tx3_out = (tx3_out + 1) % DebugBufferSize;
        tx3_stopped = 0;
    }
	 __enable_irq();

    return;
}

void rx3callback()
{
    // Note: you need to actually read from the serial to clear the RX interrupt

	unsigned char c;

	c = serial3.getc();

//	SerialSink(c);
//	serial2.printf("%c", c);

//	 myled = !myled;
}

void tx3callback()
{
	// Loop to fill more than one character in UART's transmit FIFO buffer
	// Stop if buffer empty

	 while ((serial3.writeable()) && (tx3_in != tx3_out))
	 {
		 serial3.putc(tx3_buffer[tx3_out]);
		 tx3_out = (tx3_out + 1) % DebugBufferSize;
	 }

	 if (tx3_in == tx3_out)
		 tx3_stopped = 1;

	 return;
}


extern kiss_frame_t kf;		/* Accumulated KISS frame and state of decoder. */

int kissdebug = 2;

int main()
{
	serial.baud(115200);
	serial3.baud(115200);

	serial.attach(&rxcallback);
	serial.attach(&txcallback, Serial::TxIrq);

//	serial3.attach(&rxc3allback);
	serial3.attach(&tx3callback, Serial::TxIrq);

	 /* Check if the system has resumed from WWDG reset */

	if (RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET)
	{
		Debugprintf("Reset by watchdog");
		RCC_ClearFlag();
	}

    /* Enable write access to IWDG_PR and IWDG_RLR registers */
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

    /* IWDG counter clock: LSI/256, ~6.4ms */
    IWDG_SetPrescaler(IWDG_Prescaler_256);

    IWDG_SetReload(2000);	// ~12 secs

    /* Reload IWDG counter */
    IWDG_ReloadCounter();

    /* Enable IWDG (the LSI oscillator will be enabled by hardware) */
 //   IWDG_Enable();


	Debugprintf("Clock Freq %d", SystemCoreClock);

//	init_I2C1();

	Debugprintf("i2c init returned");

//	initdisplay();

	mybutton.fall(&pressed);

	ti.attach(tick, .001);

	direwolfmain();

	myled = 0;

	while (1)
	{
		unsigned char ch;

		if (rx_in != rx_out)
		{
			ch = rx_buffer[rx_out];
			rx_out = (rx_out + 1) % KISSBufferSize;
			kiss_rec_byte (&kf, ch, kissdebug, kiss_send_rec_packet);
		}

		PollReceivedSamples();

		recv_process();

		// See if we have anything to send

		xmit_process();
	}
}


extern "C" void PlatformSleep()
{
	// Called at end of main loop

	IWDG_ReloadCounter();

    if (bTick)
    {
  //			serial.printf("ADCInterrupts %i %d %d buffer no %d \r\n", ADCInterrupts,
  //					ADC_Buffer[0][0], ADC_Buffer[1][0], DMA_GetCurrentMemoryTarget(DMA2_Stream0));

   		bTick = false;
    }

    if (bClick)
    {
   			bClick = false;
    }

	myled = !myled;

    wait(delay);
}

void Sleep(int delay)
{
	wait(delay/1000);
	return;
}

VOID Debugprintf(const char * format, ...)
{
	char Mess[1000];
	va_list(arglist);

	va_start(arglist, format);
	vsprintf(Mess, format, arglist);
	strcat(Mess, "\r\n");

	Serial3SendData((unsigned char *)Mess, strlen(Mess));

	return;
}

VOID dw_printf(const char * format, ...)
{
	char Mess[1000];
	va_list(arglist);

	va_start(arglist, format);
	vsprintf(Mess, format, arglist);

	Serial3SendData((unsigned char *)Mess, strlen(Mess));

	return;
}






