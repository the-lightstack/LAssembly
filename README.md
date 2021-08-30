
# LAsm - Custom assembly Language

# ↓ Documentation
```
OP        CODE      NOTE                                LEN (oc+params)
–––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
push    -> 0x10     push reg                             2 bytes
pop     -> 0x11     OC +reg-byte                         2 bytes

add     -> 0x12     add reg,reg                          3 bytes
sub     -> 0x13     sub reg,reg                          3 bytes
and     -> 0x14     and reg,reg                          3 bytes
mul     -> 0x15     mul reg,reg                          3 bytes
div     -> 0x16     div reg,reg                          3 bytes
xor     -> 0x17     xor reg,reg                          3 bytes

mov     -> 0x18     mov [reg/reg-loc] + 1byte, [val/reg-loc/reg] 8bytes>12 Bytes
cmp     -> 0x19     cmp reg,reg                          3 bytes
je      -> 0x20     je <4byte addr>                      5 bytes
jne     -> 0x21     jne <4byte addr>                     5 bytes
jmp     -> 0x22     jmp <4byte addr>                     5 bytes
jg      -> 0x23     jg <4byte addr>                      5 bytes
jl      -> 0x24     jl <4byte addr>                      5 bytes
syscall -> 0x25     syscall                              1 byte

–––––––––––– Pre arg definition ––––––––––––––––

[reg]     = 0x1
[val]     = 0x2 // padded to 8 bytes
[reg-loc] = 0x3 // so it is a pointer


–––––––––––– Reg byte mapping  ––––––––––––––––

rax -> 0x50
rbx -> 0x51
rcx -> 0x52

rdi -> 0x53
rsi -> 0x54
rdx -> 0x55

rsp -> 0x56
rbp -> 0x57

rip -> 0x58  # Won't be used too much
rbf  -> 0x59 # Only used by pre-compiler to beaufiy stuff [1] 

flags -> 0x60

––––––––– Syscalls –––––––––––––––––––––––––

( Not a part of compilation )

exit        -> 0x80 [If RDI not 0, the RDI is printed]
write       -> 0x81
open        -> 0x82 
read        -> 0x83
exec?       -> 0x84

print       -> 0x86


–––––––––– Precompiler instructions ––––––––

return 
call
(also push and pop?)

–––––––––––– Internal Label Structure ––
Labels are stored in a char* with the format:
<label-name>\x00<4byte address> 
    ^               ^
Max 32 byte    

–––––––––––– FLAGS ––––––––––––
cmp sets either the ZERO, GREATER or LESS flag 

--- Table of flags ----
ZERO        =   1
GREATER     =   2
LESS        =   4

Internally compary OR's (|) the flags register with the flag

The flags get reset to zero before every function/action/opcode that also sets
them (cmp/sub/add/xor/and)

–––––––––––– STACK AND HEAP –––––––––––– 

The size of the stack and the heap will be defined at compile time and cannot
be changed later. Both will always be at the same position. The stack starts
at 0 and goes up to STACK_SIZE. The Heap starts immediately after (DANGEROUS?!)
and goes from STACK_SIZE+1 up to STACK_SIZE+HEAP_SIZE. It is up to the executer
to check that `push` and `pull` only access stack space and can't read/write 
from/to the heap.

The mov instruction only checks whether the read/write location is in between
zero and STACK_SIZE+HEAP_SIZE

–––––––––––– Other Syntax –––––––––––––––
Labels are decleared like `!main` and may not contain a space, max of 32 chars.
You can later reference this label in the jump instructions like `jne main`

You can only read 8 bytes from a certain stack/heap address, so it is smart to
read the value and then AND it with the amount of bytes you want.

    mov rax,*rbx
    mov rbf,0xffff
    and rax,rbf

To read two bytes at rbx (maybe add as macro?)


The executor functions for jne/jl/jg return -1 if they don't perform any jump, 
else they return the new rip location

!! IMPORTANT !!
Both the stack and the heap grow upwards, which means to create space on the 
stack you INCREMENT rsp instead of the usual substraction.

When writing strings, you have to start at the back of the string and write it 
in reverse order. Just like:
    0x000a48
To write a single 'H'

–––––––– Appendix –––––––––––––––––
[1] rbf will be used to change `cmp rax,0x1` (which is illegal) to a legal 
    instruction like `mov rbf,0x1; cmp rax,rbf`. Leaving out the direct 
    possibility to do this makes the opcodes MUCH cleaner and shorter

```
