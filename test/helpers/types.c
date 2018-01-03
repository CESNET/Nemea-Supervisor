#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

int main() {
   printf("sizeof(size_t)=%d\n", sizeof(size_t));
   printf("sizeof(int)=%d\n", sizeof(int));
   printf("sizeof(unsigned int)=%d\n", sizeof(unsigned int));
   printf("sizeof(uint8_t)=%d\n", sizeof(uint8_t));
   printf("sizeof(uint16_t)=%d\n", sizeof(uint16_t));
   printf("sizeof(uint32_t)=%d\n", sizeof(uint32_t));
   printf("sizeof(unsigned long int)=%d\n", sizeof(unsigned long int));
   printf("sizeof(uint64_t)=%d\n", sizeof(uint64_t));
   return 0;
}

/*

sizeof(size_t)=8
sizeof(int)=4
sizeof(unsigned int)=4
sizeof(uint8_t)=1
sizeof(uint16_t)=2
sizeof(uint32_t)=4
sizeof(unsigned long int)=8
sizeof(uint64_t)=8

 */