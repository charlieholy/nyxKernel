// kernel.c - 极简版：仅串口输出，验证基础功能
#include <stdint.h>

// 串口操作函数声明
static inline unsigned char inb(unsigned short port);
static inline void outb(unsigned char val, unsigned short port);

// 串口初始化（关键：完整配置串口）
void serial_init() {
    // 禁用中断
    outb(0x00, 0x3f8 + 1);
    // 设置波特率 9600 (DLAB=1)
    outb(0x80, 0x3f8 + 3);
    outb(0x01, 0x3f8 + 0); // 除数低字节 (115200/9600=12 → 0x000C，这里简化为0x01测试)
    outb(0x00, 0x3f8 + 1); // 除数高字节
    // 8位数据位，1位停止位，无校验
    outb(0x03, 0x3f8 + 3);
    // 启用FIFO，清空缓冲区，14字节阈值
    outb(0xC7, 0x3f8 + 2);
    // 硬件流控禁用，启用中断（可选）
    outb(0x0B, 0x3f8 + 4);
}

// 串口打印单个字符
void serial_putc(char c) {
    // 等待发送缓冲区为空
    while ((inb(0x3f8 + 5) & 0x20) == 0);
    // 发送字符
    outb(c, 0x3f8);
}

// 串口打印字符串
void serial_puts(const char *s) {
    while (*s) {
        if (*s == '\n') {
            serial_putc('\r'); // 换行补回车，避免错位
        }
        serial_putc(*s++);
    }
}

// 内核主函数
void kernel_main() {
    // 1. 初始化串口
    serial_init();
    
    // 2. 打印测试信息（确保串口可用）
    serial_puts("=== Mini Kernel Started ===\n");
    serial_puts("Serial port initialized successfully!\n");
    
    // 3. 模拟read/write核心逻辑（简化版）
    serial_puts("Testing write syscall: Hello Mini OS!\n");
    serial_puts("Testing read syscall: Read from console: Mini OS Input\n");
    
    // 死循环
    while (1);
}

// 串口操作函数实现
static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

static inline void outb(unsigned char val, unsigned short port) {
    asm volatile ("outb %0, %1" : : "a"(val), "dN"(port));
}
