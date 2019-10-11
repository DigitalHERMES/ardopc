/*************************
 * stm32 adc main.c
 *************************/


//#include "mbed.h"
#include "ARDOPC.h"


//#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include <stm32f4xx_tim.h>
#include <stm32f4xx_rcc.h>
#include <stm32f4xx_exti.h>
#include <stm32f4xx_dma.h>
#include <stm32f4xx_adc.h>
#include <misc.h>


// To see how to use the STM32F4 ADC DMA, here's revelent parts from the main.c file from the CELT project I did
// some time back.

// *******************************************

#define DAC_DHR12RD_ADDRESS		((uint32_t)0x40007420)
#define DAC_DHR12R2_ADDRESS		((uint32_t)0x40007414)

#define SAMPLE_RATE     24000   // 12kHz
#define CHANNELS        1               // 1 = mono, 2 = stereo
#define SAMPLES_PER_BLOCK 1200
// *******************************************

// the ADC DMA saves the incoming ADC samples into these 2 buffers

 uint16_t ADC_Buffer[2][SAMPLES_PER_BLOCK];     // stereo capture buffers

 volatile int adc_buffer_mem = -1;
 extern int ADCInterrupts;

// *******************************************
// ADC1 channel 11 on pin PC1 with DMA2 Stream 0 configuration using double buffering


// We are now using Timer2 to clock the ADCs at 12kHz

void Stop_ADC_DMA(void)
{
	// disable the DMA stream
	DMA_Cmd(DMA2_Stream0, DISABLE);

	// wait for the DMA stream to be disabled
	while (DMA_GetCmdStatus(DMA2_Stream0) != DISABLE);

	// clear the DMA interrupt flags

	DMA_ClearFlag(DMA2_Stream0, DMA_FLAG_TCIF0 | DMA_FLAG_HTIF0 | DMA_FLAG_TEIF0 | DMA_FLAG_FEIF0 | DMA_FLAG_DMEIF0);
	DMA_ClearITPendingBit(DMA2_Stream0, DMA_IT_TCIF0 | DMA_IT_HTIF0 | DMA_IT_TEIF0 | DMA_IT_FEIF0 | DMA_IT_DMEIF0);
}

void Start_ADC_DMA(void)
{
	// start from a disabled state
	Stop_ADC_DMA();

	adc_buffer_mem = -1;

	// enable the DMA stream

	DMA_Cmd(DMA2_Stream0, ENABLE);
}

void Config_ADC_DMA(void)
{
	ADC_InitTypeDef			ADC_InitStructure = {0};
	ADC_CommonInitTypeDef   ADC_CommonInitStructure = {0};
	GPIO_InitTypeDef		GPIO_InitStructure = {0};
	NVIC_InitTypeDef		NVIC_InitStructure = {0};
	DMA_InitTypeDef			DMA_InitStructure = {0};
	TIM_TimeBaseInitTypeDef	TIM_TimeBaseStructure = {0};

	// *******************************************************************


	Stop_ADC_DMA();

	ADC_DMACmd(ADC1, DISABLE);
	ADC_Cmd(ADC1, DISABLE);

	// *******************************************************************
	// setup the ADC's

        // Enable GPIOC clock
        RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

        // Enable ADC GPIO clocks
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_ADC2, ENABLE);


       // Configure ADC1-Channel-11 (PC1) as analog inputs
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;			  // PC1
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN; 			// GPIO Analog Mode
        GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;        // no pull up/down
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
        GPIO_Init(GPIOC, &GPIO_InitStructure);

        // ADC Common Init
        ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;
        ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div4;    // 2, 4, 6 or 8
//      ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled; // single ADC mode
        ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_1;    // multi ADC mode, 2 half-words one by one, 1 then 2
        ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
        ADC_CommonInit(&ADC_CommonInitStructure);

        // ADC Init
        ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;                                                  // 6, 8, 10 or 12
        ADC_InitStructure.ADC_ScanConvMode = DISABLE;

