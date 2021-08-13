#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>

#define STACK_SIZE 2048 
#define HEAP_SIZE 2048
#define COMMENT_CHAR 0x23 // so #

/*
Components:
 -> List of all asm instructions + their functionality
 -> A stack 
 -> syscalls for: write,print (takes just pointer, assumes stdout), open, 
    read, input (read for stdin)
 -> programm always starts at `main` label
 -> first clean all comments
 -> maybe even include other files?
*/


char* STACK [STACK_SIZE];
char* HEAP [HEAP_SIZE];
int* OPEN_FILE_DESCRIPTORS[64]; 

int execute_assembly(char* code){
    // Defining all registers
    long long rax;
    long long rbx;
    long long rcx;

    long long rdi;
    long long rsi;
    long long rdx;
    
    long long rsp;
    long long rbp;
    
    long long rip;
    long long flags;


        

}

char* deleteComments(char* code){
    size_t codeLength = strlen(code);
    char* h_code = malloc(codeLength);

    int i;
    bool deleteChar;
    char* p_cleanedCode; 

    for (i = 0; i<codeLength;i++){
        if (code[i] == 0xa){
            deleteChar = false;
        }else if(code[i] == COMMENT_CHAR){
            deleteChar = true;  
        }
        // Deleting char == not copying it over
        if (!deleteChar){
            // Copy over one char to h_code (heap-code)
            *p_cleanedCode = code[i];
            p_cleanedCode ++;
        }

    }

    return h_code;
     
}

int main(int argc,char* argv[]){
    int program_fd = fopen("./example_program.lasm","r");
    struct stat stat;
    fstat(program_fd,&stat);
    
    printf("File size of ex-prog:%d\n",stat,st_size);
    
    
     
    return 0;
}
