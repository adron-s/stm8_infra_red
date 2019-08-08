#ifndef __UART_MINI_H
#define __UART_MINI_H

/* минимальная реализация под STM8 инита UART-а и вывод через него строк текста и hex чисел.
	 используется для отладки. реализован только Tx. */

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
#else
#define strsend(arg) { }
#define u8_print(arg) { }
#define u16_print(arg) { }
#define u32_print(arg) { }
#endif /* UART */
#endif /* __UART_MINI_H */
