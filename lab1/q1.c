#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

int main(int argcount, char *argvalue[])
{
    int  fd = open("Q1.datafile", O_CREAT|O_WRONLY, 0600);

    if(fd >= 0) {        //  IFF FILE OPENED SUCCESSFULLY
        for(int32_t i = -50 ; i<50 ; i++) {
            write(fd, &i, sizeof(i));
        }
        close(fd);
    }    
    return 0;
}
