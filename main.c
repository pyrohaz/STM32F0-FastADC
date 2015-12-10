#include <stm32f0xx_gpio.h>
#include <stm32f0xx_rcc.h>
#include <stm32f0xx_adc.h>
#include <stm32f0xx_tim.h>
#include <stm32f0xx_dma.h>

GPIO_InitTypeDef G;
ADC_InitTypeDef A;
TIM_TimeBaseInitTypeDef T;
TIM_OCInitTypeDef TO;
DMA_InitTypeDef D;
RCC_ClocksTypeDef RC;

/************ DEFINES **************/
//Destination of ADC data
#define BUF_SIZE	7500
uint8_t data[BUF_SIZE] = {0};

//Sample rate in Hz
#define FS			1000000
#define ADC_CHANNEL	ADC_Channel_0
#define ADC_PIN		GPIO_Pin_0
#define ADC_GPIO	GPIOA
/************************************/


int main(void)
{
	//Use clock provided by STLink on discovery board for less jitter
	RCC_SYSCLKConfig(RCC_SYSCLKSource_HSI);
	RCC_HSEConfig(RCC_HSE_Bypass);
	RCC_PREDIV1Config(RCC_PREDIV1_Div2);
	RCC_PLLCmd(DISABLE);
	RCC_PLLConfig(RCC_PLLSource_PREDIV1, RCC_PLLMul_12);
	RCC_PLLCmd(ENABLE);
	while(!RCC_GetFlagStatus(RCC_FLAG_PLLRDY));

	//Use PLL as system clock (48MHz);
	RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

	//Enable required clocks
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM15, ENABLE);

	//Populate clock frequency struct
	RCC_GetClocksFreq(&RC);

	//Set ADC clock to PCLK/2
	RCC_ADCCLKConfig(RCC_ADCCLK_PCLK_Div2);

	//Initialize GPIO pin
	G.GPIO_Pin = ADC_PIN;
	G.GPIO_Mode = GPIO_Mode_AN;
	G.GPIO_OType = GPIO_OType_PP;
	G.GPIO_PuPd = GPIO_PuPd_UP;
	G.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(ADC_GPIO, &G);

	//Setup the ADC for DMA transfers with T15 trigger output as the
	//conversion trigger
	A.ADC_ContinuousConvMode = DISABLE;
	A.ADC_DataAlign = ADC_DataAlign_Right;
	A.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T15_TRGO;
	A.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_Rising;
	A.ADC_Resolution = ADC_Resolution_8b;
	A.ADC_ScanDirection = ADC_ScanDirection_Upward;
	ADC_Init(ADC1, &A);
	ADC_GetCalibrationFactor(ADC1);

	//Enable ADC DMA
	ADC_DMACmd(ADC1, ENABLE);
	ADC_Cmd(ADC1, ENABLE);

	//Wait for ADC to be ready
	while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_ADRDY));

	//Set ADC conversion channel to fastest sample time
	ADC_ChannelConfig(ADC1, ADC_CHANNEL, ADC_SampleTime_1_5Cycles);

	//Configure TIM15 as the ADC trigger
	T.TIM_ClockDivision = TIM_CKD_DIV1;
	T.TIM_CounterMode = TIM_CounterMode_Up;
	T.TIM_Period = RC.PCLK_Frequency/FS -1;
	T.TIM_Prescaler = 0;
	T.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM15, &T);
	TIM_SelectOutputTrigger(TIM15, TIM_TRGOSource_Update);

	//Configure DMA for transfer
	//NOTE: Memory and peripheral sizes will need to be changed for
	//conversions larger than 8bits.
	D.DMA_BufferSize = BUF_SIZE;
	D.DMA_DIR = DMA_DIR_PeripheralSRC;
	D.DMA_M2M = DMA_M2M_Disable;
	D.DMA_MemoryBaseAddr = (uint32_t) &data;
	D.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
	D.DMA_MemoryInc = DMA_MemoryInc_Enable;
	D.DMA_Mode = DMA_Mode_Circular;
	D.DMA_PeripheralBaseAddr = (uint32_t) &(ADC1->DR);
	D.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
	D.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	D.DMA_Priority = DMA_Priority_VeryHigh;
	DMA_Init(DMA1_Channel1, &D);
	DMA_Cmd(DMA1_Channel1, ENABLE);

	//Enable timer!
	TIM_Cmd(TIM15, ENABLE);

	uint32_t N = 0;
    while(1)
    {
    	//Start conversions
    	ADC_StartOfConversion(ADC1);

    	//Wait for DMA to complete
    	while(!DMA_GetFlagStatus(DMA1_FLAG_TC1));

    	//Clear DMA flag
    	DMA_ClearFlag(DMA1_FLAG_TC1);

    	//Increment sample buffer complete counter
    	N++;
    }
}