//      ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;                                                              // self clocked
//      ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
//      ADC_InitStructure.ADC_ExternalTrigConv = 0;

        ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;                                                             // now triggered by a timer
        ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_Rising;
        ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T2_TRGO;

        ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
        ADC_InitStructure.ADC_NbrOfConversion = 1;
        ADC_Init(ADC1, &ADC_InitStructure);        ADC_Init(ADC2, &ADC_InitStructure);

//      ADC_RegularChannelConfig(ADC1, ADC_Channel_11, 1, ADC_SampleTime_480Cycles);    // PC1 .. 3, 15, 28, 56, 84, 112, 144 or 480
        ADC_RegularChannelConfig(ADC1, ADC_Channel_11, 1, ADC_SampleTime_112Cycles);            // PC1 .. 3, 15, 28, 56, 84, 112, 144 or 480

        // Enable ADCs
        ADC_Cmd(ADC1, ENABLE);

        // *******************************************************************
        // Setup the DMA

        // enable DMA2 clock
        RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

        // enable double buffering .. one buffer gets filled while we are working with the other
        DMA_DoubleBufferModeConfig(DMA2_Stream0, (uint32_t)&ADC_Buffer[1], DMA_Memory_0);
        DMA_DoubleBufferModeCmd(DMA2_Stream0, ENABLE);

        // enable DMA request after last transfer (Single-ADC mode)
ADC_DMARequestAfterLastTransferCmd(ADC1, ENABLE);

	// DMA configuration


	// ADC1 is connected to channel 0 of streams 0 and 4 of DMA2. We use 0/0

	DMA_InitStructure.DMA_Channel = DMA_Channel_0;                                                          // ADC1 .. DMA2_Stream0 Channel0 .. reference manual page 219
        DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;		// single ADC address
        DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)&ADC_Buffer[0]; 	// buffer address
        DMA_InitStructure.DMA_BufferSize = SAMPLES_PER_BLOCK;				// total number of buffer elements - NOT bytes
        DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
        DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
        DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
        DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;	// element size .. Byte, HalfWord, Word
        DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
        DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;                                                         // Normal, Circular
        DMA_InitStructure.DMA_Priority = DMA_Priority_High;                                                     // Low, Medium, High, VeryHigh
        DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
        DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
        DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
        DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;

	DMA_Init(DMA2_Stream0, &DMA_InitStructure);

//      DMA_Cmd(DMA2_Stream0, ENABLE);

	// Enable DMA interrupts

//      NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);

	NVIC_InitStructure.NVIC_IRQChannel = DMA2_Stream0_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;       // lower number = higher priority
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 6;               // lower number = higher priorit
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                   // enable interrupt
	NVIC_Init(&NVIC_InitStructure);

	DMA_ITConfig(DMA2_Stream0, DMA_IT_TC, ENABLE);                          // transfer complete interrupt
	DMA_ITConfig(DMA2_Stream0, DMA_IT_TE | DMA_IT_FE | DMA_IT_DME, ENABLE); // error interrupts

        // clear the interrupt flags
        DMA_ClearFlag(DMA2_Stream0, DMA_FLAG_TCIF0 | DMA_FLAG_HTIF0 | DMA_FLAG_TEIF0 | DMA_FLAG_FEIF0 | DMA_FLAG_DMEIF0);
        DMA_ClearITPendingBit(DMA2_Stream0, DMA_IT_TCIF0 | DMA_IT_HTIF0 | DMA_IT_TEIF0 | DMA_IT_FEIF0 | DMA_IT_DMEIF0);

        // **************************************************************************
        // Setup the ADC sample rate timer

        RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

        TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
        TIM_TimeBaseStructure.TIM_Period = (90000000 / SAMPLE_RATE) - 1;        // 12KHz, from 180MHz TIM2CLK (ie APB1 = HCLK / 4, TIM2CLK = HCLK / 2)
        TIM_TimeBaseStructure.TIM_Prescaler = 0;
        TIM_TimeBaseStructure.TIM_ClockDivision = 0;
        TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
        TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

        // TIM2 TRGO selection
        TIM_SelectOutputTrigger(TIM2, TIM_TRGOSource_Update);           // ADC_ExternalTrigConv_T2_TRGO

        // TIM2 enable counter
        TIM_Cmd(TIM2, ENABLE);

        // **************************************************************************

        // enable ADC1 DMA .. ADC1 is the Master
        ADC_DMACmd(ADC1, ENABLE);
}

