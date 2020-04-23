#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#define FRAMESIZE 500
#define	INIT_CCITT	-1
typedef unsigned char byte;
byte frame[FRAMESIZE];

 /* nibble table for CCITT crc */                                                
static unsigned int ccitt_h[] = {                                               
0x0000, 0x1081, 0x2102, 0x3183, 0x4204, 0x5285, 0x6306, 0x7387,                 
0x8408, 0x9489, 0xa50a, 0xb58b, 0xc60c, 0xd68d, 0xe70e, 0xf78f, };              
                                                                                  
static unsigned int ccitt_l[] = {                                               
0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,                 
0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7, }; 


unsigned short checksum_ccitt(unsigned char * in, int length)
{
	register unsigned int	n, crc;

	crc = INIT_CCITT;
	while (length-- > 0) {
		n = *in++ ^ crc;
		crc = ccitt_l[n&0x0f] ^ ccitt_h[(n>>4)&0x0f] ^ (crc>>8);
	};
	return((unsigned short)crc);
}

void corrupt_frame(byte* frame, int framesize) {
    int index;
    while(true) {
        index = rand() % framesize;
        if(frame[index] != frame[index + 1]) {
            byte save = frame[index];
            frame[index] = frame[index + 1];
            frame[++index] = save;
            break;
        } 
    }
}

int generate_naive_checksum(byte* arr, int arr_len) {
    int sum = 0;
    byte* ptr = arr;
    for(int i = 0; i < arr_len; ++i) {
        sum += *ptr;
        arr++;
    }
    return(sum); 
} 

byte* copy_array(byte* arr, int len) {
    byte* newarr = malloc(sizeof(byte) * len);
    for(int i = 0; i < len; ++i) {
        newarr[i]  = *arr;
        arr++;
    }
    return(newarr);
}

int main(int argcount, char *argvalue[])
{
    srand(getpid());  
    for(int i = 0; i < FRAMESIZE; ++i) {
        frame[i] = rand() % 256;
    }
    unsigned short checksum_before = checksum_ccitt(frame, FRAMESIZE);
    int naive_checksum_before = generate_naive_checksum(frame, FRAMESIZE);
    corrupt_frame(frame, FRAMESIZE);
//    corrupt_frame(frame, FRAMESIZE);
    unsigned short checksum_after = checksum_ccitt(frame, FRAMESIZE);
    int naive_checksum_after = generate_naive_checksum(frame, FRAMESIZE);
    printf("Checksum before %d\nNaive checksum before %d\nNaive checksum after%d\nChecksum after %d\n", checksum_before,naive_checksum_before, naive_checksum_after, checksum_after);
    return 0;
}

