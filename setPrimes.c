#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>

#define FILE_NAME "primes.txt"
#define MAX_NUMBER 100000000
#define SQRT_NUMBER 10000
#define MAX_TEXT 10

bool list[MAX_NUMBER] = {0};
int main(){
    printf("Initiating prime calculations\n", sizeof(size_t));
    int fd = open(FILE_NAME,O_CREAT|O_WRONLY|O_TRUNC, 0666);
    for(int i=2;i<SQRT_NUMBER;i++) {
        if(!list[i]) {
            for(int j=i*2;j<MAX_NUMBER;j+=i) {
                list[j] = true;
            }
        }
    }
    printf("Text produced. Ready for printing\n", sizeof(size_t));

    for(long long int i=2;i<MAX_NUMBER;i++) {
        if(!list[i]) {
            char text[MAX_TEXT];
            sprintf(text, "%lld\n", i);
            write(fd, text, strlen(text));
        }
    }
    return 0;
}
 
