#include "stm8s.h"
//#include "stm8s_gpio.h"
#include "stdio.h"
#include "ir_decoder.h"

#define UART

uint8_t g_flag1ms = 0; //flag for 1ms interrupt (for TIM4 ISR)
uint32_t g_count1ms = 0; //1ms counter (for TIM4 ISR)

static void delay(uint32_t t){
	while(t--);
}

#ifdef UART

/* моя реализация putchar и его производных */
void putchar(char c){
	while(!(UART1->SR & UART1_FLAG_TC));
	UART1->DR = c;
}
void u8_print(uint8_t dig){
	uint8_t d = dig >> 4;
	if(d > 0x9)
		d += 'A' - 0xA;
	else
		d += '0';
	putchar(d);
	d = dig & 0xf;
	if(d > 0x9)
		d += 'A' - 0xA;
	else
		d += '0';
	putchar(d);
}
void u16_print(uint16_t dig){
	u8_print((dig >> 8 )& 0xFF);
	u8_print(dig & 0xFF);
}
void u32_print(uint32_t dig){
	u16_print((dig >> 16 )& 0xFFFF);
	u16_print(dig & 0xFFFF);
}

void strsend(char *str){
	while(*str) putchar(*(str++));
}
//слегка обрезаная версия кода из STM8 PeriphLib
void MY_UART1_Init(uint32_t BaudRate, UART1_WordLength_TypeDef WordLength,
								UART1_StopBits_TypeDef StopBits, UART1_Parity_TypeDef Parity,
								UART1_SyncMode_TypeDef SyncMode){
	uint32_t BaudRate_Mantissa = 0, BaudRate_Mantissa100 = 0;
	/* Clear the word length bit */
	UART1->CR1 &= (uint8_t)(~UART1_CR1_M);
	/* Set the word length bit according to UART1_WordLength value */
	UART1->CR1 |= (uint8_t)WordLength;
	/* Clear the STOP bits */
	UART1->CR3 &= (uint8_t)(~UART1_CR3_STOP);
	/* Set the STOP bits number according to UART1_StopBits value  */
	UART1->CR3 |= (uint8_t)StopBits;
	/* Clear the Parity Control bit */
	UART1->CR1 &= (uint8_t)(~(UART1_CR1_PCEN | UART1_CR1_PS  ));
	/* Set the Parity Control bit to UART1_Parity value */
	UART1->CR1 |= (uint8_t)Parity;
	/* Clear the LSB mantissa of UART1DIV  */
	UART1->BRR1 &= (uint8_t)(~UART1_BRR1_DIVM);
	/* Clear the MSB mantissa of UART1DIV  */
	UART1->BRR2 &= (uint8_t)(~UART1_BRR2_DIVM);
	/* Clear the Fraction bits of UART1DIV */
	UART1->BRR2 &= (uint8_t)(~UART1_BRR2_DIVF);
	/* Set the UART1 BaudRates in BRR1 and BRR2 registers according to UART1_BaudRate value */
	BaudRate_Mantissa    = ((uint32_t)CLK_GetClockFreq() / (BaudRate << 4));
	BaudRate_Mantissa100 = (((uint32_t)CLK_GetClockFreq() * 100) / (BaudRate << 4));
	/* Set the fraction of UART1DIV  */
	UART1->BRR2 |= (uint8_t)((uint8_t)(((BaudRate_Mantissa100 - (BaudRate_Mantissa * 100)) << 4) / 100) & (uint8_t)0x0F);
	/* Set the MSB mantissa of UART1DIV  */
	UART1->BRR2 |= (uint8_t)((BaudRate_Mantissa >> 4) & (uint8_t)0xF0);
	/* Set the LSB mantissa of UART1DIV  */
	UART1->BRR1 |= (uint8_t)BaudRate_Mantissa;
	/* Disable the Transmitter and Receiver before setting the LBCL, CPOL and CPHA bits */
	UART1->CR2 &= (uint8_t)~(UART1_CR2_TEN | UART1_CR2_REN);
	/* Clear the Clock Polarity, lock Phase, Last Bit Clock pulse */
	UART1->CR3 &= (uint8_t)~(UART1_CR3_CPOL | UART1_CR3_CPHA | UART1_CR3_LBCL);
	/* Set the Clock Polarity, lock Phase, Last Bit Clock pulse */
	UART1->CR3 |= (uint8_t)((uint8_t)SyncMode & (uint8_t)(UART1_CR3_CPOL |
																												UART1_CR3_CPHA | UART1_CR3_LBCL));
	/* Set the Transmitter Enable bit */
	UART1->CR2 |= (uint8_t)UART1_CR2_TEN;
	/* Clear the Receiver Disable bit */
	UART1->CR2 &= (uint8_t)(~UART1_CR2_REN);
	/* Set the Clock Enable bit, lock Polarity, lock Phase and Last Bit Clock 
	pulse bits according to UART1_Mode value */
	/* Clear the Clock Enable bit */
	UART1->CR3 &= (uint8_t)(~UART1_CR3_CKEN);
}
#endif /* UART */

