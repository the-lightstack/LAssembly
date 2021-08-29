#include <stdio.h>                                                              
#include <string.h>                                                             
#include <unistd.h>                                                             
#include <stdlib.h>                                                             
#include <stdbool.h>                                                            
#include <fcntl.h>                                                              
#include <sys/types.h>                                                          
#include <sys/stat.h>                                                           

#define DEFAULT_INPUT_FILE "out.lx"
#define u_char unsigned char

#define STACK_SIZE 2048                                                         
#define HEAP_SIZE 2048                                                          
#define COMMENT_CHAR 0x23 // so #      

#define BREAKPOINT asm("int $3");

// CMP FLAGS ENUM
#define ZERO 1
#define GREATER 2
#define LESS 4

#define FLAGS_REGISTER_BYTECODE 0x60
#define RIP_REGISTER_CODE 0x58
#define RSP_REGISTER_CODE 0x57

#define JUMP_INSTRUCTIONS_SIZE 5

char* MEMORY [STACK_SIZE+HEAP_SIZE];
int* OPEN_FILE_DESCRIPTORS[64];

/*
Defining byte code - register mapping table
*/

typedef struct {
    long long* rax;
    long long* rbx;
    long long* rcx;

    long long* rdi;
    long long* rsi;
    long long* rdx;

    long long* rsp;
    long long* rbp;
    long long* rbf;

    long long* rip;
    long long* flags;
} s_registers;

typedef struct { u_char key; long long* regLoc; } t_regByteCodeLookup;
static t_regByteCodeLookup registerByteCodeVariable[] = {
    {0x50,NULL},
    {0x51,NULL},
    {0x52,NULL},
    {0x53,NULL},
    {0x54,NULL},
    {0x55,NULL},
    {0x56,NULL},
    {0x57,NULL},
    {0x58,NULL},
    {0x59,NULL},
    {0x60,NULL}      
};

#define BYTE_LOOKUP_LEN (sizeof(registerByteCodeVariable)/sizeof(t_regByteCodeLookup))



s_registers* initRegisterLookup(void){
    /*
    Initialising the lookup table with the right register pointers,
    which aren't constant.
    */

    s_registers* registers = malloc(sizeof(s_registers));
   
    /*
    Allocating 8 byte memory for each register
    */ 
    registers->rax = malloc(sizeof(long long));
    registers->rbx = malloc(sizeof(long long));
    registers->rcx = malloc(sizeof(long long));

    registers->rdi = malloc(sizeof(long long));
    registers->rsi = malloc(sizeof(long long));
    registers->rdx = malloc(sizeof(long long));

    registers->rsp = malloc(sizeof(long long));
    registers->rbp = malloc(sizeof(long long));
    registers->rip = malloc(sizeof(long long));
    registers->rbf = malloc(sizeof(long long));
    registers->flags = malloc(sizeof(long long));

    /*
    Setting every register to zero
    */
    *(registers->rax) = 0; 
    *(registers->rbx) = 0; 
    *(registers->rcx) = 0; 

    *(registers->rdi) = 0; 
    *(registers->rsi) = 0; 
    *(registers->rdx) = 0; 

    *(registers->rbp) = 0; 
    *(registers->rsp) = 0; 
    *(registers->rbf) = 0; 
    *(registers->rip) = 0; 
    *(registers->flags) = 0; 

    /*
    Initialising dictionary with pointer location of registers
    */
    
    registerByteCodeVariable[0].regLoc = registers->rax;
    registerByteCodeVariable[1].regLoc = registers->rbx;
    registerByteCodeVariable[2].regLoc = registers->rcx;

    registerByteCodeVariable[3].regLoc = registers->rdi;
    registerByteCodeVariable[4].regLoc = registers->rsi;
    registerByteCodeVariable[5].regLoc = registers->rdx;

    registerByteCodeVariable[6].regLoc = registers->rbp;
    registerByteCodeVariable[7].regLoc = registers->rsp;
    registerByteCodeVariable[8].regLoc = registers->rip;
    registerByteCodeVariable[9].regLoc = registers->rbf;
    registerByteCodeVariable[10].regLoc = registers->flags;

    return registers;
}

