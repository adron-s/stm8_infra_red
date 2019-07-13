#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

int main(int argc, char *argv[]){
	int a;
	uint32_t phases = 0x26666667;
	if(phases >> 30 != 0x0)
		printf("ALERT1\n");
	//2 последних бита были затерты и не имеют значения
	phases >>= 2;
	for(a = 0; a < 15; a++){
		uint32_t x = phases >> ((15 - a - 1) * 2);
		printf("%d\n", x & 0x3);
	}
	return 0;
}
