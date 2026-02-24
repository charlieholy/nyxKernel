; kernel.asm - 极简版：Multiboot + 内核入口
[bits 32]
[section .text]

; Multiboot 头部
MULTIBOOT_HEADER_MAGIC  equ 0x1BADB002
MULTIBOOT_HEADER_FLAGS  equ 0x00000003
MULTIBOOT_HEADER_CHECKSUM equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

align 4
dd MULTIBOOT_HEADER_MAGIC
dd MULTIBOOT_HEADER_FLAGS
dd MULTIBOOT_HEADER_CHECKSUM

; 全局符号
global start
extern kernel_main

; 内核入口
start:
    call kernel_main
    jmp $
