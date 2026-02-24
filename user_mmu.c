#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ====================== 1. 核心定义（对应示意图） ======================
#define PAGE_SIZE        4096    // 4KB 页大小（12位偏移）
#define PAGE_SHIFT       12      // 页内偏移位数
#define PTE_PER_PAGE     1024    // 每个PT有1024个PTE（10位索引）
#define DTE_PER_DIR      1024    // 每个DT有1024个DTE（10位索引）

// 虚拟地址拆分掩码/移位（32位）
#define DTE_INDEX_MASK   0xFFC00000  // 高10位：DTE索引 (22-31位)
#define PTE_INDEX_MASK   0x003FF000  // 中间10位：PTE索引 (12-21位)
#define OFFSET_MASK      0x00000FFF  // 低12位：页内偏移 (0-11位)
#define DTE_INDEX_SHIFT  22          // 右移22位取DTE索引
#define PTE_INDEX_SHIFT  12          // 右移12位取PTE索引

// DTE/PTE 属性位（低12位）
#define PRESENT_BIT      (1 << 0)    // 存在位：1=映射有效，0=缺页
#define RW_BIT           (1 << 1)    // 读写位：1=可写，0=只读
#define US_BIT           (1 << 2)    // 用户态位：1=用户可访问，0=仅内核

// 2. DTE结构（对应示意图中的DTE）：指向PT的物理页帧
typedef struct {
    uint32_t pt_pfn     : 20;  // PT的物理页帧号（Page Frame Number）
    uint32_t attrs      : 12;  // 属性位（Present/RW/US等）
} __attribute__((packed)) DTE_t;

// 3. PTE结构（对应示意图中的PTE）：指向物理内存页的页帧
typedef struct {
    uint32_t page_pfn   : 20;  // 物理页帧号
    uint32_t attrs      : 12;  // 属性位
} __attribute__((packed)) PTE_t;

// 4. 模拟物理内存（简化：用数组模拟，大小16MB）
#define PHYS_MEM_SIZE    (16 * 1024 * 1024)
uint8_t physical_mem[PHYS_MEM_SIZE] = {0};

// 5. 模拟CR3寄存器（MMU_DTE_ADDR）：存放DT的物理页帧号
uint32_t CR3 = 0;

// ====================== 2. 辅助函数：物理地址 ↔ 指针转换 ======================
// 物理地址转虚拟指针（模拟MMU映射）
static void* phys_to_virt(uint32_t phys_addr) {
    if (phys_addr >= PHYS_MEM_SIZE) return NULL;
    return &physical_mem[phys_addr];
}

// 虚拟指针转物理地址
static uint32_t virt_to_phys(void* virt_addr) {
    uintptr_t addr = (uintptr_t)virt_addr - (uintptr_t)physical_mem;
    return (uint32_t)addr;
}

// ====================== 3. 初始化页表（构建DT/DTE → PT/PTE链路） ======================
// 创建DT并初始化一个DTE
static int init_page_directory(uint32_t dt_pfn) {
    // 1. 计算DT的物理地址
    uint32_t dt_phys = dt_pfn << PAGE_SHIFT;
    DTE_t* dt = (DTE_t*)phys_to_virt(dt_phys);
    if (!dt) return -1;

    // 2. 清空DT
    memset(dt, 0, DTE_PER_DIR * sizeof(DTE_t));

    // 3. 分配PT的物理页帧（比如分配第100个页帧）
    uint32_t pt_pfn = 100;
    uint32_t pt_phys = pt_pfn << PAGE_SHIFT;
    PTE_t* pt = (PTE_t*)phys_to_virt(pt_phys);
    if (!pt) return -1;

    // 4. 清空PT
    memset(pt, 0, PTE_PER_PAGE * sizeof(PTE_t));

    // 5. 初始化一个DTE（比如DTE索引=0）：指向上述PT
    dt[0].pt_pfn = pt_pfn;
    dt[0].attrs = PRESENT_BIT | RW_BIT | US_BIT;  // 存在+可写+用户可访问

    // 6. 初始化一个PTE（比如PTE索引=0）：指向物理页帧200
    uint32_t page_pfn = 200;
    pt[0].page_pfn = page_pfn;
    pt[0].attrs = PRESENT_BIT | RW_BIT | US_BIT;

    // 7. 设置CR3：指向DT的物理页帧
    CR3 = dt_pfn;

    printf("=== 页表初始化完成 ===\n");
    printf("CR3(MMU_DTE_ADDR) = 0x%08x (DT页帧号=%d)\n", CR3 << PAGE_SHIFT, dt_pfn);
    printf("DTE[0] → PT页帧号=%d (物理地址=0x%08x)\n", pt_pfn, pt_phys);
    printf("PTE[0] → 物理页帧号=%d (物理地址=0x%08x)\n", page_pfn, page_pfn << PAGE_SHIFT);
    printf("========================\n\n");

    return 0;
}

