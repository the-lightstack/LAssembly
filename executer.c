#include <stdio.h>                                                              
#include <string.h>                                                             
#include <unistd.h>                                                             
#include <stdlib.h>                                                             
#include <stdbool.h>                                                            
#include <fcntl.h>                                                              
#include <sys/types.h>                                                          
#include <sys/stat.h>                                                           


#define STACK_SIZE 2048                                                         
#define HEAP_SIZE 2048                                                          
#define COMMENT_CHAR 0x23 // so #      

char* STACK [STACK_SIZE];
char* HEAP [HEAP_SIZE];
int* OPEN_FILE_DESCRIPTORS[64];


int execute_assembly(char* code){
    // Defining all registers
    long long rax = 0;
    long long rbx = 0;
    long long rcx = 0;
    long long rdx = 0;

    long long rdi = 0;
    long long rsi = 0;
    long long rdx = 0;

    long long rsp = 0;
    long long rbp = 0;

    long long rip = 0;
    long long flags = 0;

}

int main(int argc, char* argv[]){

    return 0;
}


