#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>

#define	POLY_CRC16	0xa001
#define	INIT_CRC16	0
#define	INIT_CCITT	-1
#define FRAMESIZE 1000
typedef unsigned char byte;
byte frame[FRAMESIZE];

 /* nibble table for CCITT crc */                                                
static unsigned int ccitt_h[] = {                                               
0x0000, 0x1081, 0x2102, 0x3183, 0x4204, 0x5285, 0x6306, 0x7387,                 
0x8408, 0x9489, 0xa50a, 0xb58b, 0xc60c, 0xd68d, 0xe70e, 0xf78f, };              
                                                                                  
static unsigned int ccitt_l[] = {                                               
0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,                 
0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7, }; 

int64_t get_microseconds(void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return(now.tv_sec * 1000000 + now.tv_usec);
}

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


int checksum_internet(unsigned short *ptr, int nbytes)
{
    long                sum;
    unsigned short      oddbyte, answer;

    sum = 0L;
    while(nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }

    if(nbytes == 1) {   /* mop up an odd byte if necessary */
        oddbyte = 0;    /* make sure that the top byte is zero */
        *((unsigned char *)&oddbyte) = *(unsigned char *)ptr; /* 1 byte only */
        sum += oddbyte;
    }

    /* Now add back carry outs from top 16 bits to lower 16 bits */
    sum = (sum >> 16) + (sum & 0xffff); /* add hi-16 to lo-16 */
    sum += (sum >> 16);                 /* add carry bits */
    answer  = ~sum;     /* one's complement, then truncate to 16 bits */

    return(answer);
}

unsigned short checksum_crc16(unsigned char *in, int length)
{
	register unsigned short crc, bit;
	register int i;

	crc = INIT_CRC16;
	while(length-- > 0) {
		for (i=1; i <= 0x80; i<<=1) {
			bit = (((*in)&i)?0x8000:0);
			if (crc & 1) bit ^= 0x8000;
			crc >>= 1;
			if (bit) crc ^= (int)(POLY_CRC16);
  		}
		++in;
	}
	return crc;
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

int get_file_size(char* filename) {
    struct stat st;
    stat(filename, &st);
    return(st.st_size);
}

unsigned short* read_shorts_from_file(char* filename) {
    FILE* fd = fopen(filename, "r");
    unsigned short* frame;
    if(fd == NULL) {
        printf("Couldn't find file %s\n",filename);
        return(NULL);
    }
    int size = get_file_size(filename);
    if(size < 0) {
        printf("File read error, exiting\n");
        return(NULL);
    }
    frame = malloc(size * sizeof(unsigned short));
    if(frame == NULL) {
        printf("Failed to allocate memory, exiting\n");
        return(NULL);
    }
    fread(frame, size, 1, fd);
    return(frame);
    
}

byte* read_bytes_from_file(char* filename) {
    FILE* fd = fopen(filename, "r");
    byte* frame;
    if(fd == NULL) {
        printf("Couldn't find file %s\n",filename);
        return(NULL);
    }
    int size = get_file_size(filename);
    if(size < 0) {
        printf("File read error, exiting\n");
        return(NULL);
    }
    frame = malloc(size * sizeof(byte));
    if(frame == NULL) {
        printf("Failed to allocate memory, exiting\n");
        return(NULL);
    }
    fread(frame, size, 1, fd);
    return(frame);
}
void test_timings(void) {
    char* filepath = "../../roms.zip";
    int64_t start;
    int size = get_file_size(filepath);
    byte* frame = read_bytes_from_file(filepath);
    unsigned short* short_frame  = read_shorts_from_file(filepath);
    if(size < 0) {
        return;
    }

    printf("Testing execution time of naive checksum\n");
    start = get_microseconds();
    generate_naive_checksum(frame, size);
    printf("Execution time for naive checksum: %lu usecs\n", get_microseconds() - start);
    

    printf("Testing CCIT Checksum: \n");
    start = get_microseconds();
    checksum_ccitt(frame, size); 
    printf("Execution time for ccitt checksum: %lu usecs\n", get_microseconds() - start);       
    
    printf("Testing execution time of checksum crc16 (byte by byte reading): \n");
    start = get_microseconds();    
    checksum_crc16(frame, size);
    printf("Execution time for crc16: %lu usecs\n", get_microseconds() - start);

    printf("Testing internet checksum: \n");
    start = get_microseconds();
    checksum_internet(short_frame, size);
    printf("Execution tiem for internet checksum: %lu usecs\n", get_microseconds() - start);
    
    free(short_frame);
    free(frame);
}

int main(int argcount, char *argvalue[])
{
    srand(getpid());  
    test_timings();
    return 0;
}

