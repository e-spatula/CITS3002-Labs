#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>

int main(int argcount, char *argvalue[])
{
    struct stat st;
    stat("Q1.datafile", &st);
    int size = st.st_size;
    
    FILE* fp = fopen("Q1.datafile", "r");
    int iterations = size / sizeof(int32_t);
    int32_t  *ptr = malloc(sizeof(int32_t));
    if(ptr == NULL) {
        printf("Malloc failed\n");
    }
    for(int i = 0; i < iterations; ++i) {
        fread(ptr, sizeof(int32_t), 1, fp);
        printf("%d\n", *ptr);
    }
    free(ptr);     
    return 0;
}
