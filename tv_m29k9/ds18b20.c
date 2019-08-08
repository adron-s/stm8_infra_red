#include "stm8s.h"
#include "ds18b20.h"

/* реализация работы с термодатчиком ds18b20 для stm8.
	 код взят отсюда: http://hex.pp.ua/stm8-1wire-ds18b20.php
	 и немного доработан */

static void us_xz_delay(uint8_t t){
	while(t--) nop();
}
static void us_xz_delay2(uint16_t t){
	while(t--) nop();
}
/* задержки калибрувались с помощью осцилографа ногодрыгом
	 в while(1){ ... } для stm8s103 */
#define delay_1us() nop();
#define delay_15us() us_xz_delay(30);
#define delay_45us() us_xz_delay(98);
#define delay_60us()  us_xz_delay(133);
#define delay_480us() us_xz_delay2(762);

#define THERM_INPUT_MODE()  THERM_PORT->DDR &= ~THERM_PIN
#define THERM_OUTPUT_MODE() THERM_PORT->DDR |=  THERM_PIN
#define THERM_LOW()         THERM_PORT->ODR &= (uint8_t)(~THERM_PIN)
#define THERM_HIGH()        THERM_PORT->ODR |= (uint8_t)THERM_PIN
#define THERM_READ()        (THERM_PORT->IDR & (volatile uint8_t)THERM_PIN)
//volatile char iCurrentTemp = 0;
// Команды
#define THERM_CMD_CONVERTTEMP 0x44
#define THERM_CMD_RSCRATCHPAD 0xBE
#define THERM_CMD_WSCRATCHPAD 0x4E
#define THERM_CMD_CPYSCRATCHPAD 0x48
#define THERM_CMD_RECEEPROM 0xB8
#define THERM_CMD_RPWRSUPPLY 0xB4
#define THERM_CMD_SEARCHROM 0xF0
#define THERM_CMD_READROM 0x33
#define THERM_CMD_MATCHROM 0x55
#define THERM_CMD_SKIPROM 0xCC
#define THERM_CMD_ALARMSEARCH 0xEC
 
#define THERM_DECIMAL_STEPS_12BIT 625 //0625
//Режимы работы. По-умолчанию 12 бит

/**
  * @brief Сброс термодатчика
  * @par
  * Параметры отсутствуют
  * @retval
  * false - Неисправность; true - Норма
*/
uint8_t therm_reset(void){
  unsigned char i = 0xFF;
  THERM_OUTPUT_MODE();
  THERM_LOW();
  delay_480us();
  THERM_INPUT_MODE();
  delay_60us();
  i = THERM_READ();
  delay_480us();
  // 0 означает правильный сброс, 1 - ошибка
  return (0 == i) ? 1 : 0;
}
/**
  * @brief Запись бита
  * @param[in] bBit бит
  * @retval
  * Возвращаемое значение отсутствует
*/
void therm_write_bit(uint8_t bBit){
  THERM_OUTPUT_MODE();
  THERM_LOW();
  delay_1us();
  if(bBit){
    THERM_INPUT_MODE();
  }
  delay_60us();
  THERM_INPUT_MODE();
}
/**
  * @brief Чтение бита
  * @par
  * Параметры отсутствуют
  * @retval
  * Значение бита.
*/
uint8_t therm_read_bit(void){
  uint8_t bBit = 0;
  THERM_OUTPUT_MODE();
  THERM_LOW();
  delay_1us();
  THERM_INPUT_MODE();
  delay_15us();
  if(THERM_READ()){
    bBit = 1;
  }
  delay_45us();
  return bBit;
}
/**
  * @brief Чтение байта
  * @par
  * Параметры отсутствуют
  * @retval
  * Значение байта.
*/
static unsigned char therm_read_byte(void){
  unsigned char i = 8;
  unsigned char n = 0;
  while (i--){
    // Сдвинуть на одну позицию вправо и сохранить значение бита
    n >>= 1;
    n |= (therm_read_bit() << 7);
  }
  return n;
}
/**
  * @brief Запись байта
  * @param[in] byte байт
  * @retval
  * Возвращаемое значение отсутствует
*/
void therm_write_byte(unsigned char byte){
  unsigned char i = 8;
  while (i--){
    // Записать текущий бит и сдвинуть на 1 позицию вправо
    // для доступа к следующему биту
    therm_write_bit(byte & 1);
    byte >>= 1;
  }
}
/**
  * @brief Установить режим работы термодатчика
  * @param[in] mode Режим работы
  * @retval
  * Возвращаемое значение отсутствует
*/
void therm_init_mode(THERM_MODE mode){
  therm_reset();
  therm_write_byte(THERM_CMD_SKIPROM);
  therm_write_byte(THERM_CMD_WSCRATCHPAD);
  therm_write_byte(0);
  therm_write_byte(0);
  therm_write_byte(mode);
}
/**
  * @brief Чтение температуры
  * @par
  * Параметры отсутствуют
  * @retval
  * Температура
*/
char GetTemperature(void){
  char iResult = 0;
  unsigned char temperature[2] = {0, 0};
  unsigned char digit;
  short iReadLimit;
  // Запрет прерываний во время опроса датчика
  disableInterrupts();
  // Сброс и сразу переход к преобразованию температуры
  iResult = therm_reset();
  therm_write_byte(THERM_CMD_SKIPROM);
  therm_write_byte(THERM_CMD_CONVERTTEMP);
  // Ожидание завершения преобразования
  iReadLimit = 10;
  while (!therm_read_bit() && (--iReadLimit > 0)) { ; }
  // Сброс и чтение байт температуры
  therm_reset();
  therm_write_byte(THERM_CMD_SKIPROM);
  therm_write_byte(THERM_CMD_RSCRATCHPAD);  
  temperature[0] = therm_read_byte();
  temperature[1] = therm_read_byte();
  // Разрешить прерывания
  enableInterrupts();
  digit = 0;
  digit  = (  temperature[0]         >> 4 ) & 0x0F;
  digit |= ( (temperature[1] & 0x0F) << 4 ) & 0xF0;
  // Отрицательная температура - инвертировать и прибавить 1
  if (temperature[1] & 0x80){
    iResult = ~digit;
    iResult++;
  } else{
    iResult = digit;
  }
  //iCurrentTemp = iResult;
  return iResult;
}
