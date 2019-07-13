#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

//2 + 32 bit
#define IR_DELAYS 34
static uint8_t ir_index = IR_DELAYS;
static uint8_t ir_delays[IR_DELAYS];

#define IR_DECODER_NUMBER_OF_MISSING_INTERVAL 2UL
#define IR_DECODER_INTERVAL_REFERENCE_PERCENT 20UL
//------------------------------------------------------------------------------
//считает 32-х битное число по задержкам принятых импульсов
/* Используется относительное кодирование, т. е. сравнивая длительности предыдущего
	 и текущего принятого импульса выносится решение о кодировании текущего импульса
	 как «0» или «1». Если текущий импульс короче или длиннее предыдущего, то кодируем
	 текущий импульс как «1», иначе как «0».
	 http://ziblog.ru/2013/05/14/distantsionnoe-upravlenie-ot-ik-pulta.html */
uint32_t calc_32bit_ir_code(void){
	uint8_t a;
	uint8_t end = ir_index;
	uint32_t res = 0;
	uint16_t timer_delay;
	uint16_t min = 0;
	uint16_t max = 0;

	/* не используем в рассчете код повтора(обычно он все таки принимается вместе
		 с основным пакетом) его видно по огромной задержке. */
	if(end - IR_DECODER_NUMBER_OF_MISSING_INTERVAL > 32)
		end = 32 + IR_DECODER_NUMBER_OF_MISSING_INTERVAL;
	for(a = IR_DECODER_NUMBER_OF_MISSING_INTERVAL; a < end; a++){
		timer_delay = ir_delays[a];
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

//заполняет массив задержек IR битов(NEC: 2 + 32)
void fill_delays_from_ir_code(uint32_t code){
	const uint8_t pd = 0x46; //0.56ms for DIV_128
	uint8_t pred_bit = 0;
	uint8_t pred_period = 2; //1.12ms
	uint8_t bit, period;
	uint8_t a;
	ir_delays[0] = 0; //empty
	ir_delays[1] = (15 << 4) | 7; //16(9ms), 8(4.5ms)(16 * 0.56ms ~ 9ms) - start
	printf("%02X, 0x%02X,\n", ir_delays[0], ir_delays[1]);
	for(a = 0; a < 32; a++){
		bit = (code >> (31 - a)) & 0x1;
		//printf("%d\n", bit);
		period = pred_period;
		if(bit){ //если 1 то нужно изменить значение на противоположное
			if(period == 4)
				period = 2;
			else
				period = 4;
		}
		pred_bit = bit;
		pred_period = period;
		//обычный формат - в ir_delays[x] сохраняется премя молчания в тиках
		//ir_delays[a + 2] = (period - 1) * pd;
		/* машинный формат. старшие 4 бита - сколько длится период передачи
			 (в тактах = 0.56ms. начиная с 0(0 это 0.56))
			 остальные 4 бита - сколько длится период молчания(1..3). */
		//тут всегда старшие 4 бита == 0(1 тик длиной 0.56ms)
		ir_delays[a + 2] = (period - 2);
		//printf("%d - %04X\n", bit, ir_delays[a + 2]);
		printf("%02X, ", ir_delays[a + 2]);
		if(a == 15)
			printf("\n");
	}
	printf("\n");
}

int main(int argc, char *argv[]){
	const uint32_t code = 0x189835B5;
	fill_delays_from_ir_code(code);
	//printf("\n");
	//printf("IR code = %X\n", calc_32bit_ir_code());
	return 0;
}
