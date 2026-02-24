// 补充缺失的 stdint.h 头文件（核心修复1）
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// ====================== 1. 核心数据结构定义 ======================
// 模拟 epoll_event 结构体（epoll_ctl/epoll_wait 的事件参数）
typedef struct {
    uint32_t events;  // 监听的事件：EPOLLIN/EPOLLOUT
    int fd;           // 关联的文件描述符
} my_epoll_event_t;

// 红黑树节点（简化：用链表模拟红黑树，核心逻辑一致）
typedef struct rb_node {
    int fd;                     // 监听的fd
    my_epoll_event_t event;     // 该fd监听的事件
    struct rb_node *next;       // 链表下一个节点
} rb_node_t;

// 模拟 epoll 实例（对应内核中的 epoll_fd 对应的结构体）
typedef struct {
    rb_node_t *rb_root;         // 红黑树根节点（链表头）
    my_epoll_event_t *ready_list; // 就绪链表
    int ready_count;            // 就绪链表中fd的数量
    int max_ready;              // 就绪链表最大容量
} my_epoll_t;

// 事件定义（模拟 EPOLLIN/EPOLLOUT）
#define MY_EPOLLIN  0x01    // 读就绪
#define MY_EPOLLOUT 0x02    // 写就绪

// ====================== 2. 函数声明（核心修复2：提前声明所有函数） ======================
my_epoll_t *my_epoll_create(int size);
int my_epoll_ctl(my_epoll_t *ep, int op, int fd, my_epoll_event_t *event);
int my_epoll_wait(my_epoll_t *ep, my_epoll_event_t *events, int maxevents, int timeout);
void my_epoll_activate(my_epoll_t *ep, int fd, uint32_t events);

// 操作类型：EPOLL_CTL_ADD/EPOLL_CTL_MOD/EPOLL_CTL_DEL
#define MY_EPOLL_CTL_ADD 1
#define MY_EPOLL_CTL_MOD 2
#define MY_EPOLL_CTL_DEL 3

// ====================== 3. 模拟 epoll_create ======================
// 创建 epoll 实例（对应 epoll_create1(0)）
my_epoll_t *my_epoll_create(int size) {
    my_epoll_t *ep = (my_epoll_t *)malloc(sizeof(my_epoll_t));
    if (!ep) return NULL;

    ep->rb_root = NULL;
    ep->ready_list = (my_epoll_event_t *)malloc(sizeof(my_epoll_event_t) * size);
    ep->ready_count = 0;
    ep->max_ready = size;

    memset(ep->ready_list, 0, sizeof(my_epoll_event_t) * size);
    return ep;
}

// ====================== 4. 模拟 epoll_ctl（增删改监听fd） ======================
int my_epoll_ctl(my_epoll_t *ep, int op, int fd, my_epoll_event_t *event) {
    if (!ep || fd < 0) return -1;

    rb_node_t *node = NULL;
    rb_node_t *prev = NULL;

    // 1. 查找fd是否已存在于红黑树
    node = ep->rb_root;
    while (node) {
        if (node->fd == fd) break;
        prev = node;
        node = node->next;
    }

    // 2. 根据操作类型处理
    switch (op) {
        case MY_EPOLL_CTL_ADD: {
            // 新增fd：已存在则返回错误
            if (node) return -1;

            // 创建新节点
            rb_node_t *new_node = (rb_node_t *)malloc(sizeof(rb_node_t));
            if (!new_node) return -1;

            new_node->fd = fd;
            new_node->event = *event;
            new_node->next = NULL;

            // 加入红黑树（链表尾）
            if (!ep->rb_root) {
                ep->rb_root = new_node;
            } else {
                prev->next = new_node;
            }
            printf("[epoll_ctl ADD] fd=%d, events=0x%x\n", fd, event->events);
            break;
        }

        case MY_EPOLL_CTL_MOD: {
            // 修改fd的监听事件：不存在则返回错误
            if (!node) return -1;

            node->event = *event;
            printf("[epoll_ctl MOD] fd=%d, new events=0x%x\n", fd, event->events);
            break;
        }

        case MY_EPOLL_CTL_DEL: {
            // 删除fd：不存在则返回错误
            if (!node) return -1;

            // 从链表中移除节点
            if (prev) {
                prev->next = node->next;
            } else {
                ep->rb_root = node->next;
            }

            free(node);
            printf("[epoll_ctl DEL] fd=%d\n", fd);
            break;
        }

        default:
            return -1;
    }

    return 0;
}

