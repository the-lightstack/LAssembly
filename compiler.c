#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <signal.h>
#include <stdlib.h>     /* strtoll */

#define u_char unsigned char

#define COMMENT_CHAR 0x23 // == #
#define LINE_GROWTH_SIZE 0x60 // should be 0x60
#define MAX_LABEL_LENGTH 0x20
#define MAX_REGISTER_LENGTH 0x6 // flags
#define DEFAULT_OUTPUT_FILENAME "./out.lx"
#define LABEL_ADDR_GROWTH_STEPS 0x100


/*
Components:
 -> List of all asm instructions + their opcodes
 -> syscalls for: write,print (takes just pointer, assumes stdout), open, 
    read, input (read for stdin)
 -> program always starts at `_start` label
 -> first clean all comments
*/

// Mapping of registers/instructions and bytes
typedef struct { char *key; char val; } t_regInsByteLookup;

static t_regInsByteLookup byteMappings[] = {
    {"rax",0x50},
    {"rbx",0x51},
    {"rcx",0x52},
    {"rdi",0x53},
    {"rsi",0x54},
    {"rdx",0x55},
    {"rsp",0x56},
    {"rbp",0x57},
    {"rip",0x58},
    {"rbf",0x59},
    {"flags",0x60},

    {"push",0x10},
    {"pop",0x11},
    {"add",0x12},
    {"sub",0x13},
    {"and",0x14},
    {"mul",0x15},
    {"div",0x16},
    {"xor",0x17},
    {"mov",0x18},
    {"cmp",0x19},
    {"je",0x20},
    {"jne",0x21},
    {"jmp",0x22},
    {"jg",0x23},
    {"jl",0x24},
    {"syscall",0x25}
};

#define BYTE_LOOKUP_LEN (sizeof(byteMappings)/sizeof(t_regInsByteLookup))


