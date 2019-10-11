#include "ARDOPC.h"

//#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include <stm32f4xx_tim.h>
#include <stm32f4xx_rcc.h>
//#include <stm32f4xx_exti.h>
#include <stm32f4xx_dma.h>
#include <stm32f4xx_dac.h>

extern unsigned short buffer[2][1200];

#define   DAC_DHR12R1_ADDR  0x40007408			// DAC 12 Bit Left Justified
#define   DAC_DHR12L1_ADDR  0x40007410			// DAC 12 Bit Right Justified
#define   CNT_FREQ			90000000			// TIM6 clock (prescaled APB1) ?? 180/2
#define   TIM_PERIOD		(CNT_FREQ/24000)	// Generate DMS Request at this interval.

static void TIM6_Config(void);
static void DAC1_Config(void);

// DMA 1 Stream 5 Channel 7 is hard wired to the DAC

void StartDAC()
{
	// Set up the DAC and start sending a frame under DMA
	GPIO_InitTypeDef gpio_A;

	//	We don't need to do this each time, but I doubt if it costs much

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);

	gpio_A.GPIO_Pin  = GPIO_Pin_4;
	gpio_A.GPIO_Mode = GPIO_Mode_AN;
	gpio_A.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &gpio_A);

	DAC1_Config();
	TIM6_Config();
}

void stopDAC()
{
	TIM_Cmd(TIM6, DISABLE);
	DMA_Cmd(DMA1_Stream5, DISABLE);
	DAC_Cmd(DAC_Channel_1, DISABLE);
	DAC_DMACmd(DAC_Channel_1, DISABLE);
}

static void TIM6_Config(void)
{
	TIM_TimeBaseInitTypeDef TIM6_TimeBase;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);

	TIM_TimeBaseStructInit(&TIM6_TimeBase);
	TIM6_TimeBase.TIM_Period        = (uint16_t)TIM_PERIOD;
	TIM6_TimeBase.TIM_Prescaler     = 0;
	TIM6_TimeBase.TIM_ClockDivision = 0;
	TIM6_TimeBase.TIM_CounterMode   = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM6, &TIM6_TimeBase);
	TIM_SelectOutputTrigger(TIM6, TIM_TRGOSource_Update);	// So it triggers DMA

	TIM_Cmd(TIM6, ENABLE);
}

static void DAC1_Config(void)
{
	DAC_InitTypeDef DAC_INIT;
	DMA_InitTypeDef DMA_INIT;

	DAC_INIT.DAC_Trigger        = DAC_Trigger_T6_TRGO;
	DAC_INIT.DAC_WaveGeneration = DAC_WaveGeneration_None;
	DAC_INIT.DAC_OutputBuffer   = DAC_OutputBuffer_Enable;
	DAC_Init(DAC_Channel_1, &DAC_INIT);

	DMA_DeInit(DMA1_Stream5);

	// enable double buffering .. one buffer gets sent while we are filling the other

	DMA_DoubleBufferModeConfig(DMA1_Stream5, (uint32_t)&buffer[1], DMA_Memory_0);
	DMA_DoubleBufferModeCmd(DMA1_Stream5, ENABLE);

	DMA_INIT.DMA_Channel            = DMA_Channel_7;
	DMA_INIT.DMA_PeripheralBaseAddr = DAC_DHR12R1_ADDR;
	DMA_INIT.DMA_Memory0BaseAddr    = (uint32_t)&buffer[0];
	DMA_INIT.DMA_DIR                = DMA_DIR_MemoryToPeripheral;
	DMA_INIT.DMA_BufferSize         = 1200;
	DMA_INIT.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
	DMA_INIT.DMA_MemoryInc          = DMA_MemoryInc_Enable;
	DMA_INIT.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_INIT.DMA_MemoryDataSize     = DMA_MemoryDataSize_HalfWord;
	DMA_INIT.DMA_Mode               = DMA_Mode_Circular;
	DMA_INIT.DMA_Priority           = DMA_Priority_High;
	DMA_INIT.DMA_FIFOMode           = DMA_FIFOMode_Disable;
	DMA_INIT.DMA_FIFOThreshold      = DMA_FIFOThreshold_HalfFull;
	DMA_INIT.DMA_MemoryBurst        = DMA_MemoryBurst_Single;
	DMA_INIT.DMA_PeripheralBurst    = DMA_PeripheralBurst_Single;
	DMA_Init(DMA1_Stream5, &DMA_INIT);

	DMA_Cmd(DMA1_Stream5, ENABLE);
	DAC_Cmd(DAC_Channel_1, ENABLE);
	DAC_DMACmd(DAC_Channel_1, ENABLE);
}
