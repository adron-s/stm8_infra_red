#ifndef __DS18B20_H
#define __DS18B20_H

#include "stm8s.h"
#include "stdio.h"

//термодатчик подключен к B4
#define THERM_PORT GPIOB
#define THERM_PIN  GPIO_PIN_4

typedef enum{
  THERM_MODE_9BIT  = 0x1F,
  THERM_MODE_10BIT = 0x3F,
  THERM_MODE_11BIT = 0x5F,
  THERM_MODE_12BIT = 0x7F
} THERM_MODE;

void therm_init_mode(THERM_MODE mode);
char GetTemperature(void);

#endif /* __DS18B20_H */