void CLK_Config(void);
int main(void){
	int a = 0;
	uint8_t key = 0;
	GPIOB->DDR |= GPIO_PIN_5;
	GPIOB->CR1 |= GPIO_PIN_5;
	GPIOB->ODR |= GPIO_PIN_5;

	/* Initialization of the clock */
	CLK_Config();
#ifdef UART
	//UART1_DeInit(); //и так работает!
	MY_UART1_Init((uint32_t)115200, UART1_WORDLENGTH_8D, UART1_STOPBITS_1, UART1_PARITY_NO,
		UART1_SYNCMODE_CLOCK_DISABLE);
	//printf использовать можно но очень уд жорого получается(по занимаемому месту!)
	strsend("OwL is READY!\n\r");
#endif /* UART */

	ir_decoder_init();

	/* сигнал инвертирован. то есть на вход пина в спокойном состоянии все время подается 1 а при
		 приеме импульса 0! */

	//http://ziblog.ru/2011/07/31/rabotaem-s-ik-pultom.html - тут пример с двумя каналами таймера!
	//TIM2_CH1 - PD4
	//TIM2_CH2 - PD3
	//эти два пина необходимо соединить вместе. я так и не нашел способ програмно CH2 подсоединить к PD4.

	//http://embedded-lab.com/blog/continuing-stm8-microcontroller-expedition/6/
	/* Ссылка выше на английском но там более менее написано чтобы понять как работает таймер.
		 Нижний код переключает таймер в режим PWM input signal measurement.
   	 Данный режим работы предназначен для измерения длительности импульса и периода ШИМ сигнала.
   */

	/* http://ziblog.ru/2013/05/14/distantsionnoe-upravlenie-ot-ik-pulta.html
		 тут пример с одним каналом! я его использовал! */
	//таймер2 - счетчик 8 мкс(длительность одного тика таймера) для формирования интервалов, таймаутов
	//и "захвата" импульсов от ИК приемника
	/* подключать таймер к шине тактирования. это обычно делают в CLK_Config */
	CLK_PeripheralClockConfig(CLK_PERIPHERAL_TIMER2, ENABLE);
	TIM2_DeInit(); //ресет таймера
	//устанавливается частота работы таймера. prescaler - предделитель, 16250 - 1 это периот таймера.
	/* 1 тик таймера это:
			16Mhz / 32 =  500Khz = 0.002ms = 2µs
			16Mhz / 64 =  250Khz = 0.004ms = 4µs
			16Mhz / 128 = 125Khz = 0.008ms = 8µs
			я подключал вход stm8 к ренератору на 1Khz(тестовый выход осцилографа):
				DIV_32  | 500Khz | 0x1EC | 492 | 492 * 0.002ms = 0,984ms ~ 1Khz
				DIV_64  | 250Khz | 0x0F6 | 246 | 246 * 0.004ms = 0,984ms ~ 1Khz
				DIV_128 | 125Khz | 0x07B | 123 | 123 * 0.008ms = 0,984ms ~ 1Khz
			То есть для DIV_128 таймер тикает 125 000 раз в секунду.
		 	Второй параметр это макимальное число тиков таймера, 	по достижении которых
		 	при появлении сигнала на входах DP4 и DP4 прерывания для CH1 и CH2 больше не
		 	генерируются! и соответственно по таймауту сгерерируется прерывание CH3.
		 	В частности 16250 - 1 это 130 ms при DIV_128 16250×0.008ms = 130ms.
		 	Минус 1 так как отсчет ведется начиная с 0. */
	TIM2_TimeBaseInit(TIM2_PRESCALER_128, 16250 - 1); //устанавливается частота работы таймера
	//чтобы прерывание таймаута вызывалось один раз а не долбило постоянно(No repeat)
	TIM2_SelectOnePulseMode(TIM2_OPMODE_SINGLE);
	/* захват импульса от ИК приемника. смотри картинку http://ziblog.ru/2011/07/31/rabotaem-s-ik-pultom.html.
		 у нас сигнал инвертирован и нам интересны интервалы тишины(т.к время передачи импульса это константа
		 560µs или 0.56ms) и соответственно время полного интервала это полученное нами тут время + 560µs. */
	/* если поменять местами CH1 и CH2(FALLING с RISING) то будет мерять только длину импульса(0.56ms). всегда!
	   то есть все значения будут примерно одинаковые: 004D 004C 004D 0046 004C ... */
	/* настраиваем 1-й канал таймера на RISING импульса */
	TIM2_ICInit(TIM2_CHANNEL_1, TIM2_ICPOLARITY_RISING, TIM2_ICSELECTION_DIRECTTI, TIM2_ICPSC_DIV1, 0xF);
	/* настраиваем 2-й канал таймера на FALLING импульса */
	TIM2_ICInit(TIM2_CHANNEL_2, TIM2_ICPOLARITY_FALLING,  TIM2_ICSELECTION_DIRECTTI, TIM2_ICPSC_DIV1, 0xF);
	//IC - это Input, есть еще OC - Output(для работы в режиме шим генератора!)
	//включение генерации прерывания(InterrupT) для 1-го(CC1) канала таймера. IT - Interrupt.
	TIM2_ClearFlag(TIM2_FLAG_CC1); //подготовка перед включением генерации перывания
	TIM2_ITConfig(TIM2_IT_CC1, ENABLE); //включение генерации прерывания
	//аналогично включение генерации прерывания для 2-го(CC2) канала таймера
	TIM2_ClearFlag(TIM2_FLAG_CC2);
	TIM2_ITConfig(TIM2_IT_CC2, ENABLE);
	/* формирование таймаута - 80 мс. опять таки в тиках таймера! 80ms/0.008ms = 10000ticks
		 если никаких умпульсов не поступит в течении этого времени то считаем что прием IR
		 пакета данных завершен и отображаем накопленные результаты. */
	TIM2_SetCompare3(10000); //этот таймаут отмеряет 3-й(CCR3) канал наймера !
	//аналогично включение генерации прерывания для 3-го(CC3) канала таймера
	TIM2_ClearFlag(TIM2_FLAG_CC3);
	TIM2_ITConfig(TIM2_IT_CC3, ENABLE);

	/* врубаем таймер - чтобы он сделал первый проход.
		 это необходимо чтобы таймер проинициализировался.
		 первый проход задержка импульса считается неверно. */
	TIM2_Cmd(ENABLE);

	//разрешаем прерывания
	enableInterrupts();

	while(1){
		//delay(100000);
		//GPIOB->ODR ^= GPIO_PIN_5;
		//key = GPIO_ReadInputData(IR_PORT) & 0x10;
		if(ir_decoder.is_received){
			uint32_t code;
			disableInterrupts();
			if(ir_decoder.index){
				code = calc_32bit_ir_code();
				//мигаем встроенной лампочкой при нажатии кнопки OK
				if(code == 0x189835B5)
					GPIOB->ODR ^= GPIO_PIN_5;
#ifdef UART
				strsend("32bit IR code: ");
				u32_print(code);
				strsend(", ");
				u8_print(ir_decoder.index);
				strsend("\n\r");
				print_ir_delays();
				strsend("\n\r");
			}
#endif /* UART */
			ir_decoder_init();
			//готовы новому приему
			enableInterrupts();
		}
	}
}

