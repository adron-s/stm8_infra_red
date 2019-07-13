CC=sdcc --opt-code-size
CFLAGS=-mstm8
#STDPER_LIB=/home/prog/stm8/stm8pack/lib/STM8S_StdPeriph_Lib_V2.1.0
STDPER_LIB=/home/prog/stm8/STM8S_StdPeriph_Lib
STDPER_LIB_SRC=$(STDPER_LIB)/Libraries/STM8S_StdPeriph_Driver/src
INCLUDEPATH=$(STDPER_LIB)/Libraries/STM8S_StdPeriph_Driver/inc/
DEFINES= STM8S103
OUTPUT_DIR=build
SOURCE=leds
all: flash

#https://www.ondrovo.com/a/20170107-stm8-getting-started/
compile:
	mkdir -p $(OUTPUT_DIR)
#те что закоментиованы - один раз нужно скомпилить! это сама библиотека StdPeriph
#	$(CC) $(CFLAGS) -I $(INCLUDEPATH) -I ./ -D $(DEFINES) -o $(OUTPUT_DIR)/ $(STDPER_LIB_SRC)/stm8s_uart1.c -c
#	$(CC) $(CFLAGS) -I $(INCLUDEPATH) -I ./ -D $(DEFINES) -o $(OUTPUT_DIR)/ $(STDPER_LIB_SRC)/stm8s_clk.c -c
#	$(CC) $(CFLAGS) -I $(INCLUDEPATH) -I ./ -D $(DEFINES) -o $(OUTPUT_DIR)/ $(STDPER_LIB_SRC)/stm8s_gpio.c -c
#	$(CC) $(CFLAGS) -I $(INCLUDEPATH) -I ./ -D $(DEFINES) -o $(OUTPUT_DIR)/ $(STDPER_LIB_SRC)/stm8s_tim2.c -c
#компил файла с ф-ями обработчиков прерываний
#	$(CC) $(CFLAGS) -I $(INCLUDEPATH) -I ./ -D $(DEFINES) -o $(OUTPUT_DIR)/ stm8s_it.c -c
	$(CC) $(CFLAGS) -I $(INCLUDEPATH) -I ./ -D $(DEFINES) -o $(OUTPUT_DIR)/ main.c -c
	$(CC) $(CFLAGS)	-D $(DEFINES) -o $(OUTPUT_DIR)/$(SOURCE).ihx \
	$(OUTPUT_DIR)/stm8s_it.rel \
	$(OUTPUT_DIR)/stm8s_tim2.rel \
	$(OUTPUT_DIR)/stm8s_clk.rel \
	$(OUTPUT_DIR)/main.rel

#$(OUTPUT_DIR)/stm8s_uart1.rel \
#$(OUTPUT_DIR)/stm8s_clk.rel \
#$(OUTPUT_DIR)/stm8s_gpio.rel \

prepare:
	packihx $(OUTPUT_DIR)/$(SOURCE).ihx > $(OUTPUT_DIR)/$(SOURCE).hex

clean:
	rm -Rrf $(OUTPUT_DIR)

flash: compile
	stm8flash -c stlinkv2 -p stm8s103?3 -w $(OUTPUT_DIR)/leds.ihx