unsigned long getFileSize(char* filename){
    /*
    Getting the size of a file in bytes and returning it.
    Returns 0xffffff if file couldn't be opened.
    */
    unsigned long fileSize = 0;
    FILE* file_fd = NULL; 
    file_fd = fopen(filename,"rb");
    if (file_fd == NULL){
        fprintf(stderr,"File '%s' couldn't be opened.",filename);
    }
    fseek(file_fd,0L,SEEK_END);                                              
    fileSize = ftell(file_fd);                                    
    fseek(file_fd,0L,SEEK_SET);     
    fclose(file_fd);

    return fileSize;
}


u_char* loadByteCode(char* filename){
    /*
    Creating buffer of the right size for the contents of the given file
    and copying over all the bytes. Also appends 0xff to end of file acting as
    EOF character.
    Then returning pointer to loaded bytes.
    */
    unsigned long fileSize = getFileSize(filename);
    unsigned char* byteCode = malloc(fileSize+1);
    *(byteCode+fileSize) = 0xff;

    FILE* file_fd = NULL; 
    file_fd = fopen(filename,"rb");
    if (file_fd == NULL){
        fprintf(stderr,"File '%s' couldn't be opened.",filename);
    }

    /* Copying over bytes */
    fread(byteCode,1,fileSize,file_fd);
    fclose(file_fd);

    return byteCode; 
}

long long* getRegisterPointer(u_char searchByte){                                                   
    /*
    Searches the byte-Code/register-pointer table for the 
    */

    unsigned int i;                                                             
    for (i = 0; i<BYTE_LOOKUP_LEN;i++){                                         
        t_regByteCodeLookup sym = registerByteCodeVariable[i];                               
        if ( sym.key == searchByte){                                         
            return sym.regLoc;                                             
        }                                                                       
    }                                                                           
    puts("Invalid register in instruction given.");
    return NULL;                                                                  
}             

void setZeroFlag(long long result){
    /*
    Function that first resets the zero flag and then sets it if the result
    is equal to zero.
    */ 
    long long* flagsPointer = getRegisterPointer(FLAGS_REGISTER_BYTECODE);
    /*
    Setting the first bit to zero/ Resetting zero flag
    */
    *flagsPointer &= ~(1UL << 0);

    if (result == 0){
        /*
        Setting the zero flag (first bit = 1)
        */
        *flagsPointer |= 1;
    }

}


int addInstruction(u_char* instruction){
    /*
    Returns the length of the entire instruction (including 1 byte for opcode)
    Then adds the second register onto the first one.
    */
    u_char reg1_code = *(instruction+1);
    u_char reg2_code = *(instruction+2);

    long long* reg1_pointer = getRegisterPointer(reg1_code);
    long long* reg2_pointer = getRegisterPointer(reg2_code);
    if (reg1_pointer == NULL || reg2_pointer == NULL){
        puts("Execution Aborted, couldn't parse registers.");
        exit(1);
    }
    *reg1_pointer = *reg1_pointer+*reg2_pointer;
    setZeroFlag(*reg1_pointer+*reg2_pointer);
    return 3;
}

int subInstruction(u_char* instruction){
    /*
    Returns the length of the entire instruction (including 1 byte for opcode)
    Then subtracts the second register from the first register.
    */
    u_char reg1_code = *(instruction+1);
    u_char reg2_code = *(instruction+2);

    long long* reg1_pointer = getRegisterPointer(reg1_code);
    long long* reg2_pointer = getRegisterPointer(reg2_code);
    if (reg1_pointer == NULL || reg2_pointer == NULL){
        puts("Execution Aborted, couldn't parse registers.");
        exit(1);
    }
    *reg1_pointer = *reg1_pointer - *reg2_pointer;
    setZeroFlag(*reg1_pointer-*reg2_pointer);
    return 3;
}


int andInstruction(u_char* instruction){
    /*
    Returns the length of the entire instruction (including 1 byte for opcode)
    Then performs a binary AND (&) on both and puts the result into the first
    register. 
    */

    u_char reg1_code = *(instruction+1);
    u_char reg2_code = *(instruction+2);

    long long* reg1_pointer = getRegisterPointer(reg1_code);
    long long* reg2_pointer = getRegisterPointer(reg2_code);
    if (reg1_pointer == NULL || reg2_pointer == NULL){
        puts("Execution Aborted, couldn't parse registers.");
        exit(1);
    }
    *reg1_pointer = *reg1_pointer & *reg2_pointer;
    setZeroFlag(*reg1_pointer&*reg2_pointer);
    return 3;
}

