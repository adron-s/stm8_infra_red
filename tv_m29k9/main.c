#include "stm8s.h"
#include "stdio.h"
#include "ds18b20.h"

uint8_t g_flag1ms = 0; //flag for 1ms interrupt (for TIM4 ISR)
uint32_t g_count1ms = 0; //1ms counter (for TIM4 ISR)

//включение UART-а: PD5. RX only.
//#define UART
#include "uart_mini.h"

//пороговое значение входного напряжения при котором приставка считается включенной
#define STATUS_PIN_VCC_TRIG_THRE 0x249
//при какой температуре(градусы, >=) включать вентилятор
#define FAN_NEED_ON_TEMPERATURE 25

/* сколько для DIV_8 таймера нужно тиков чтобы достичь 38Khz */
#define KHZ_38 52
#define KHZ_38_HALF 26
//сколько проходов таймера нужно чтобы достичь 0.56ms
#define DOT_56 21

//выполняется ли передача IR импульса в данный момент
static volatile uint8_t is_ir_in_progress;

//какая то задержка. хз сколько...зависит от микроконтроллера. использовать на глазок.
static void delay(uint32_t t){
	while(t--) nop();
}

/* раелизация analogRead.
	 http://www.electroons.com/blog/stm8-tutorials-3-adc-interfacing/ */
uint16_t readADC1(uint8_t channel){
	uint16_t val = 0;
  //using ADC in single conversion mode
  ADC1->CSR |= (0x0F & channel); //select channel
  ADC1->CR2 |= 1 << 3; //Right Aligned Data
  ADC1->CR1 |= 1 << 0; //ADC ON
  ADC1->CR1 |= 1 << 0; //ADC Start Conversion
  while(((ADC1->CSR) & (1 << 7)) == 0); //Wait till EOC
  val |= (uint8_t)ADC1->DRL;
  val |= (uint8_t)ADC1->DRH << 8;
  ADC1->CR1 &= ~(1 << 0); // ADC Stop Conversion
  val &= 0x03ff;
  return val;
}

