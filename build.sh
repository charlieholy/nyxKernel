#!/bin/bash
set -e

echo "=== 1. 汇编编译 kernel.asm ==="
nasm -f elf32 kernel.asm -o kernel.asm.o

echo "=== 2. C语言编译 kernel.c ==="
gcc -m32 -c kernel.c -o kernel.c.o -ffreestanding -nostdlib -fno-builtin -Wall

echo "=== 3. 链接生成 kernel.elf ==="
ld -m elf_i386 -T link.ld -o kernel.elf kernel.asm.o kernel.c.o

echo "=== 4. 转换为二进制镜像 ==="
objcopy -O binary kernel.elf kernel.bin

echo "=== 5. 运行内核 ==="
qemu-system-i386 -kernel kernel.elf -nographic -serial mon:stdio