// ====================== 5. 模拟 epoll_wait（等待就绪事件） ======================
int my_epoll_wait(my_epoll_t *ep, my_epoll_event_t *events, int maxevents, int timeout) {
    if (!ep || !events || maxevents <= 0) return -1;

    // 模拟超时等待（简化：直接返回就绪链表中的fd）
    if (timeout > 0) {
        printf("[epoll_wait] wait %d ms...\n", timeout);
        usleep(timeout * 1000);
    }

    // 拷贝就绪链表到用户态缓冲区
    int copy_count = ep->ready_count > maxevents ? maxevents : ep->ready_count;
    memcpy(events, ep->ready_list, sizeof(my_epoll_event_t) * copy_count);

    // 清空就绪链表（模拟内核态就绪链表消费后清空）
    int ret = copy_count;
    ep->ready_count = 0;
    memset(ep->ready_list, 0, sizeof(my_epoll_event_t) * ep->max_ready);

    return ret;
}

// ====================== 6. 模拟内核回调：将fd加入就绪链表 ======================
// 模拟 fd 就绪（如网卡收到数据、文件可写），内核调用此函数将fd加入就绪链表
void my_epoll_activate(my_epoll_t *ep, int fd, uint32_t events) {
    if (!ep || fd < 0 || ep->ready_count >= ep->max_ready) return;

    // 查找该fd是否在监听列表中
    rb_node_t *node = ep->rb_root;
    while (node) {
        if (node->fd == fd) break;
        node = node->next;
    }

    // 未监听该fd则忽略
    if (!node) return;

    // 检查事件是否匹配（只加入监听的事件）
    if ((node->event.events & events) == 0) return;

    // 将fd加入就绪链表
    ep->ready_list[ep->ready_count].fd = fd;
    ep->ready_list[ep->ready_count].events = events;
    ep->ready_count++;

    printf("[epoll_activate] fd=%d ready, events=0x%x (ready count=%d)\n", 
           fd, events, ep->ready_count);
}

// ====================== 7. 测试 Demo ======================
int main() {
    // 1. 创建 epoll 实例（监听最大10个fd）
    my_epoll_t *ep = my_epoll_create(10);
    if (!ep) {
        perror("my_epoll_create failed");
        return -1;
    }

    // 2. 添加监听的fd（模拟监听 3个fd：1/2/3）
    my_epoll_event_t ev;
    ev.events = MY_EPOLLIN;
    ev.fd = 1;
    my_epoll_ctl(ep, MY_EPOLL_CTL_ADD, 1, &ev);

    ev.events = MY_EPOLLIN | MY_EPOLLOUT;
    ev.fd = 2;
    my_epoll_ctl(ep, MY_EPOLL_CTL_ADD, 2, &ev);

    ev.events = MY_EPOLLOUT;
    ev.fd = 3;
    my_epoll_ctl(ep, MY_EPOLL_CTL_ADD, 3, &ev);

    // 3. 模拟 fd 就绪（内核态触发：如fd=1可读，fd=2可写）
    printf("\n=== 模拟内核触发fd就绪 ===\n");
    my_epoll_activate(ep, 1, MY_EPOLLIN);   // fd=1 读就绪
    my_epoll_activate(ep, 2, MY_EPOLLOUT);  // fd=2 写就绪

    // 4. 调用 epoll_wait 获取就绪事件
    printf("\n=== 调用 epoll_wait 获取就绪fd ===\n");
    my_epoll_event_t ready_events[10];
    int n = my_epoll_wait(ep, ready_events, 10, 100); // 超时100ms
    printf("[epoll_wait] return %d ready fd(s)\n", n);

    // 5. 处理就绪事件
    for (int i = 0; i < n; i++) {
        printf("  fd=%d, ready events=0x%x -> ", ready_events[i].fd, ready_events[i].events);
        if (ready_events[i].events & MY_EPOLLIN) {
            printf("可读\n");
        } else if (ready_events[i].events & MY_EPOLLOUT) {
            printf("可写\n");
        }
    }

    // 6. 移除一个fd，再次测试
    printf("\n=== 移除 fd=3 后再次测试 ===\n");
    my_epoll_ctl(ep, MY_EPOLL_CTL_DEL, 3, NULL);
    my_epoll_activate(ep, 3, MY_EPOLLOUT); // fd=3已移除，不会加入就绪链表
    n = my_epoll_wait(ep, ready_events, 10, 50);
    printf("[epoll_wait] return %d ready fd(s)\n", n);

    // 7. 释放资源
    free(ep->ready_list);
    rb_node_t *node = ep->rb_root;
    while (node) {
        rb_node_t *tmp = node;
        node = node->next;
        free(tmp);
    }
    free(ep);

    return 0;
}