void CLK_Config(void);
void tim2_pre_start_init(void);
int main(void){
	uint32_t prot = 0;
	uint8_t now_state = 20;
	uint8_t last_state = 10;
	volatile uint16_t now_voltage = 0;
	volatile char temperature = 0;
	volatile uint32_t temperature_need_read = 0x0;

	//светодиод на плате. включаем эту ножку на выход.
	GPIOB->DDR |= GPIO_PIN_5;
	GPIOB->CR1 |= GPIO_PIN_5;
	GPIOB->ODR |= GPIO_PIN_5; //!инвертированный

	/* Status Pin: C4 - ADC1-3 вход с тв приставки.
	 	 сигнализирует о включенном или выключенном состоянии.
		 я его дальше использую как аналоговый вход. */
	//GPIOC->CR2 &= ~GPIO_PIN_4;
	//GPIOC->DDR &= ~GPIO_PIN_4; /* Set Input mode */
	//GPIOC->CR1 |= GPIO_PIN_4;
	//GPIOC->CR1 &= ~GPIO_PIN_4;

	//C5 - управление вентилятором через транзистор
	GPIOC->DDR |= GPIO_PIN_5;
	GPIOC->CR1 |= GPIO_PIN_5;
	GPIOC->ODR &= ~GPIO_PIN_5; //по умолчанию вЫкл

	//PD4 - TIM2_CH1
	GPIOD->DDR |= GPIO_PIN_4;
	GPIOD->CR1 |= GPIO_PIN_4;
	GPIOD->ODR &= ~GPIO_PIN_4; //чтобы TIM2_CH1 до запуска таймера был LOW

	//PD5 - для тестов как работает прерывание таймера. я этот пин оттуда дергаю и смотрю осцилографом.
	//так же это UART Tx(для отладки)
	GPIOD->DDR |= GPIO_PIN_5;
	GPIOD->CR1 |= GPIO_PIN_5;
	GPIOD->ODR &= ~GPIO_PIN_5;

	/* Initialization of the clock */
	CLK_Config();

	//http://ziblog.ru/2011/07/31/rabotaem-s-ik-pultom.html
	//http://ziblog.ru/2013/05/14/distantsionnoe-upravlenie-ot-ik-pulta.html
	//http://www.count-zero.ru/2016/stm8_spl_pwm/ - Общие данные о таймерах на stm8

	/* IR импульс(длительностью 0.56ms) нужно подавать на IR диод шим сигналом с частотой 38Khz.
		 Это называется частотная модуляция. Не достаточно просто подать логическую единицу а потом
		 в конце 0. Приемник такой сигнал не примет!
		 Нужен именно шим сигнал! Для 0.56ms это ~ 21 такт шима на 38Khz.
		 Шим сигнал у нас генерируется TIM2_CH1. Выход PD4. */

	/* подключать таймер к шине тактирования. это обычно делают в CLK_Config */
	CLK_PeripheralClockConfig(CLK_PERIPHERAL_TIMER2, ENABLE);
	TIM2_DeInit(); //ресет таймера
	/* устанавливается частота работы таймера. второй параметр это период таймера.
		 то есть частота генерируемого сигнала. отсчет счетчика от 0, поэтому отнимаем 1. */
	//52 - 1 при DIV_8 это ~ 38 khz. 16Mhz / 8 = 2000Khz. 2000Khz / 52 ~ 38Khz
	//52 это 0.0264ms
	//0.56ms это примерно 21-н проход таймера на частоте 38Khz(0.0264 * 21 ~ 0.56ms)
	TIM2_TimeBaseInit(TIM2_PRESCALER_8, KHZ_38 - 1);
	//первый полупериод шим сигнала. включаем генерацию шим сигнала на PD4.
	/* тут определяется длина(KHZ_38_HALF) LOW составляющей шим сигнала. полярность специально задана
		 как LOW и если мы делаем TIM2_SetCompare1(0) - задаем 0-ю продолжительность второй
		 половины(HIGH) то весь шим сигнал становится == 0(1-я половина занимает все что
		 не занято второй половиной). */
	TIM2_OC1Init(TIM2_OCMODE_PWM2, TIM2_OUTPUTSTATE_ENABLE, KHZ_38_HALF, TIM2_OCPOLARITY_LOW);
	//второй полупериод шим сигнала(по продолжительности такой же как и первый)
	TIM2_SetCompare1(0); //пока что ВЫКЛ. пеперачу включит код в прерывании таймера.
	/* включение генерации прерывания для 2-го(CC2) канала таймера.
	 	 обработчик прерываний будет вызываться каждые 38Khz. */
	TIM2_ClearFlag(TIM2_FLAG_CC2);
	TIM2_ITConfig(TIM2_IT_CC2, ENABLE);
	//разрешаем прерывания
	enableInterrupts();
#ifdef UART
	//UART1_DeInit(); //и так работает!
	MY_UART1_Init((uint32_t)115200, UART1_WORDLENGTH_8D, UART1_STOPBITS_1, UART1_PARITY_NO,
		UART1_SYNCMODE_CLOCK_DISABLE);
	//printf использовать можно но очень уд жорого получается(по занимаемому месту!)
	strsend("OwL is READY!\n\r");
#endif /* UART */
	therm_init_mode(THERM_MODE_12BIT); //инит термодатчика
	while(1){
		//0x249 это порог срабатывания по напряжению(чуть ниже 3.3v)
		now_voltage = readADC1(2);
		now_state = now_voltage > STATUS_PIN_VCC_TRIG_THRE ? 1 : 0;
		if(now_state != last_state){
			last_state = now_state;
			prot = 1;
		}
		if(prot > 0)
			prot++;
		//защита от флапания. если now_state продержится 100000 циклов то начинаем реагировать
		if(prot > 100000){
			prot = 0;
			if(now_state){
				strsend("now_state = On(0x");
				u16_print(now_voltage);
				strsend(")\n\r");
				GPIOB->ODR &= ~GPIO_PIN_5; //вкл лампочка test
				//GPIOC->ODR |= GPIO_PIN_5; //вкл вентилятор. это сделает код контроля термодатчика.
			}else{
				strsend("now_state = OFF(0x");
				u16_print(now_voltage);
				strsend(")\n\r");
				GPIOB->ODR |= GPIO_PIN_5; //вЫкл лампочка test
				GPIOC->ODR &= ~GPIO_PIN_5; //вЫкл вентилятор
			}
			//врубаем таймер для IR импульса
			tim2_pre_start_init();
			TIM2_Cmd(ENABLE);
		}
		/* читаем температуру, только если в данный момент нет передачи IR импульсов т.к.
			 чтение температуры вырубает прерывания которые использует таймер передачи IR импульсов.
			 читаем температуру только при состоянии вкл.
			 0xAFFFF - примерно каждые 5 сек. */
		if(now_state && temperature_need_read++ > 0xAFFFF && is_ir_in_progress == 0){
			temperature = GetTemperature();
			strsend("temperature = ");
			u8_print(temperature);
			strsend("h\n\r");
			if(temperature >= FAN_NEED_ON_TEMPERATURE)
				GPIOC->ODR |= GPIO_PIN_5; //вкл вентилятор
			else
				GPIOC->ODR &= ~GPIO_PIN_5; //вЫкл вентилятор
			temperature_need_read = 0x0;
		}
	}
}

