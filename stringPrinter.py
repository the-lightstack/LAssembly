#!/usr/bin/env python3
import sys

# A simple script to convert strings to lasm syntax


def convert(s):
    # Append 0x00 then Split string up into 8 byte parts
    # The packs stay at the right position, only inside they get reversed in 
    # endianness 

    s = (bytes(s,"utf-8")+b"\x00")
    res = ""

    packs = []
    for i in range(0,len(s)-1,8):
        pack = s[i:i+8]
        packs.append(pack)

    print(packs)
    for i in packs:
        res += "0x"
        for j in i[::-1]:
            res += hex(j)[2:].zfill(2)
        res += "\n"

    return res

def fullStringPrint(s):
    # Get all the right byte codes
    result = ""
    codes = [i for i in convert(s).split("\n") if i is not ""]
    print("codes:",codes)
    result += "# Auto-Generated Print Function\n"
    for i in codes:
        result += f"mov rbf, {i}\n"
        result += "push rbf\n"

    result += "mov rax,0x86\n"
    result += f"mov rbf,{hex(len(codes)*8)}\n"
    result += "mov rcx,rsp\n"
    result += "sub rcx,rbf\n"
    result += "mov rdi,rcx\n"
    result += "syscall\n"
    
    return result

if __name__ == "__main__":
    print(fullStringPrint("Let's see if my code works!!\n"))