// ====================== 4. 模拟MMU：遍历DTE/PTE完成地址翻译 ======================
// 输入虚拟地址，输出物理地址（返回-1表示缺页）
static uint32_t mmu_translate(uint32_t virt_addr) {
    // 步骤1：拆分虚拟地址（对应示意图第一步）
    uint32_t dte_index = (virt_addr & DTE_INDEX_MASK) >> DTE_INDEX_SHIFT;
    uint32_t pte_index = (virt_addr & PTE_INDEX_MASK) >> PTE_INDEX_SHIFT;
    uint32_t offset    = virt_addr & OFFSET_MASK;

    printf("=== 虚拟地址拆分 ===\n");
    printf("虚拟地址: 0x%08x\n", virt_addr);
    printf("DTE索引: %d (高10位)\n", dte_index);
    printf("PTE索引: %d (中间10位)\n", pte_index);
    printf("页内偏移: 0x%04x (低12位)\n", offset);
    printf("====================\n\n");

    // 步骤2：从CR3找到DT，读取对应的DTE（对应示意图DT → DTE）
    uint32_t dt_phys = CR3 << PAGE_SHIFT;
    DTE_t* dt = (DTE_t*)phys_to_virt(dt_phys);
    if (!dt) {
        printf("错误：DT物理地址无效！\n");
        return (uint32_t)-1;
    }

    DTE_t dte = dt[dte_index];
    if (!(dte.attrs & PRESENT_BIT)) {
        printf("缺页异常：DTE[%-3d] Present位为0（未映射）\n", dte_index);
        return (uint32_t)-1;
    }

    // 步骤3：从DTE找到PT，读取对应的PTE（对应示意图DTE → PT → PTE）
    uint32_t pt_phys = dte.pt_pfn << PAGE_SHIFT;
    PTE_t* pt = (PTE_t*)phys_to_virt(pt_phys);
    if (!pt) {
        printf("错误：PT物理地址无效！\n");
        return (uint32_t)-1;
    }

    PTE_t pte = pt[pte_index];
    if (!(pte.attrs & PRESENT_BIT)) {
        printf("缺页异常：PTE[%-3d] Present位为0（未映射）\n", pte_index);
        return (uint32_t)-1;
    }

    // 步骤4：拼接物理地址（PTE → 物理页 + 偏移）
    uint32_t page_phys = pte.page_pfn << PAGE_SHIFT;
    uint32_t phys_addr = page_phys + offset;

    printf("=== 地址翻译完成 ===\n");
    printf("DTE[%-3d] → PT物理地址: 0x%08x\n", dte_index, pt_phys);
    printf("PTE[%-3d] → 物理页地址: 0x%08x\n", pte_index, page_phys);
    printf("最终物理地址: 0x%08x (物理页+偏移)\n", phys_addr);
    printf("====================\n\n");

    return phys_addr;
}

// ====================== 5. 测试主函数 ======================
int main() {
    // 1. 初始化页表：分配DT的物理页帧为50
    if (init_page_directory(50) != 0) {
        printf("页表初始化失败！\n");
        return -1;
    }

    // 2. 测试1：有效虚拟地址（DTE=0, PTE=0, 偏移=0x123）
    printf("===== 测试1：有效虚拟地址 0x00000123 =====\n");
    uint32_t virt_addr1 = 0x00000123;
    uint32_t phys_addr1 = mmu_translate(virt_addr1);
    if (phys_addr1 != (uint32_t)-1) {
        printf("✅ 翻译成功：虚拟地址0x%08x → 物理地址0x%08x\n\n", virt_addr1, phys_addr1);
    } else {
        printf("❌ 翻译失败\n\n");
    }

    // 3. 测试2：无效虚拟地址（DTE=0, PTE=100，未初始化）
    printf("===== 测试2：无效虚拟地址 0x000FF123 =====\n");
    uint32_t virt_addr2 = 0x000FF123;  // PTE索引=100，未初始化
    uint32_t phys_addr2 = mmu_translate(virt_addr2);
    if (phys_addr2 != (uint32_t)-1) {
        printf("✅ 翻译成功：虚拟地址0x%08x → 物理地址0x%08x\n\n", virt_addr2, phys_addr2);
    } else {
        printf("❌ 翻译失败\n\n");
    }

    // 4. 测试3：无效虚拟地址（DTE=1，未初始化）
    printf("===== 测试3：无效虚拟地址 0x00400123 =====\n");
    uint32_t virt_addr3 = 0x00400123;  // DTE索引=1，未初始化
    uint32_t phys_addr3 = mmu_translate(virt_addr3);
    if (phys_addr3 != (uint32_t)-1) {
        printf("✅ 翻译成功：虚拟地址0x%08x → 物理地址0x%08x\n\n", virt_addr3, phys_addr3);
    } else {
        printf("❌ 翻译失败\n\n");
    }

    return 0;
}

////////////
ubuntu20@NYX:~/opt/linux_sdk/kernel/Linux_driver/INPUT_DEV$ ./a.out
=== 页表初始化完成 ===
CR3(MMU_DTE_ADDR) = 0x00032000 (DT页帧号=50)
DTE[0] → PT页帧号=100 (物理地址=0x00064000)
PTE[0] → 物理页帧号=200 (物理地址=0x000c8000)
========================

===== 测试1：有效虚拟地址 0x00000123 =====
=== 虚拟地址拆分 ===
虚拟地址: 0x00000123
DTE索引: 0 (高10位)
PTE索引: 0 (中间10位)
页内偏移: 0x0123 (低12位)
====================

=== 地址翻译完成 ===
DTE[0  ] → PT物理地址: 0x00064000
PTE[0  ] → 物理页地址: 0x000c8000
最终物理地址: 0x000c8123 (物理页+偏移)
====================

✅ 翻译成功：虚拟地址0x00000123 → 物理地址0x000c8123

===== 测试2：无效虚拟地址 0x000FF123 =====
=== 虚拟地址拆分 ===
虚拟地址: 0x000ff123
DTE索引: 0 (高10位)
PTE索引: 255 (中间10位)
页内偏移: 0x0123 (低12位)
====================

缺页异常：PTE[255] Present位为0（未映射）
❌ 翻译失败

===== 测试3：无效虚拟地址 0x00400123 =====
=== 虚拟地址拆分 ===
虚拟地址: 0x00400123
DTE索引: 1 (高10位)
PTE索引: 0 (中间10位)
页内偏移: 0x0123 (低12位)
====================

缺页异常：DTE[1  ] Present位为0（未映射）
❌ 翻译失败
