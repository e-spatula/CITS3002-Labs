#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define N_TRIALS 1000000
#define FRAMESIZE 1000
#define INIT_CCITT -1
typedef unsigned char byte;
#define NBBY 8
#define MIN_BURSTLENGTH         10
#define MAX_BURSTLENGTH         100

/* nibble table for CCITT crc */
static unsigned int ccitt_h[] = {
    0x0000, 0x1081, 0x2102, 0x3183, 0x4204, 0x5285, 0x6306, 0x7387,
    0x8408, 0x9489, 0xa50a, 0xb58b, 0xc60c, 0xd68d, 0xe70e, 0xf78f,
};

static unsigned int ccitt_l[] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
};

unsigned short checksum_ccitt(unsigned char *in, int length) {
  register unsigned int n, crc;

  crc = INIT_CCITT;
  while (length-- > 0) {
    n = *in++ ^ crc;
    crc = ccitt_l[n & 0x0f] ^ ccitt_h[(n >> 4) & 0x0f] ^ (crc >> 8);
  };
  return ((unsigned short)crc);
}

void corrupt_frame(byte *frame, int framesize) {
  int index;
  while (true) {
    index = rand() % framesize;
    if (frame[index] != frame[index + 1]) {
      byte save = frame[index];
      frame[index] = frame[index + 1];
      frame[++index] = save;
      break;
    }
  }
}

unsigned short generate_naive_checksum(byte *arr, int arr_len) {
  unsigned short sum = 0;
  byte *ptr = arr;
  for (int i = 0; i < arr_len; ++i) {
    sum += *ptr;
    arr++;
  }
  return (sum);
}

int get_file_size(char *filename) {
  struct stat st;
  stat(filename, &st);
  return (st.st_size);
}

byte *deep_copy(byte *arr, int len) {
  byte *new_ptr = malloc(sizeof(byte) * len);
  byte *return_ptr = new_ptr;
  if (new_ptr == NULL) {
    printf("Failed to allocate memory, exiting\n");
    return (return_ptr);
  }
  byte *temp_ptr = arr;
  for (int i = 0; i < len; ++i) {
    *new_ptr = *temp_ptr;
    new_ptr++;
    temp_ptr++;
  }
  return (return_ptr);
}

byte *read_bytes_from_file(char *filename) {
  FILE *fd = fopen(filename, "r");
  byte *frame;
  if (fd == NULL) {
    printf("Couldn't find file %s\n", filename);
    return (NULL);
  }
  int size = get_file_size(filename);
  if (size < 0) {
    printf("File read error, exiting\n");
    return (NULL);
  }
  frame = malloc(size * sizeof(byte));
  if (frame == NULL) {
    printf("Failed to allocate memory, exiting\n");
    return (NULL);
  }
  fread(frame, size, 1, fd);
  return (frame);
}

void burst_error(byte* frame, int length) {
    int nbits           = (length * NBBY);
    while(true) {
        int     b0      = rand() % nbits;
        int     b1      = rand() % nbits;
	int	burst	= b1 - b0;

        if(burst >= MIN_BURSTLENGTH && burst <= MAX_BURSTLENGTH) {
            for(int b=b0 ; b<b1 ; ++b) {
                int     byte    = b / NBBY;
                int     bit     = b % NBBY;

                frame[byte]     = (frame[byte] | (1UL << bit));
            }
            break;
        }
    }
}

void flip_random_bit(byte* frame, int size) {
    int byte= (rand() % size);
    frame[byte] = ~(frame[byte]);
}

void toggle_random_bit(byte* frame, int size) {
    int byte = (rand() % size);
    int bit = (rand() % NBBY);
    frame[byte] = (frame[byte] ^ (1UL << bit));
}

float test_success_rate(unsigned short (*fun_ptr)(), void (*cor_ptr)(), byte *frame, int size) {
  unsigned short orig_checksum = fun_ptr(frame, size);
  int failures = 0;
  unsigned short corrupt_checksum;
  for (int i = 0; i < N_TRIALS; ++i) {
    byte *frame_copy = deep_copy(frame, size);
    cor_ptr(frame_copy, size);
    corrupt_checksum = fun_ptr(frame_copy, size);
    if (orig_checksum == corrupt_checksum) {
      failures++;
    }
    free(frame_copy);
  }
  double result = failures;
  result /= N_TRIALS;
  printf("Failure rate was %.6f\n", result);
  return (result);
}

int main(int argcount, char *argvalue[]) {
  srand(getpid());
  /*
  int size = get_file_size("../../roms.zip");

  byte* frame = read_bytes_from_file("../../roms.zip");
   if(size == -1) {
      return(1);
  }
  */

  byte frame[FRAMESIZE];
  int size = FRAMESIZE;
  for (int i = 0; i < size; ++i) {
    frame[i] = rand() % 256;
  }

  unsigned short (*ccitt)(byte *, int) = checksum_ccitt;
  unsigned short (*naive)(byte *, int) = generate_naive_checksum;
  void (*cor_ptr)(byte*, int) = burst_error;
  test_success_rate(ccitt, cor_ptr, frame, size);
  test_success_rate(naive, cor_ptr, frame, size);
  return (0);
  unsigned short checksum_before = checksum_ccitt(frame, size);
  int naive_checksum_before = generate_naive_checksum(frame, size);
  corrupt_frame(frame, size);
  corrupt_frame(frame, size);
  unsigned short checksum_after = checksum_ccitt(frame, size);
  int naive_checksum_after = generate_naive_checksum(frame, size);
  printf("Checksum before %d\nNaive checksum before %d\nNaive checksum "
         "after%d\nChecksum after %d\n",
         checksum_before, naive_checksum_before, naive_checksum_after,
         checksum_after);
  free(frame);
  return 0;
}