int mulInstruction(u_char* instruction){
    /*
    Returns the length of the entire instruction (including 1 byte for opcode)
    Then multiplies both registers and puts the resulting value into the 
    first provided register. 
    */
    u_char reg1_code = *(instruction+1);
    u_char reg2_code = *(instruction+2);

    long long* reg1_pointer = getRegisterPointer(reg1_code);
    long long* reg2_pointer = getRegisterPointer(reg2_code);
    if (reg1_pointer == NULL || reg2_pointer == NULL){
        puts("Execution Aborted, couldn't parse registers.");
        exit(1);
    }
    *reg1_pointer = (*reg1_pointer) * (*reg2_pointer);
    setZeroFlag((*reg1_pointer) * (*reg2_pointer));
    return 3;
}

int divInstruction(u_char* instruction){
    /*
    Returns the length of the entire instruction (including 1 byte for opcode)
    Then divides both registers and puts the resulting value into the 
    first provided register. Value is returned as an integer (simply casted 
    to a ll)
    */
    u_char reg1_code = *(instruction+1);
    u_char reg2_code = *(instruction+2);

    long long* reg1_pointer = getRegisterPointer(reg1_code);
    long long* reg2_pointer = getRegisterPointer(reg2_code);
    if (reg1_pointer == NULL || reg2_pointer == NULL){
        puts("Execution Aborted, couldn't parse registers.");
        exit(1);
    }
    /*
    If the dividend is 0, return zero division error
    */
    if (*reg2_pointer == 0){
        puts("Zero division error encountered.");
        exit(1);
    }
    *reg1_pointer = (long long)(*reg1_pointer) / (*reg2_pointer);
    setZeroFlag((*reg1_pointer) / (*reg2_pointer));
    return 3;
}


int xorInstruction(u_char* instruction){
    /*
    Returns the length of the xor instruction (3) and modifies the 
    first register to be the xor result of reg1 and reg2.
    */

    u_char reg1_code = *(instruction+1);
    u_char reg2_code = *(instruction+2);

    long long* reg1_pointer = getRegisterPointer(reg1_code);
    long long* reg2_pointer = getRegisterPointer(reg2_code);
    if (reg1_pointer == NULL || reg2_pointer == NULL){
        puts("Execution Aborted, couldn't parse registers.");
        exit(1);
    }
    *reg1_pointer = (*reg1_pointer) ^ (*reg2_pointer);
    setZeroFlag((*reg1_pointer) ^ (*reg2_pointer));
    return 3;
}

int movInstruction(u_char* instruction){
    /*
    Returns the length of the xor instruction (3) and modifies the 
    first register to be the xor result of reg1 and reg2.
    */
    /*
    Tells the execution engine whether to treat the following register byte
    as the register to put the value into or the stack/heap location to write
    to. 
    1 = register / 3 = register location
    */
    u_char specifier = *(instruction+1);
    u_char reg1 = *(instruction+2);
    long long* firstRegister = getRegisterPointer(reg1);

    if (firstRegister == NULL){
        puts("Execution Aborted, couldn't parse registers.");
        exit(1);
    }

    /*
        This value specifies whether the following 8 bytes shall be treated
        as a simple number, a register or the location of a register to 
        read data from. 
        1 = register/ 2 = val / 3 = register location
    */
    u_char valSpecifier = *(instruction+3);
    long long writeValue = 0;


    if (valSpecifier == 1){
        /*
        Read the least significant byte to get byte value for the register.
        */    
        u_char registerByte = *(instruction+11);
        long long* registerPointer = getRegisterPointer(registerByte);
        if (registerPointer == NULL){
            puts("Execution Aborted, couldn't parse registers.");
            exit(1);
        }
        writeValue = *registerPointer;
    }else if(valSpecifier == 2){
        /*
        Read 8 bytes and store them as the write Value
        */
        writeValue = *((long long*)(instruction+4));
        /*
        We read it in the wrong byte order so we have to change it here.
        */
        writeValue = __builtin_bswap64(writeValue);
    }else if(valSpecifier == 3){
        /*
        First get the value of the register (stored in least significant byte)
        and then check if it is inbounds of stack and/or heap. Later read
        8 bytes from this address and store them in writeValue.
        */
        u_char registerByte = *(instruction+11);
        long long* registerPointer = getRegisterPointer(registerByte);
        if (registerPointer == NULL){
            puts("Execution Aborted, couldn't parse registers.");
            exit(1);
        }
        long long registerValue = *registerPointer;
       
        /*
        Checking whether the read is out of bounds or not.
        */ 
        if (registerValue >= 0 && registerValue+8 <= STACK_SIZE+HEAP_SIZE){
            writeValue = *((long long*)(MEMORY+registerValue));
        }else{
            printf("Out of bounds read to %lld detected!\n",registerValue);
            exit(1);
        }
    }else{
        printf("Read invalid valueSpecifier in mov instruction (second part)\
[%d]\n",valSpecifier);
        exit(1);
    }

    /*
    Checking the write-to parameter
    */
    if (specifier == 1){
        // Write to the register
        *firstRegister = writeValue;

    }else if(specifier == 3){
        // Write to register location
        
        long long registerValue = *firstRegister;
        if (registerValue >= 0 && registerValue+8 <= STACK_SIZE+HEAP_SIZE){
            // Performing write operation 
            *((long long*)(MEMORY+registerValue)) = writeValue;
        }else{
            printf("Out of bounds write to %lld detected!\n",registerValue);
            exit(1);
        }
    }else{
        printf("Read invalid specifier in mov instruction (first part)\
        [%d]\n",specifier);
        exit(1);
    }
    
    return 12;
}