char* deleteComments(char* code){
    size_t codeLength = strlen(code);

    bool deleteChar = false;
    char* p_cleanedCode = malloc(codeLength+1); 
    if (p_cleanedCode == NULL){
        fprintf(stderr,"%s\n","malloc couldn't allocate more memore.");
        exit(1);
    }
    memset(p_cleanedCode,0,codeLength+1);
    char* p_start_cleanedCode = p_cleanedCode;

    unsigned int i;
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

char *string_trim_inplace(char *s) {
  char *original = s;
  size_t len = 0;

  while (isspace((unsigned char) *s)) {
    s++;
  } 
  if (*s) {
    char *p = s;
    while (*p) p++;
    while (isspace((unsigned char) *(--p)));
    p[1] = '\0';
    len = (size_t) (p - s + 1);  
  }

  return (s == original) ? s : memmove(original, s, len + 1);
}


// Should already be left-stripped!
char* rStripWhitespace(char* line){
    int lineLength = strlen(line)+1;
    char* strippedLine = malloc(lineLength);
    if (strippedLine == NULL){
        fprintf(stderr,"%s\n","malloc couldn't allocate more memore.");
        exit(1);
    }
    memset(strippedLine,0,lineLength);
    // Copy until we hit space/tab/newline
    int startNoWhitespace;
    printf("Line Length: %d\n",lineLength);
    int i;
    for (i = lineLength-1;i>=0; i--){
        if (*(line+i) != 0x9 &&*(line+i) != 0x20 &&*(line+i) != 0xa ){
            startNoWhitespace = i;
            break;
        }
    }

    strncpy(strippedLine,line,startNoWhitespace+1); // printf("Right-stripped String: '%s'\n",strippedLine); return strippedLine;
    puts("Reached end of rSTripWhitespace, do not use this function!");
    exit(1);
}

char* lStripWhitespace(char* line){
    int lineLength = strlen(line)+1;
    char* whiteSpaceStrippedLine = malloc(lineLength);
    if (whiteSpaceStrippedLine == NULL){
        fprintf(stderr,"%s\n","malloc couldn't allocate more memore.");
        exit(1);
    }
    memset(whiteSpaceStrippedLine,0,lineLength);
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
    unsigned int i;
    for (i = 0; i<strlen(line); i++){
        char c = *(line+i);
        if (c != 0xa && c != 0x20 && c != 0x9 ){
            return false;
        }
    }
    return true;
}

char getByteCode(char* text){
    unsigned int i;
    // printf("Looking up reg for byte %s\n",text);
    for (i = 0; i<BYTE_LOOKUP_LEN;i++){
        t_regInsByteLookup sym = byteMappings[i];
        if (strcmp(text,sym.key) == 0){
            return (u_char)sym.val;
        }
    }
    printf("COMPILE ERROR: couldn't decode string '%s' to byte code!\n",text);
    return -1;
}

short parseTwoReg(char* lastPart){
    int firstReg_offset = 0;
    int secondReg_offset = 0;
    
    // We have to use the len of lastPart, else copying 
    // too much whitespace causes a buffer overflow
    int inputSize = strlen(lastPart);
    char firstReg [inputSize];
    char secondReg [inputSize];

    memset(firstReg,0,inputSize);
    memset(secondReg,0,inputSize);

    char twoRegReturn[2];

    unsigned int i;
    bool copyFirstBuf = true;
    // printf("Last part passed to parseTwoReg:%s\n",lastPart);
    for (i = 0; i<strlen(lastPart);i++){
        if(*(lastPart+i) == 0x2c){
            *(firstReg+firstReg_offset) = 0x0;
            copyFirstBuf = false;
        }
        else if(*(lastPart+i) == 0xa){
            *(secondReg+secondReg_offset) = 0x0;

            break;
        }else{
            if (copyFirstBuf){
                // Copying over byte
                *(firstReg+firstReg_offset) = *(lastPart+i);
                firstReg_offset ++; 
            }else{
                *(secondReg+secondReg_offset) = *(lastPart+i);
                secondReg_offset ++;

            }
        }
    }
    // printf("First REG:%s\n",firstReg);
    // printf("second REG:%s\n",secondReg);
    twoRegReturn[0] = getByteCode(lStripWhitespace(firstReg));
    // printf("Second register arfter strip: %s\n",lStripWhitespace(secondReg));
    twoRegReturn[1] = getByteCode(lStripWhitespace(secondReg));


    // Packing it into a short
    // Short has 16bit
    short retVal;
    retVal = (((short)twoRegReturn[0]) << 8) | twoRegReturn[1];
    return retVal;
}

// BIG ENDIAN!, so highest bits first
char* lluiToBytes(long long int number){
    char* retVal = malloc(8);
    if (retVal == NULL){
        fprintf(stderr,"%s\n","malloc couldn't allocate more memore.");
        exit(1);
    }
    retVal[7] = (u_char)number&0xff;
    retVal[6] = (u_char)(number>>8)&0xff;
    retVal[5] = (u_char)(number>>16)&0xff;
    retVal[4] = (u_char)(number>>24)&0xff;
    retVal[3] = (u_char)(number>>32)&0xff;
    retVal[2] = (u_char)(number>>40)&0xff;
    retVal[1] = (u_char)(number>>48)&0xff;
    retVal[0] = (u_char)(number>>56)&0xff;

    return retVal; 

}


char* handleMovInstruction(char* params,char opCode ){
    // p1: reg/reg*
    // p2: reg/reg*/number [hex/dec]
    // seperator ","

    char* s_Params = lStripWhitespace(params);
    int paramStringLen = strlen(s_Params)+1;

    int fp_offset = 0;
    int sp_offset = 0;

    char* firstParam = malloc(paramStringLen);
    if (firstParam == NULL){
        fprintf(stderr,"%s\n","malloc couldn't allocate more memore.");
        exit(1);
    }
    char* secondParam = malloc(paramStringLen);
    if (secondParam == NULL){
        fprintf(stderr,"%s\n","malloc couldn't allocate more memore.");
        exit(1);
    }
    memset(firstParam,0x0,paramStringLen);
    memset(secondParam,0x0,paramStringLen);

    bool usingFirstBuf = true; 
    // Setting size of following instructions
    char* compiledLine = malloc(sizeof(short)+12);
    if (compiledLine == NULL){
        fprintf(stderr,"%s\n","malloc couldn't allocate more memore.");
        exit(1);
    }

    // Just assuming that sizeof(short) is 2
    *((short*)compiledLine) = 12;
    *(compiledLine+2) = (u_char)opCode;

    int i;
    for (i = 0;i<paramStringLen; i++){
        // If the current char is a comma
        if (*(s_Params+i) == 0x2c){
            *(firstParam+i) = 0x0;
            usingFirstBuf = false;
        }else if(*(s_Params+i) == 0xa){
            *(secondParam+i) = 0x0;
            break;
        }else{
            // Copy char into right buffer
            if (usingFirstBuf){
                *(firstParam+fp_offset) = *(s_Params+i);
                fp_offset ++;
            }else{
                *(secondParam+sp_offset) = *(s_Params+i);
                sp_offset ++;
            }
        }
    }
    char* temp_free_1 = firstParam;
    char* temp_free_2 = secondParam;

    firstParam = lStripWhitespace(firstParam);
    secondParam = lStripWhitespace(secondParam);

    if (temp_free_1 != NULL){
        free(temp_free_1);
        temp_free_1 = NULL;
    }
    if (temp_free_2 != NULL){
        free(temp_free_2);
        temp_free_2 = NULL;
    }

    // printf("First part:%s\n",firstParam);
   
    // HANDLING FIRST PARAMETER  
    char regByteCode; 
    // If first character is a star
    if (*(firstParam) == 0x2a){
        // Write byte 0x3 (pointer)
        *(compiledLine+sizeof(short)+1) = 0x3;
        // Now get Byte-code of firstParam +1 (dis-regarding star)
        regByteCode = getByteCode(firstParam+1);
    }else{
        // Write byte 0x1 for normal register location
        *(compiledLine+sizeof(short)+1) = 0x1;
        regByteCode = getByteCode(firstParam);
    }

    // Writing register to next byte
    if (regByteCode == -1){
        fprintf(stderr,"Couldn't find byte code for first mov register\
        (%s)\n",firstParam);
    }
    *(compiledLine+sizeof(short)+2) = regByteCode;

    // HANDLING SECOND PARAMETER  
    regByteCode = 0xff;  
    if (*(secondParam) == 0x30  && *(secondParam+1) == 0x78){
        // If first two chars are '0x' 
        *(compiledLine+sizeof(short)+3) = (u_char)0x2; // a number

        long long int num = strtoll(secondParam,NULL,16);
        char* lluiByteNumber = lluiToBytes(num);
        for (int i=0;i<8;i++){
            *(compiledLine+sizeof(short)+4+i) = lluiByteNumber[i];
        }

    }else if(*(secondParam) >= 0x30 && *(secondParam) <= 0x39){ 
        // If it is a number between 0-9
        *(compiledLine+sizeof(short)+3) = (u_char)0x2; // a number
        long long int num = strtoll(secondParam,NULL,10);
        char* lluiByteNumber = lluiToBytes(num);
        for (int i=0;i<8;i++){
            *(compiledLine+sizeof(short)+4+i) = lluiByteNumber[i];
        }

    }else if (*(secondParam) == 0x2a){
        // If it is a star
        *(compiledLine+sizeof(short)+3) = (u_char)0x3; // a pointer
        // Write 7 0x0 bytes, then reg number
        for (int i=0;i<8;i++){
            *(compiledLine+sizeof(short)+4+i) = (u_char)0x0;
        }
        regByteCode = getByteCode(secondParam+1);
        if (regByteCode == -1){
                fprintf(stderr,"Couldn't find byte code for second mov register\
                (%s)\n",firstParam);
            }
        *(compiledLine+sizeof(short)+11) = regByteCode;

    }else{
        // Treat it as a normal register
        *(compiledLine+sizeof(short)+3) = (u_char)0x1; // a normal reg
        for (int i=0;i<8;i++){
            *(compiledLine+sizeof(short)+4+i) = (u_char)0x0;
        }
        regByteCode = getByteCode(secondParam);
        if (regByteCode == -1){
                fprintf(stderr,"Couldn't find byte code for second mov register\
                (%s)\n",firstParam);
            }
        *(compiledLine+sizeof(short)+11) = regByteCode;
    }



    // This is the end of the function
    if (firstParam != NULL){
        free(firstParam);
        firstParam = NULL;
    }
    if (secondParam != NULL){
        free(secondParam);
        secondParam = NULL;
    }
    
    return compiledLine;

} 

char parseOneReg(char* lastPart){
    // Read until "," then until newline
    char firstReg [strlen(lastPart)];
    unsigned int i;
    for (i = 0; i<strlen(lastPart); i++){
        if (*(lastPart+i) != 0xa){
            *(firstReg+i) = *(lastPart+i);
        }
    }
    // Strip away whitespace
    char* strippedFirstReg = lStripWhitespace(firstReg);
    return getByteCode(strippedFirstReg);
}


void assembleTwoByteInstruction(char opcode, char reg,char* returnBuffer){
    // TODO: Fix this here!!
    short opcodeLength = 0x2;
    returnBuffer = realloc(returnBuffer,sizeof(short)+2);
    if (returnBuffer == NULL){
        fprintf(stderr,"%s\n","REALLOC couldn't allocate more memore.");
        exit(1);
    }
    memset(returnBuffer,0,sizeof(short)+opcodeLength);
    // check if zeroed out or not
    *((short*)returnBuffer) = opcodeLength; // len = 2
    *(returnBuffer+sizeof(short)) = (char)opcode;
    *(returnBuffer+sizeof(short)+1) = (char)reg;
}

void assembleThreeByteInstruction(char opcode,char reg1, char reg2, char* retBuf){
    short opcodeLength = 0x3; 
    retBuf = realloc(retBuf,sizeof(short)+3);
    if (retBuf == NULL){
        fprintf(stderr,"%s\n","REALLOC couldn't allocate more memore.");
        exit(1);
    }
    memset(retBuf,0,sizeof(short)+opcodeLength);
    *((short*) retBuf) = opcodeLength; // len = 2
    *(retBuf+sizeof(short)) = (char)opcode;
    *(retBuf+sizeof(short)+1) = (char)reg1;
    *(retBuf+sizeof(short)+2) = (char)reg2;
}
char* handleThreeByteInstruction(char* nextLinePart,char opCode,char* result){
    short registers;
    char reg1;
    char reg2;

    registers = parseTwoReg(nextLinePart);
    reg1 = (registers&0xff00)>>8;
    reg2 = (registers&0xff);
    assembleThreeByteInstruction(opCode,reg1,
                    reg2,result);
    return result; 
}

char* handleJumpInstruction(u_char opCode,char* label, char* placeholderAddr, 
                    unsigned long* placeholderOffset,long* writtenBytes){
    /* Filling in placeholder information */
    char* returnBuffer = malloc(sizeof(short)+1+sizeof(int));
    *((short*)returnBuffer) = 5;
    *(returnBuffer+sizeof(short)) = opCode;
    *((int*)(returnBuffer+3)) = 0xeeeeeeee;

    /* Creating entry in placeholder dict */
    strcpy(placeholderAddr+*placeholderOffset,label);
    *((char*)(placeholderAddr+*placeholderOffset+strlen(label))) = 0x0;
    *((int*)(placeholderAddr+*placeholderOffset+strlen(label)+1)) = ((int)*writtenBytes)+1;
    *(placeholderOffset) += strlen(label)+1+4;

    return returnBuffer;
}

unsigned int findLabelAddress(char* label,char* labelDict, long labelDictLen){
    // Parsing address from label
    int address = -1;
    for (int i=0; i<labelDictLen;){
        if (strcmp(labelDict+i,label) == 0){
            address = *((int*)(labelDict+i+strlen(label)+1));
            break;
        }
        i += strlen(labelDict+i)+1+4;
    }
    if (address == -1){
        printf("Label '%s' does not exist. Abort Compilation.\n",label);
        exit(1);
    }
    return address;
}

char* parseLine(char* line,char* jumpPlaceholders,unsigned long* jumpPlaceholderOffset,long* writtenBytes){
    /*
        Return Value
        sizeof(int) [Indicate size of byte code || if -1 label follows,
                     read until 0 byte]
        Either label till 0x0 or byte sequence as compiled code ! Not \x00 end
    */ 
    
    printf("%s\n",line);

    // ??? Free this later again!
    char* strippedLine = lStripWhitespace(line);

    // We return int(length)+compiled bytes [directly concated]
    char* lenAndCompiledByteCode = malloc(sizeof(short)); 
    if (lenAndCompiledByteCode == NULL){
        fprintf(stderr,"%s\n","malloc couldn't allocate more memore.");
        exit(1);
    }

    
    char* operationString = malloc(strlen(strippedLine));  
    if (operationString == NULL){
        fprintf(stderr,"%s\n","malloc couldn't allocate more memore.");
        exit(1);
    }
    char* label = malloc(MAX_LABEL_LENGTH+1);
    if (label == NULL){
        fprintf(stderr,"%s\n","malloc couldn't allocate more memore.");
        exit(1);
    }
    memset(label,0,MAX_LABEL_LENGTH+1);
    
        
    // Detect label
    if (strippedLine[0] == 0x21){

        int i;
        for (i = 1; i < MAX_LABEL_LENGTH; i++){
             if (*(strippedLine+i) == 0x20 || *(strippedLine+i) == 0xa ||*(strippedLine+i) == 0x0){
                *(label+i-1) = 0x0;
                break;
            }
            *(label+i-1) = *(strippedLine+i);
        }

        // Returning label
        lenAndCompiledByteCode = realloc(lenAndCompiledByteCode,
                                         sizeof(short)+strlen(label)+1);
        if (lenAndCompiledByteCode == NULL){
            fprintf(stderr,"%s\n","REALLOC couldn't allocate more memore.");
            exit(1);
        }

        memset(lenAndCompiledByteCode,0,sizeof(short)+strlen(label)+1);
        *((short* )lenAndCompiledByteCode) = (short)-1;
        strncpy(lenAndCompiledByteCode+sizeof(short),label,strlen(label)+1);
        return lenAndCompiledByteCode;

    }else{
        // Read until first whitespace/ newline
        unsigned int i;
        for (i = 0; i< strlen(strippedLine); i++){
            // Break out if we hit space or newline (not TAB!) 
            if (*(strippedLine+i) == 0x20 || *(strippedLine+i) == 0xa){
                *(operationString+i) = 0x0;
                break;
            }
            *(operationString+i) = *(strippedLine+i);
        }
        // TODO: Add oprecompiler here
        // Here is the place the converter has to replace 
        // for exmpl: add rax,0x1 with mov rbf, 0x1 ; add rax,rbf
       
        char opCode = getByteCode(operationString);
        
        // Some operations are only 1 byte long
        switch (opCode){
            // Syscall
            case 0x25:
                lenAndCompiledByteCode = realloc(lenAndCompiledByteCode,
                                                 sizeof(short)+1);
                if (lenAndCompiledByteCode == NULL){
                    fprintf(stderr,"%s\n","REALLOC couldn't allocate more memore.");
                    exit(1);
                }
                *((short*)lenAndCompiledByteCode) = (short)1;
                *(lenAndCompiledByteCode+(sizeof(short))) = opCode;
                return lenAndCompiledByteCode; 
        }


        char* nextLinePart = strippedLine+strlen(operationString);
        // printf("Next part:%s\n",nextLinePart);

        // 2-byte operations
        char firstRegister;
        switch (opCode){
            // push
            case 0x10:
                firstRegister = parseOneReg(nextLinePart);
                assembleTwoByteInstruction(opCode,firstRegister,
                                            lenAndCompiledByteCode);
                return lenAndCompiledByteCode; 
                // pop
             case 0x11:
                firstRegister = parseOneReg(nextLinePart);
                assembleTwoByteInstruction(opCode,firstRegister,
                                            lenAndCompiledByteCode);
                return lenAndCompiledByteCode;
            }
        
        short registers;
        char reg1;
        char reg2;
        switch(opCode){
            case 0x12:

                registers = parseTwoReg(nextLinePart);
                // Upper 8 bits
                reg1 = (registers&0xff00)>>8;
                // Lower 8 bits
                reg2 = (registers&0xff);

                assembleThreeByteInstruction(opCode,reg1,
                                reg2,lenAndCompiledByteCode);
                return lenAndCompiledByteCode; 
            case 0x13:
                return handleThreeByteInstruction(nextLinePart,opCode,lenAndCompiledByteCode);
            case 0x14:
                return handleThreeByteInstruction(nextLinePart,opCode,lenAndCompiledByteCode);
            case 0x15:
                return handleThreeByteInstruction(nextLinePart,opCode,lenAndCompiledByteCode);
            case 0x16:
                return handleThreeByteInstruction(nextLinePart,opCode,lenAndCompiledByteCode);
            case 0x17:
                return handleThreeByteInstruction(nextLinePart,opCode,lenAndCompiledByteCode);
            case 0x19:
                return handleThreeByteInstruction(nextLinePart,opCode,lenAndCompiledByteCode);
            }
             // *Mov* instruction
        if (opCode == 0x18){
            lenAndCompiledByteCode = handleMovInstruction(nextLinePart,opCode);
            return lenAndCompiledByteCode; 
        }

        nextLinePart = string_trim_inplace(nextLinePart);
        // All the jump instructions
        switch (opCode){
            case 0x20:
                return handleJumpInstruction(opCode,nextLinePart,jumpPlaceholders,jumpPlaceholderOffset,writtenBytes);
            case 0x21:
                return handleJumpInstruction(opCode,nextLinePart,jumpPlaceholders,jumpPlaceholderOffset,writtenBytes);
            case 0x22:
                return handleJumpInstruction(opCode,nextLinePart,jumpPlaceholders,jumpPlaceholderOffset,writtenBytes);
            case 0x23:
                return handleJumpInstruction(opCode,nextLinePart,jumpPlaceholders,jumpPlaceholderOffset,writtenBytes);
            case 0x24:
                return handleJumpInstruction(opCode,nextLinePart,jumpPlaceholders,jumpPlaceholderOffset,writtenBytes);
        }

    }
    
    if (strippedLine != NULL){
        free(strippedLine);
        strippedLine = NULL;
    }
    if (operationString != NULL){
        free(operationString);
        operationString = NULL;
    }
 
    // printf("Operator: %s\n",operationString);
    *(lenAndCompiledByteCode) = (short)0x0;
    fprintf(stderr,"Invalid code: '%s'\n",line);
    fprintf(stderr,"%s\n","COMPILE ERROR in parseLine: about to return pointer into wilderness");
    exit(1);
}


char* compileSourceCode(char* sourceCode,long* outputByteSize){
    // sourceCode length just estimate, could break!
    // Format: "main"\0xff 4 byte address of byte (last written +1)
    long sourceCodeSize = strlen(sourceCode);

    long compiledCode_offset = 0;
    char* p_compiledCode = malloc(sourceCodeSize);
    if (p_compiledCode == NULL){
        fprintf(stderr,"%s\n","malloc couldn't allocate more memore.");
        exit(1);
    }

    memset(p_compiledCode,0,sourceCodeSize);

    // maybe vulnerable to long-overflow =)
    long currentLineSize = LINE_GROWTH_SIZE;
    char* currentLine = malloc(LINE_GROWTH_SIZE);
    if (currentLine == NULL){
        fprintf(stderr,"%s\n","malloc couldn't allocate more memore.");
        exit(1);
    }


    char* start_currentLine = currentLine;
    memset(start_currentLine,0,LINE_GROWTH_SIZE);

    char* oneLineCompiled;
    short restSize;
    int lineCounter = 0;

    /*
    Format: <label>\x00<adress in source where to put label's addr (4byte)>
    */
    unsigned long* jumpPlaceholderOffset = malloc(sizeof(unsigned long));
    *jumpPlaceholderOffset = 0;
    char* jumpPlaceholders = malloc(sourceCodeSize+1);
    if (jumpPlaceholders == NULL){
        fprintf(stderr,"%s\n","malloc couldn't allocate more memore.");
        exit(1);
    }
    memset(jumpPlaceholders,0,sourceCodeSize+1);

    long curAllocatedLabelDictSize = LABEL_ADDR_GROWTH_STEPS;
    long curLabelAddressWrittenBytes = 0;
    char* labelAddressDict = malloc(LABEL_ADDR_GROWTH_STEPS);
    memset(labelAddressDict,0,LABEL_ADDR_GROWTH_STEPS);
    if (labelAddressDict == NULL){
        fprintf(stderr,"%s\n","malloc couldn't allocate more memore.");
        exit(1);
    }
    
    int i;
    for (i = 0; i<sourceCodeSize; i++){
        /*
         Copy chars over until we hit \n, add nullbyte, then call parseLine
         and write over old line do it again!
        */ 
        if (*(sourceCode+i) == 0xa){ 
            lineCounter ++;
            *(currentLine+1) = 0x0;
            // Resetting point to start of string
            currentLine = start_currentLine; 
            if (!isOnlyWhitespace(start_currentLine)){
                char* tempCurrentLine = string_trim_inplace(start_currentLine);
                oneLineCompiled = parseLine(tempCurrentLine,jumpPlaceholders,
                                            jumpPlaceholderOffset,outputByteSize);
                memset(start_currentLine,0,currentLineSize);
                // printf("oneLineCompiled = %x\n",*(oneLineCompiled));
                if (*((short*)oneLineCompiled) == 0x0){
                    fprintf(stderr,"Error when parsing line %d!\n",lineCounter);
                    
                }
                // Read first short to determine length of code
                restSize = *((short*)oneLineCompiled);

                // We get a label
                if (restSize == -1){
                    // Before writing to labelAddrDict check if it fits
                    // and realloc if needed
                    if (curLabelAddressWrittenBytes + strlen((oneLineCompiled+sizeof(short)))+1
                        >= curAllocatedLabelDictSize ){

                        puts("Having to realloc, so thing is really long.");
                        char* newDictRegion = realloc(labelAddressDict,curAllocatedLabelDictSize+LABEL_ADDR_GROWTH_STEPS);
                        curAllocatedLabelDictSize += LABEL_ADDR_GROWTH_STEPS;
                        if (newDictRegion == NULL){
                            fprintf(stderr,"%s\n","Realloc couldn't allcote memory, exiting.");
                            exit(1);
                        }
                        if (newDictRegion != labelAddressDict){
                            memset(newDictRegion,0,curAllocatedLabelDictSize);
                            // Realloc changed pointer location
                            int k;
                            for (k=0; k<curAllocatedLabelDictSize;k++){
                                *(newDictRegion) = *(labelAddressDict);
                            }
                        }else{
                            memset(newDictRegion+curLabelAddressWrittenBytes,0,LABEL_ADDR_GROWTH_STEPS);
                        }
                        labelAddressDict = newDictRegion;
                    }
                    // Copying over string
                    strncpy(labelAddressDict+curLabelAddressWrittenBytes,oneLineCompiled+sizeof(short),MAX_LABEL_LENGTH);
                    *(labelAddressDict+curLabelAddressWrittenBytes+strlen(oneLineCompiled)) = 0x0;
                    curLabelAddressWrittenBytes += strlen(oneLineCompiled+sizeof(short))+1;

                    // Getting address
                    *((int*)(labelAddressDict+curLabelAddressWrittenBytes)) = (int)*(outputByteSize);
                    curLabelAddressWrittenBytes += sizeof(int);
                }

                 
                // For debugging
                if ((unsigned short)restSize > 0x10 && (short) restSize != -1 ){
                    puts("Hit really bad debug breakpoint!");
                    asm("int $3");
                }

                // Copying over one-line code to entire code
                int j;
                for ( j = 0; j< restSize;j++ ){
                    // printf("Writing byte %x to %p\n",
                    // *(oneLineCompiled+sizeof(short)+j),
                    //(p_compiledCode+compiledCode_offset+j));
                   *(p_compiledCode+compiledCode_offset+j) = *(oneLineCompiled+sizeof(short)+j);
                   *(outputByteSize) = *(outputByteSize) + 1;
                }
                // Maybe put this before loop
                if(restSize != -1){
                    compiledCode_offset += restSize;
                }
                
            }
            continue;
            
        }

        // Copy if current line is big enough, else first realloc and add 0x60
        if (currentLine - start_currentLine + 1 >= currentLineSize){
            currentLineSize += LINE_GROWTH_SIZE;
            start_currentLine = realloc(start_currentLine,currentLineSize);
            if (start_currentLine == NULL){
                fprintf(stderr,"%s\n","REALLOC couldn't allocate more memore.");
                exit(1);
            }
            memset(start_currentLine,0,currentLineSize);
            puts("Having to reallocate space!\n");
        }
        
        // Actual byte copying
        *currentLine = *(sourceCode+i);
        currentLine ++; 

    }

    /*
    Filling in labels found in jumpPlacholders TODO!
    Looping over all placeholder entries
    */

    i = 0;
    char* tempLabel = malloc(MAX_LABEL_LENGTH+1);
    if (tempLabel == NULL){
        fprintf(stderr,"%s\n","REALLOC couldn't allocate more memore.");
        exit(1);
    }
    memset(tempLabel,0,MAX_LABEL_LENGTH+1);

    unsigned int addressDestination = -1;
    unsigned int address = -1;
    for (i=0;i<*jumpPlaceholderOffset;){
        /* the label one jump wants*/
        strncpy(tempLabel,jumpPlaceholders+i,MAX_LABEL_LENGTH);
        i += strlen(tempLabel)+1;
        address = findLabelAddress(tempLabel,labelAddressDict,curLabelAddressWrittenBytes);
        if (address == -1){
            puts("Error when trying to find label.");
        }
        /*
        Reading address where to put the label's address
        */
        addressDestination = *((int*)(jumpPlaceholders+i));
        /* Writing real address to adressDestination */
        *((int*)(p_compiledCode+addressDestination)) = address;
        i += 4; 

    }
    if (tempLabel != NULL){
        free(tempLabel);
        tempLabel = NULL;
    }
    

    puts("");
    puts("Compiled Source Code");
    return p_compiledCode; 
}


int main(int argc,char* argv[]){
    /* Checking if enough command line arguments were supplied */
    if (argc < 2){
        puts("Supply source file as command line argument 1.");
        exit(1);
    }


    FILE* program_fd = NULL;
    // program_fd = fopen("./example_program.lasm","rb");
    program_fd = fopen(argv[1],"rb");

    if (program_fd == NULL){
        puts("Failed to open file.");
        exit(1);
    }

    fseek(program_fd,0L,SEEK_END);
    long sourceCodeSize = ftell(program_fd);
    fseek(program_fd,0L,SEEK_SET);
    printf("File size of example-prog:%ld\n",sourceCodeSize);
    puts("");
        
    // Copying file into fitting buffer on heap
    char* code = malloc(sourceCodeSize+1);
    if (code == NULL){
        fprintf(stderr,"%s\n","malloc couldn't allocate more memore.");
        exit(1);
    }
    memset(code,0,sourceCodeSize+1);
    fread(code,1,sourceCodeSize,program_fd);
    fclose(program_fd);
    
    //printf("Code with Comments:\n%s\n",code);
    char* uncommentedCode = deleteComments(code);
    
    long* compiledCodeByteSize = malloc(sizeof(long));
    if (compiledCodeByteSize == NULL){
        fprintf(stderr,"%s\n","malloc couldn't allocate more memore.");
        exit(1);
    }
    *compiledCodeByteSize = (long)0x0;
    char* p_compiledCode =  compileSourceCode(uncommentedCode,compiledCodeByteSize);

    printf("Compiled Code Byte Size: %ld\n",*(compiledCodeByteSize));
    FILE* outFile_fd = NULL;
    outFile_fd = fopen(DEFAULT_OUTPUT_FILENAME,"wb");
    fwrite(p_compiledCode,1,(long)*compiledCodeByteSize,outFile_fd);
    fclose(outFile_fd);
    
    return 0;
}