/* 2 + 32 */
#define DELAYS_COUNT 34
/* массив уже рассчитанных ранее задержек активности(старшие 4 бита)
	 и молчания(младшие 4-ре бита) для IR битов. 0-й элемент всегда 0,
	 1-й элемент это 9ms импульс + 4.5ms молчания, остальные(2..34)
	 это инфо биты(32 штуки).

	 так как 0-й задержки быть не может то 0 это 1 задержка(0.56ms).
	 соответственно для всех инфо битов старшие 4-ре бита всегда == 0
	 так как длительность IR импульса всегда 0.56ms).

	 массив генерирует ф-я fill_delays_from_ir_code(uint32_t code)
	 из stm8_ir_receiver/gen_phases/gen.c */
/* static uint8_t delays[DELAYS_COUNT] = {
	0x0, 0xF7,
	00, 00, 00, 02, 00, 00, 00, 00, 02, 02, 02, 00, 02, 02, 02, 02,
	02, 02, 00, 02, 02, 00, 00, 02, 00, 00, 02, 00, 00, 02, 02, 00,
}; */
static uint8_t delays[DELAYS_COUNT] = {
	00, 0xF7,
	00, 00, 00, 00, 00, 00, 02, 00, 02, 02, 02, 02, 02, 02, 00, 02,
	00, 02, 00, 00, 02, 00, 00, 00, 02, 00, 02, 02, 00, 02, 02, 02
};
static volatile uint8_t delays_index; //текущий индекс бита задержки
static volatile uint16_t pc; //pass counter - смчетчик проходов таймера
//значения лимитов для pc(когда выключать передачу IR сигнала а когда включать)
static volatile uint16_t lim1, lim2;
//нужно дать ~последний~ импульс(0.56ms) и отсановить таймер
static volatile uint8_t last_imp;

//преинит таймерных переменных перед стартом TIM2
void tim2_pre_start_init(void){
	is_ir_in_progress = 1;
	pc = 0;
	delays_index = 0;
	/* при старте таймера передача выключена и тут устанавливаем значения заглушек
	 	 чтобы сразу ее включить. при этих значениях сразу выполнится и
	 	 (pc == lim1) и (pc == lim2) код. */
	lim1 = 1;
	lim2 = 1;
	last_imp = 0;
}

/* Обработчик прерывания от TIM2 */
//тут все на регистрах. ищи в StdPeriph_Lib-е эти регистры чтобы понимать что они делают.
INTERRUPT_HANDLER(TIM2_CAP_COM_IRQHandler, 14){
	if(TIM2->SR1 & TIM2_SR1_CC2IF){ //2-й канал таймера
		/* для отладки можно дергать эту ножку и смотреть на осцилограф.
			 осцилограф покажет 19.2Khz(пусть тебя это не удивляет) так как мы
			 каждым рывком этого обработчика прерываний(раз в 38.4Khz)
			 генерируем полупериод для этого сигнала на пине PD5. */
		//GPIOD->ODR ^= GPIO_PIN_5;
		pc++;
		/* период передачи IR сигнала(0.56ms) закончился(отработали) */
		if(pc == lim1){
			TIM2_SetCompare1(0); //вырубаем передачу IR сигнала
			//если это был последний импульс
			if(last_imp){
				//вырубаем таймер
				TIM2_Cmd(DISABLE);
				tim2_pre_start_init();
				is_ir_in_progress = 0;
			}
		}
		/* периоды передачи сигнала и молчания для IR бита отработали */
		if(pc == lim2){ //переходим к следующему биту(задержке)
			delays_index++;
			//если еще есть биты(задержки)
			if(delays_index < DELAYS_COUNT){
				lim1 = delays[delays_index] >> 4;
				lim2 = delays[delays_index] & 0xF;
				lim1++;
				lim2++;
				lim1 *= DOT_56;
				lim2 *= DOT_56;
				lim2 += lim1; //lim2 начинается после lim1
			}else{ //все биты(задержки) былы отработаны
				//осталось дать последний 0.56ms IR импульс(без последующей задержки!)
				last_imp = 1;
				lim1 = DOT_56;
			}
			pc = 0;
			//врубаем передачу IR сигнала
			TIM2_SetCompare1(KHZ_38_HALF);
		}
		//очистка бита ожидания прерывания(аналог TIM2_ClearFlag)
		TIM2->SR1 = (uint8_t)(~TIM2_SR1_CC2IF);
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