void DMA2_Stream0_IRQHandler(void)
{
//      DMA_Cmd(DMA2_Stream0, DISABLE);         // for IAR DEBUGING only

//      if (DMA_GetFlagStatus(DMA2_Stream0, DMA_FLAG_TCIF0) != RESET)

	if (DMA_GetITStatus(DMA2_Stream0, DMA_IT_TCIF0) != RESET)
	{
		// Transfer complete interrupt

		// clear the interrupt flag
//	DMA_ClearFlag(DMA2_Stream0, DMA_FLAG_TCIF0);

		DMA_ClearITPendingBit(DMA2_Stream0, DMA_IT_TCIF0);

		ADCInterrupts++;

		if (adc_buffer_mem < 0) // the exec is ready for new sample
			adc_buffer_mem = (DMA_GetCurrentMemoryTarget(DMA2_Stream0) == 0) ? 1 : 0;
	}

	if ((DMA_GetITStatus(DMA2_Stream0, DMA_IT_TEIF0) != RESET) ||
			(DMA_GetITStatus(DMA2_Stream0, DMA_IT_FEIF0) != RESET) ||
			(DMA_GetITStatus(DMA2_Stream0, DMA_IT_DMEIF0) != RESET))
	{

		// DMA error

		// clear the interrupt flags
		//  DMA_ClearFlag(DMA2_Stream0, DMA_FLAG_TEIF0 | DMA_FLAG_FEIF0 | DMA_FLAG_DMEIF0);

		DMA_ClearITPendingBit(DMA2_Stream0, DMA_IT_TEIF0 | DMA_IT_FEIF0 | DMA_IT_DMEIF0);

		// ToDo:
	}
}

// ********************************************************************************************

int dmamain(void)
{
	Config_ADC_DMA();
	Start_ADC_DMA();

	while (1)
	{
		// *************************
		// convert the saved ADC 12-bit unsigned samples into 16-bit signed samples
/*
                if (adc_buffer_mem >= 0 && adc_buffer_mem <= 1)
                {
                        uint16_t *src = (uint16_t *)&ADC_Buffer[adc_buffer_mem];        // point to the DMA buffer where the ADC samples were saved
                        int16_t *dst = (int16_t *)&audioInputBuffer;                            // point to our own buffer where we are going to copy them too

                        if (CHANNELS == 1)
                        {       // 12-bit unsigned stereo to 16-bit signed mono
                                int i;
                                for (i = (SAMPLES_PER_BLOCK * 2) / 2; i > 0; i--)
                                {
                                        register int32_t s1 = ((int16_t)(*src++) - 2048) << 4;  // unsigned 12-bit to 16-bit signed .. ADC channel 1
                                        register int32_t s2 = ((int16_t)(*src++) - 2048) << 4;  // unsigned 12-bit to 16-bit signed .. ADC channel 2
                                        *dst++ = (int16_t)((s1 + s2) >> 1);
                                }
                        }
                        else
                        if (CHANNELS == 2)
                        {       // 12-bit unsigned stereo to 16-bit signed stereo
                                int i;
                                for (i = (SAMPLES_PER_BLOCK * 2) / 2; i > 0; i--)
                                {
                                        *dst++ = ((int16_t)(*src++) - 2048) << 4;       // unsigned 12-bit to 16-bit signed .. ADC channel 1
                                        *dst++ = ((int16_t)(*src++) - 2048) << 4;       // unsigned 12-bit to 16-bit signed .. ADC channel 2
                                }
                        }



                        // toggle an output pin - test only
                        GPIO_ToggleBits(GPIOD, GPIO_Pin_15);
                }

                adc_buffer_mem = -1;    // finished with the ADC buffer that the DMA filled

                // *************************



*/
	}
	return 0;
}
