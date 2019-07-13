#ifndef IR_DECODER_H_
#define IR_DECODER_H_

#define DELAYS_DEBUG

//кол-во пропускаемых импульсов вначале посылки
#define IR_DECODER_NUMBER_OF_MISSING_INTERVAL 2UL
//процентное отклонение для эталонного импульса (в процентах)
#define IR_DECODER_INTERVAL_REFERENCE_PERCENT 20UL
//длительность одного тика таймера (мкс)
#define IR_DECODER_TIMER_TICK 8UL
//максимальная длина посылки (бит) [используется для работы 32 бита]
#define IR_DECODER_COMMAND_LENGHT_MAXIMUM 64UL

struct ir_decoder_type {
	volatile uint8_t is_received;
	volatile uint8_t index;
	volatile uint16_t delays[IR_DECODER_COMMAND_LENGHT_MAXIMUM];
	volatile uint32_t phases;
	volatile uint32_t phases_count;
};

extern struct ir_decoder_type ir_decoder;

void ir_decoder_init(void);
void ir_decoder_refresh(void);
void ir_decoder_refresh_timeout(void);
uint32_t calc_32bit_ir_code(void);
void print_ir_delays(void);

#endif /* IR_DECODER_H_ */