int jmpInstruction(u_char* instruction, unsigned long codeSize){
    /*
    Returns the new value for the instruction pointer.
    */
    int jumpAddress = *((int*)(instruction+1));
     
    /*
    Making sure you can only jump in the region the loaded code is in.
    */
    if (jumpAddress <= codeSize){
        return jumpAddress;        
    }
    printf("Prohibited invalid jump to %d. (Instruction starts @ %p with\
 codeSize of %lu\n",jumpAddress,instruction,codeSize);
    exit(1);
}

int jneInstruction(u_char* instruction,unsigned long codeSize){
    /*
    Returns either the new rip if the zero flag was NOT set or -1 telling the
    executor to just increment rip by 5 (the size of je/jne/jg/jl) 
    */

    /*
    Check if zero flag is set
    */
    long long* flags = getRegisterPointer(FLAGS_REGISTER_BYTECODE);
    
    if (*flags & 1) {
        return -1; 
    }else{
        return jmpInstruction(instruction,codeSize);
    }
}

int jeInstruction(u_char* instruction,unsigned long codeSize){
    /*
    Returns either the new rip if the zero flag was set or -1 telling the
    executor to just increment rip by 5 (the size of je/jne/jg/jl) 
    */

    /*
    Check if zero flag is set
    */
    long long* flags = getRegisterPointer(FLAGS_REGISTER_BYTECODE);
    
    if (*flags & 1) {
        return jmpInstruction(instruction,codeSize);
    }else{
        return -1; 
    }
}
int jlInstruction(u_char* instruction,unsigned long codeSize){
    /*
    Returns either the new rip if the LESS flag was set or -1 telling the
    executor to just increment rip by 5 (the size of je/jne/jg/jl) 
    */

    /*
    Check if LESS flag is set
    */
    long long* flags = getRegisterPointer(FLAGS_REGISTER_BYTECODE);
    
    if ((*flags>>2) & 1) {
        return jmpInstruction(instruction,codeSize);
    }else{
        return -1; 
    }
}

int jgInstruction(u_char* instruction,unsigned long codeSize){
    /*
    Returns either the new rip if the GREATER flag was set or -1 telling the
    executor to just increment rip by 5 (the size of je/jne/jg/jl) 
    */

    /*
    Check if GREATER flag is set
    */
    long long* flags = getRegisterPointer(FLAGS_REGISTER_BYTECODE);
    
    if ((*flags>>1) & 1) {
        return jmpInstruction(instruction,codeSize);
    }else{
        return -1; 
    }
}

int pushInstruction(u_char* instruction){
    /*
    Pushes the 8 byte value stored in the provided register onto the stack
    (at rsp) and increments rsp by 8.
    Returns 2, the length of the instruction
    */ 

    /*
    Check if enough space for the variable is remaining
    */

    long long* rspPointer = getRegisterPointer(RSP_REGISTER_CODE);
    if (*rspPointer+8 > STACK_SIZE){
        puts("Stack full, about to be oveflowed by push instruction.");
        exit(1);
    }

    /*
    Get the value of the register to be pushed
    Create space on the stack for the variable
    */
    u_char registerByteCode = *(instruction+1);
    long long* registerPointer = getRegisterPointer(registerByteCode);
    long long registerValue = *registerPointer;

    *(rspPointer) = registerValue;
    *(rspPointer) += 8;

    return 2;
}

