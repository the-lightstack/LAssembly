#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <regex.h>

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
    long long rax = 0;
    long long rbx = 0;
    long long rcx = 0;

    long long rdi = 0;
    long long rsi = 0;
    long long rdx = 0;
    
    long long rsp = 0;
    long long rbp = 0;
    
    long long rip = 0;
    long long flags = 0;
            

}

char* deleteComments(char* code){
    size_t codeLength = strlen(code);

    bool deleteChar;
    char* p_cleanedCode = malloc(codeLength); 
    char* p_start_cleanedCode = p_cleanedCode;

    int i;
    for (i = 0; i<codeLength;i++){
        if (*(code+i) == 0xa){
            deleteChar = false;
        }else if(*(code+i) == COMMENT_CHAR){
            deleteChar = true;  
        }
        // Deleting char == not copying it over
        if (!deleteChar){
            // Copy over one char to h_code (heap-code)
            *p_cleanedCode = *(code+i);
            p_cleanedCode ++;
        }

    }

    return p_start_cleanedCode;
}



int main(int argc,char* argv[]){
    FILE* program_fd = NULL;
    program_fd = fopen("./example_program.lasm","rb");

    if (program_fd == NULL){
        puts("Failed to open file.");
        exit(1);
    }

    fseek(program_fd,0L,SEEK_END);
    long sourceCodeSize = ftell(program_fd);
    fseek(program_fd,0L,SEEK_SET);
    printf("File size of example-prog:%ld\n",sourceCodeSize);
        
    // Copying file into fitting buffer on heap
    char* code = malloc(sourceCodeSize);
    fread(code,1,sourceCodeSize,program_fd);

    printf("Code with Comments:\n%s\n",code);
    char* uncommentedCode = deleteComments(code);
    printf("After Removing Comments:\n%s\n", uncommentedCode);


    
    
     
    return 0;
}