/* Обработчик прерывания от TIM2 */
//тут все на регистрах. ищи в StdPeriph_Lib-е эти регистры чтобы понимать что они делают.
INTERRUPT_HANDLER(TIM2_CAP_COM_IRQHandler, 14){
	//для отладки фаз(какой канал идет первым и в какой последовательности)
	if(ir_decoder.phases_count++ < 15)
		ir_decoder.phases <<= 2;
	if(TIM2->SR1 & TIM2_SR1_CC1IF){ //1-й канал таймера - RISING импульса тишины
		ir_decoder.phases |= 0x1;
		//strsend("TIM2 chan1 is TRIGGED\n\r");
		/* обнуление таймера(то что он раньше насчитал.
			 TIM2_CH2 уже поссчитает задержку начиная с этого момента. */
		TIM2->CNTRL = 0;
		TIM2->CNTRH = 0;
		//запуск таймера для измерения длительности отсюда до FALLING
		TIM2->CR1 |= TIM2_CR1_CEN;
		//очистка бита ожидания прерывания(аналог TIM2_ClearFlag)
		TIM2->SR1 = (uint8_t) (~TIM2_SR1_CC1IF);
	}else if(TIM2->SR1 & TIM2_SR1_CC2IF){ //2-й канал таймера - FALLING импульса тишины
		ir_decoder.phases |= 0x2;
		//strsend("TIM2 chan2 is TRIGGED\n\r");
		ir_decoder_refresh();
		//strsend("ir_decoder_refresh()\n\r");
		//запуск таймера таймаута
		TIM2->CR1 |= TIM2_CR1_CEN;
		//очистка бита ожидания прерывания(аналог TIM2_ClearFlag)
		TIM2->SR1 = (uint8_t) (~TIM2_SR1_CC2IF);
	}else if(TIM2->SR1 & TIM2_SR1_CC3IF){ //3-й канал таймера
		ir_decoder.phases |= 0x3;
		//strsend("TIM2 chan3 is TRIGGED\n\r");
		ir_decoder_refresh_timeout();
		//strsend("ir_decoder_refresh_timeout()\n\r");
		//очистка бита ожидания прерывания(аналог TIM2_ClearFlag)
		TIM2->SR1 = (uint8_t) (~TIM2_SR1_CC3IF);
	}
}

void assert_failed(uint8_t* file, uint32_t line){
	file = NULL;
	line = 0;
	/* Infinite loop */
	while(1);
}

void CLK_Config(void){
	CLK_DeInit();
	/* Clock divider to HSI/1 */
	//https://www.drive2.ru/b/1595687/
	//от этого зависит работа таймера TIM2 !!!!
	//HSI(встроенный RC генератор) - 16Мгц
	CLK_HSIPrescalerConfig(CLK_PRESCALER_HSIDIV1);

	/* Output Fcpu on CLK_CCO pin */
	CLK_CCOConfig(CLK_OUTPUT_MASTER);
}
