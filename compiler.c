#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <regex.h>

#define COMMENT_CHAR 0x23 // == #
#define LINE_GROWTH_SIZE 0x60

/*
Components:
 -> List of all asm instructions + their opcodes
 -> syscalls for: write,print (takes just pointer, assumes stdout), open, 
    read, input (read for stdin)
 -> program always starts at `_start` label
 -> first clean all comments
*/


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

char* lStripWhitespace(char* line){
    int lineLength = strlen(line);
    char* whiteSpaceStrippedLine = malloc(lineLength);
    char* start_whiteSpaceStripped = whiteSpaceStrippedLine;

    // Stripping leading spaces
    int i;
    bool skippedAllSpaces = false;
    for (i = 0; i<lineLength; i++ ){
        // If char is not space and tab, we skipped everyting
        if (*(line+i) != 0x20 && *(line+i) != 0x9 ){
            skippedAllSpaces = true;
        }

        if (skippedAllSpaces){
            // Check if line end
            if (*(line+i) == 0x0a){
                break;
            }

            // Copy over char
            *whiteSpaceStrippedLine = *(line+i);
            whiteSpaceStrippedLine ++;
        }            
    }

    return start_whiteSpaceStripped;

}

bool isOnlyWhitespace(char* line){
    // Returns True if line is only whitespace (tab/space/newline)
    int i;
    for (i = 0; i<strlen(line); i++){
        char c = *(line+i);
        if (c != 0xa && c != 0x20 && c != 0x9 ){
            return false;
        }
    }
    return true;
}


char* parseLine(char* line){
    //"     xor rbp,rbp\x0a"
    printf("NL: %s\n",line);
}


char* compileSourceCode(char* sourceCode){
    // sourceCode length just estimate, could break!
    // Format: "main"\0xff 4 byte address of byte (last written +1)
    long sourceCodeSize = strlen(sourceCode);
    char* labelDict = malloc(sourceCodeSize); 
    char* compiledCode = malloc(sourceCodeSize);

    // maybe vulnerable to long-overflow =)
    long currentLineSize = LINE_GROWTH_SIZE;
    char* currentLine = malloc(LINE_GROWTH_SIZE);
    char* start_currentLine = currentLine;
    int writtenCharsLine = 0;

    int i;
    for (i = 0; i<sourceCodeSize; i++){
        /*
         Copy chars over until we hit \n, add nullbyte, then call parseLine
         and write over old line do it again!
        */ 
        if (*(sourceCode+i) == 0xa){

            *(currentLine) = 0x0;
            // Resetting point to start of string
            currentLine = start_currentLine; 
            if (!isOnlyWhitespace(start_currentLine)){
                parseLine(start_currentLine);
            }
            writtenCharsLine = 0;
            
        }

        // Copy if current line is big enough, else first realloc and add 0x60
        if (currentLine - start_currentLine + 1 >= currentLineSize){
            currentLineSize += LINE_GROWTH_SIZE;
            start_currentLine = realloc(start_currentLine,currentLineSize);
        }
        
        // Actual byte copying
        *currentLine = *(sourceCode+i);
        currentLine ++; 
        writtenCharsLine ++;


    }
}


int main(int argc,char* argv[]){
    FILE* program_fd = NULL;
    // program_fd = fopen("./example_program.lasm","rb");
    program_fd = fopen("./short_example.lasm","rb");

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

    //printf("Code with Comments:\n%s\n",code);
    char* uncommentedCode = deleteComments(code);

    compileSourceCode(uncommentedCode);
    
    return 0;
}
