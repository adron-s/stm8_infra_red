#include "stm8s.h"
#include "ir_decoder.h"

struct ir_decoder_type ir_decoder;

void strsend(char *str);
void u8_print(uint8_t dig);
void u16_print(uint16_t dig);
void u32_print(uint32_t dig);

//------------------------------------------------------------------------------
void ir_decoder_init(void){
	uint8_t *p = (void*)&ir_decoder;
	uint32_t a;
	for(a = 0; a < sizeof(ir_decoder); a++, p++)
		*p = 0x0;
}

//------------------------------------------------------------------------------
void ir_decoder_refresh(void){
	uint16_t timer_delay;
	if(ir_decoder.is_received)
		return;
	//считываем длительность импульса в тиках таймера
	//аналог TIM2_GetCapture2(сколько насчитал таймер при срабатывании TIM2_CH2)
	timer_delay = TIM2->CCR2H << 8;
	timer_delay += TIM2->CCR2L;
	//запоминаем длительность каждого полученого импульса
	if(ir_decoder.index < IR_DECODER_COMMAND_LENGHT_MAXIMUM)
		ir_decoder.delays[ir_decoder.index++] = timer_delay;
}

//------------------------------------------------------------------------------
void ir_decoder_refresh_timeout(void){
	ir_decoder.is_received = 1;
}

//------------------------------------------------------------------------------
//считает 32-х битное число по задержкам принятых импульсов
/* Используется относительное кодирование, т. е. сравнивая длительности предыдущего
	 и текущего принятого импульса выносится решение о кодировании текущего импульса
	 как «0» или «1». Если текущий импульс короче или длиннее предыдущего, то кодируем
	 текущий импульс как «1», иначе как «0».
	 http://ziblog.ru/2013/05/14/distantsionnoe-upravlenie-ot-ik-pulta.html */
uint32_t calc_32bit_ir_code(void){
	uint8_t a;
	uint8_t end = ir_decoder.index;
	uint32_t res = 0;
	uint16_t timer_delay;
	uint16_t min = 0;
	uint16_t max = 0;

	/* не используем в рассчете код повтора(обычно он все таки принимается вместе
		 с основным пакетом) его видно по огромной задержке. */
	if(end - IR_DECODER_NUMBER_OF_MISSING_INTERVAL > 32)
		end = 32 + IR_DECODER_NUMBER_OF_MISSING_INTERVAL;
	for(a = IR_DECODER_NUMBER_OF_MISSING_INTERVAL; a < end; a++){
		timer_delay = ir_decoder.delays[a];
		//пропуск 31-го(самого 1-го, старшего) бита т.к. он NEC кодировке он всегда 0.
		//самый первый бит посылки используется только для сравнения и не учитывается.
		if(a > IR_DECODER_NUMBER_OF_MISSING_INTERVAL){
			res <<= 1;
			if(timer_delay < min || timer_delay > max){
				res++;
			}
		}
		//запоминаем интервал
		min = timer_delay;
		max = timer_delay;
		//рассчитываем отклонение(так как полностью одинаковые импульсы быть не могут!)
		timer_delay *= IR_DECODER_INTERVAL_REFERENCE_PERCENT;
		timer_delay /= 100;
		//устанавливаем границы
		max += timer_delay;
		min -= timer_delay;
	}
	return res;
}

//------------------------------------------------------------------------------
//печатает все накопленные задержки
/* DIV_128 | 125Khz | 0x07B | 123 | 123 * 0.008ms = 0,984ms ~ 1Khz
	 у нас в итоге две основные задержки: 0x00D1 и 0x0044:
	 	0x00D1 | 209 | 209 * 0.008ms = 1,672ms | отклонение 0,018 или 1.0%
	 	0x0044 | 068 | 068 * 0.008ms = 0,544ms | отклонение 0,016 или 2.9%
	 исходя из этого: https://www.sbprojects.net/knowledge/ir/nec.php
	 	в идеале должно быть
	 		или 2.25 - 0.56 = 1,69 = 0xD3
	 		или 1.12 - 0,56 = 0.56 = 0x46!
	 то есть довольно близкие значения!
	 померял так же и длину импульса(поменяв RISING и FALLING местами):
	 	0048 .. 004C. То есть от 72 до 76 или 0,576ms до 0,608ms.
*/
void print_ir_delays(void){
	uint8_t a;
	uint8_t k = 0;
	uint16_t timer_delay;

	for(a = 0; a < ir_decoder.index; a++){
		timer_delay = ir_decoder.delays[a];
		//пропуск для подсчета res первых N интервалов
		if(a == IR_DECODER_NUMBER_OF_MISSING_INTERVAL){
			strsend("\n\r");
			k = 0;
		}
		u16_print(timer_delay); k++;
		if(k == 16){
			k = 0;
			strsend("\n\r");
		}else{
			strsend(" ");
		}
	}
	strsend("phases: ");
	u32_print(ir_decoder.phases);
	strsend("\n\r");
}