int popInstruction(u_char* instruction){
    /*
    Checks if rsp is bigger than 8, if so it pops the value at rsp into 
    the first provided register and then decrements rsp by 8.
    returns 2, length of instruction (with opcode)
    */
    long long* rspPointer = getRegisterPointer(RSP_REGISTER_CODE);
    if ()




    return 2;
}

void executeInstruction(u_char* instruction,s_registers* registers,
                        unsigned long codeSize){
    /*
    instruction is a pointer starting at the instruction we are executing.
    `instruction` shall be code+rip. After execution we increment rip by 
    the length of the previously executed instruction
    CodeSize is needed for the jumps to check whether the jump is performed
    in bounds of the loaded code to prevent exploits of the executer.
    */
    u_char opCode = *(instruction);

    int lenInstruction;
    int newRIP = 0;
    switch(opCode){
        /* push */
        case 0x10:
            break;
        /* pop */
        case 0x11:
            break;
        /* add */
        case 0x12:
            lenInstruction = addInstruction(instruction);
            *(registers->rip) = *(registers->rip) + lenInstruction;
            break;
         /* sub */
        case 0x13:
            lenInstruction = subInstruction(instruction);
            *(registers->rip) = *(registers->rip) + lenInstruction;
            break;   
        /* and  */
        case 0x14:
            lenInstruction = andInstruction(instruction);
            *(registers->rip) = *(registers->rip) + lenInstruction;
            break;
        /* mul */
        case 0x15:
            lenInstruction = mulInstruction(instruction);
            *(registers->rip) = *(registers->rip) + lenInstruction;
            break;
        /* div */
        case 0x16:
            lenInstruction = divInstruction(instruction);
            *(registers->rip) = *(registers->rip) + lenInstruction;
            break;
        /* xor */
        case 0x17:
            lenInstruction = xorInstruction(instruction);
            *(registers->rip) = *(registers->rip) + lenInstruction;
            break;
        /* mov */
        case 0x18:
            lenInstruction = movInstruction(instruction);
            *(registers->rip) = *(registers->rip) + lenInstruction;
            break;
        /* cmp */
        case 0x19:
            break;
        /* je */
        case 0x20:
            newRIP = jeInstruction(instruction,codeSize);
            if (newRIP == -1){
                *(registers->rip) += JUMP_INSTRUCTIONS_SIZE;
            }else{
                *(registers->rip) = newRIP;
            }
            break;
        /* jne */
        case 0x21:
            newRIP = jneInstruction(instruction,codeSize);
            if (newRIP == -1){
                *(registers->rip) += JUMP_INSTRUCTIONS_SIZE;
            }else{
                *(registers->rip) = newRIP;
            }
            break;
        /* jmp */
        case 0x22:
            newRIP = jmpInstruction(instruction,codeSize);
            *(registers->rip) = (long long)newRIP;
            break;
        /* jg */
        case 0x23:
            newRIP = jgInstruction(instruction,codeSize);
            if (newRIP == -1){
                *(registers->rip) += JUMP_INSTRUCTIONS_SIZE;
            }else{
                *(registers->rip) = newRIP;
            }
            break;
        /* jl */
        case 0x24:
            newRIP = jlInstruction(instruction,codeSize);
            if (newRIP == -1){
                *(registers->rip) += JUMP_INSTRUCTIONS_SIZE;
            }else{
                *(registers->rip) = newRIP;
            }
            break;
        /* syscall */
        case 0x25:
            break;
        default:
            printf("Opcode (0x%x) is invalid!\n",opCode);
            exit(1);

    }
 return;
    
}

int execute_assembly(u_char* code,s_registers* registers,char* filename){
    /*
    First skips shebang then 
    Execute the instruction at the current instruction pointer forever.
    Instruction with exit syscall can quit program/loop
    */

    /* Getting rid of shebang */
    if (code[0] == 0x23 && code[1] == 0x21){
        while (*(code) != 0xa) code++;
    }
    unsigned long codeSize = getFileSize(filename);

    while (true){
       executeInstruction(code+*(registers->rip),registers,codeSize);
    }

}

int main(int argc, char* argv[]){

    s_registers* registers = initRegisterLookup();
    char filename[] = DEFAULT_INPUT_FILE;
    u_char* byteCode = loadByteCode(filename);
    execute_assembly(byteCode,registers,filename);
  
    return 0;
}


