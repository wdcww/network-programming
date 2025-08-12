# 多路I/O复用

虽然，多进程（../server_mp/）和多线程（../server_mt/）的两个版本的server端程序较../server/已经高效了不少【指可以并发】，
但是，它们是需要工作在用户态的服务器程序亲自去监听，然后有client请求时进行后续工作。
也就是说，../server_mp/server和../server_mt/server都是亲自listen，有了一个来连接的client，server亲自去创建一个cfd给这个client。

这样的机制没有高效地利用系统资源进行并发处理，也没有利用了内核的强大能力。
而且，服务器程序不可能将大部分的资源放在通信上，应当有自己的其他的业务。

server_multi_IO/ 下的几种版本的server程序，相当于是请了一个秘书专门来负责监听。下面展开说说select、poll、epoll、epoll reactor，
它们都是都是将文件描述符的监视放在内核态的机制，使得在用户态的服务器程序可以进行其他操作无需阻塞等待 I/O 事件。

## 1 select
*  与之前相同的是，服务器端调用 socket() 创建一个lfd，然后bind()到一个特定的地址和端口，最后使用listen()开始监听连接请求。

* 不同的是，服务器将lfd添加到一个文件描述符集合，调用 select 函数，将控制权交给内核，内核会监视 集合 中的文件描述符是否有事件发生。
```
select 函数在内核中运行，服务器将监听的 lfd 交给 select 来监视。
当有新的连接请求到达时，select会通知服务器，服务器再调用accept创建一个新的连接套接字。
这个流程保证了服务器能够同时监视多个文件描述符并处理 I/O 事件。
```
_在内核态_，select进行文件描述符监视，这样服务器在用户态可以释放资源用于其他业务逻辑的处理。

_在用户态_，服务器可以进行其他操作，无需阻塞等待 I/O 事件。

### 1.1 select()函数原型：
```
int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout);
```
1 _select()返回值_ ：返回三个文件描述符集合中的总事件数。

2 _nfds_ ：需要传入的值是目前 _监听_ 的所有文件描述符中最大的文件描述符+1，当select内部做一个循环时，这个玩意相当于一个循环上限的存在。

3 _fd_set* readfds_ 、 _fd_set* writefds_ 、 _fd_set* exceptfds_ ： 是上面提到的文件描述符集合，底层是位图的形式，三个都是传入传出参数，

传入时是自己想要监听的一些文件描述符，传出时是真的有对应行为（读、写、异常）的文件描述符集合。

4 _struct timeval* timeout_ ：所指向的struct timeval是一个包含两个成员的结构体，struct timeval{long tv_sec;  long tv_usec; }; ，

若timeout传入NULL，则会永远等下去；若传入所设置的tv_sec，tv_usec，则等待设置的固定时间；若传入0，则检查描述符后立即返回。

```
                                                                                lfd
                                                                               / 
                                           cfd=Accept()                      3  
|  0   |                           | server |————————————> |     内核     | /__4__ | client1（cfd1）|
|  1   |                                                   |   (select)   | \ 
|  2   |                                                                     5
|  3   | lfd                                                                   \
|  4   | cfd1                                                                  | client2（cfd2）|
|  5   | cfd2
|  ... | ...
| 1023 | ...                        readfds（读集合） :   |  4,5  |  监听“4”“5”是否有读事件 
                                    writefds（写集合）：  |   3   |  监听“3”是否有写事件
文件描述符表                        exceptfds(异常集合)： |   4,5  | → 监听“4”“5”是否有异常   
```

如上图所示，一个程序启动之后，左侧的文件描述符表中的“0”“1”“2”都被占用了，后续只能使用“3”~“1023” 。

select()使用了“3”去做listen，第一个client1占用“4”，client2占用“5”，则此时调用select()函数时，nfds需要传入5+1=6。

三个集合则是传入自己想监听的事件，图中右侧给了例子



## 2 poll

如果文件描述符在文件描述符表中比较的离散时，就需要对select()做一些提升，poll()就是对于select()的提升，但是并没有提升太多，这里只是为了过渡，为引出epoll()。

poll() 函数的作用是同时监控多个文件描述符，因此需要传入一个包含多个 struct pollfd 元素的数组，每个元素对应一个要监控的文件描述符及其事件配置。

### 2.1 poll()函数原型
```
int poll(struct pollfd* fds, nfds_t nfds, int timeout);
```

1 _poll()返回值_ ：返回满足对应监听事件的文件描述符的总个数。返回值为正整数时，表示有多少个文件描述符发生了事件（即 struct pollfd 数组中 revents 字段非 0 的元素数量）。

返回 0 表示在指定的 timeout 时间内，没有任何文件描述符发生事件（超时返回）。返回 -1 表示 poll() 调用失败，此时会设置全局变量 errno 来指示具体错误原因。

2 _struct pollfd* fds_ ：fds是指向一个 struct pollfd 结构体数组 的首地址的指针变量，
```
struct pollfd {
    int   fd;         // 要监控的文件描述符（如 socket、文件句柄等）
    short events;     // 期望监控的事件（如 POLLIN 表示等待可读）
    short revents;    // 实际发生的事件（由 poll() 函数填充）
};

```

3 _nfds_ ：nfds 表示数组中元素的数量（即要监控的文件描述符总数）。

4 _timeout_ : 超时时长(ms)。若传入正数：超时时间（毫秒）；若传入0：立即返回，不阻塞；若传入1：无限期等待，直到有事件发生。

### 2.2 与select()相比
```
1. 文件描述符的表示方式
select：
通过三个独立的文件描述符集合（readfds、writefds、exceptfds）分别表示需要监控的 “可读”“可写”“异常” 事件，每个集合本质是一个位图（bitmask）。
需使用 FD_SET、FD_CLR、FD_ISSET 等宏操作集合；
最大监控数量受系统限制（通常由 FD_SETSIZE 定义，默认 1024），超过该值会失效。
poll：
通过一个 struct pollfd 结构体数组表示所有监控对象，每个结构体包含：
fd：要监控的文件描述符；
events：期望监控的事件（如 POLLIN 可读、POLLOUT 可写）；
revents：实际发生的事件（由内核填充）。
无固定上限（仅受系统内存和进程文件描述符限制），可监控更多文件描述符。

2. 输入输出的处理方式
select：
输入的文件描述符集合是 “值传递”，内核会修改这些集合，将未发生事件的文件描述符从集合中清除；
因此，每次调用 select 前必须重新初始化集合（否则会丢失之前的监控配置），操作繁琐。
poll：
输入的 struct pollfd 数组是 “引用传递”，内核仅修改 revents 字段（记录实际事件），events 字段保持不变；
因此，poll 不需要重新初始化数组，可直接复用，操作更简洁。

3. 超时时间的处理
select：
超时时间通过 struct timeval 结构体传入，包含秒（tv_sec）和微秒（tv_usec）。
内核会修改该结构体，将剩余未超时的时间写入（部分系统实现），因此每次调用前可能需要重新设置超时值。
poll：
超时时间通过一个 int 类型参数直接传入，单位为毫秒（-1 表示无限等待，0 表示立即返回）。
内核不会修改该参数，无需重新设置，使用更直观。

4. 性能差异
select：
每次调用需遍历三个文件描述符集合（位图），当监控数量较多时（接近 FD_SETSIZE），遍历效率低；
位图操作（FD_SET 等）在用户态和内核态之间的复制成本随监控数量增加而上升。
poll：
基于数组遍历，无需处理位图，当监控数量较多时，遍历效率略高于 select；
结构体数组的复制成本更线性，适合监控大量文件描述符的场景（但仍不如 epoll 高效）。

5. 错误处理
select：
若监控的文件描述符被关闭，select 会将其视为 “可读” 事件（FD_ISSET 会返回真），需手动检查 read 操作的返回值（如 ECONNRESET）来判断是否为错误。
poll：
若文件描述符出错（如连接关闭），内核会在 revents 中设置 POLLERR 或 POLLHUP 标志，可直接通过 revents 判断错误，处理更明确。
```

## 3 epoll

核心优势是支持海量文件描述符（FD）高效监控，并通过 “事件驱动” 模式大幅降低系统开销。

1 高效的事件通知：内核通过 “就绪链表” 直接返回发生事件的 FD，无需像 select/poll 那样遍历全部 FD，性能不受监控数量影响（适合 10 万 + 并发连接）。

2 按需注册：只需通过 epoll_ctl 注册一次 FD，后续可重复使用，无需像 select 那样每次调用都重新初始化 FD 集合。

3 支持边缘触发（ET）和水平触发（LT）：水平触发（LT，默认）：只要 FD 有未处理的事件，epoll_wait 就会持续通知（与 select/poll 行为一致，易用性高）。边缘触发（ET）：仅在 FD 状态从 “无事件” 变为 “有事件” 时通知一次（需一次性处理完所有数据，效率更高，但编程复杂）。

4 无 FD 数量限制：仅受系统内存和进程可打开的最大 FD 数限制（可通过 ulimit 调整），远超 select 的 FD_SETSIZE（默认 1024）。

### epoll 的核心操作与数据结构
epoll 通过三个系统调用实现功能，依赖内核维护的两个关键数据结构：
红黑树：存储所有注册的 FD 及其事件（eventpoll 结构体），支持高效的添加 / 删除 / 修改操作。

就绪链表：存储发生了事件的 FD，用户态只需直接读取该链表即可获取事件，无需遍历全部 FD。

### 3.1 epoll_create：创建 epoll 实例
```
#include <sys/epoll.h>
int epoll_create(int size); // size 为历史参数，现代 Linux 已忽略，只需传 >0 的值
```
 _功能_：创建一个 epoll 实例（内核数据结构），返回一个 ** epoll 文件描述符（epfd）**，用于后续操作。

### 3.2 epoll_ctl：注册 / 修改 / 删除监控事件
```
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
```

参数：

1 _epfd_ ：epoll_create 返回的实例描述符。

2 _op_ ：操作类型（EPOLL_CTL_ADD 注册、EPOLL_CTL_MOD 修改、EPOLL_CTL_DEL 删除）。

3 _fd_ ：需要监控的文件描述符（如 socket）。

4 _event_ ：struct epoll_event 结构体，指定关注的事件（如 EPOLLIN 可读）和用户数据（void *ptr）。
```
struct epoll_event {
    uint32_t events;  // 关注的事件（EPOLLIN、EPOLLOUT、EPOLLERR 等）
    epoll_data_t data; // 用户数据（通常存 FD 或自定义结构体指针）
};
typedef union epoll_data {
    void    *ptr;
    int      fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;
```

### 3.3 epoll_wait：等待事件发生
```
int epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout);
```

功能：等待注册的 FD 发生事件，将发生事件的 FD 及其信息填入 events 数组。

参数：

1 _epfd_ : 见上

2 _events_ ：用户态数组，用于接收就绪事件。

3 _maxevents_：events 数组的最大长度（每次最多处理的事件数）。

4 _timeout_ ：超时时间（毫秒，-1 无限等待，0 立即返回）。

5 _返回值_ ：发生事件的 FD 数量（>0）、超时（0）、出错（-1）。


```
示例：用 epoll 监控 socket 可读事件
#include <sys/epoll.h>
#include <stdio.h>
#include <unistd.h>

int main() {
    int epfd = epoll_create(1); // 创建 epoll 实例
    struct epoll_event ev, events[10];
    int listen_fd = ...; // 假设已创建并绑定监听 socket

    // 注册监听 socket 的可读事件（LT 模式）
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);

    while (1) {
        // 等待事件，最多返回 10 个事件，超时 500 毫秒
        int nfds = epoll_wait(epfd, events, 10, 500);
        if (nfds == -1) {
            perror("epoll_wait failed");
            break;
        }

        // 处理就绪事件
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == listen_fd && (events[i].events & EPOLLIN)) {
                // 监听 socket 可读，处理新连接
                printf("New connection incoming\n");
            }
        }
    }

    close(epfd);
    return 0;
}
```

## 4 epoll reactor

在./epoll_reactor/server.c中的实现的逻辑是一个基于 epoll 反应堆模式 的 “回声服务器”（收到什么数据就回复什么数据），

其设计思路：epoll ET 模式 + 非阻塞 I/O + 自定义结构体维护上下文 + 回调函数驱动事件处理。


### 4.1 核心逻辑梳理
整个服务器的工作流程可分为 _初始化→事件循环→事件处理→资源回收_ 四步，形成一个完整的 “反应堆” 闭环：
#### 4.1.1. 初始化阶段（main 函数前期）
全局资源初始化：

global_events 数组（大小 1025）初始化，所有元素状态设为 0（未监听）。

创建 epoll 实例（global_efd = epoll_create(...)），作为事件监控的核心。

监听 socket（lfd）初始化：
通过 initlistensocket 函数创建 lfd，设置为 非阻塞，并开启端口复用（SO_REUSEADDR）。
绑定端口（默认 9527）、监听连接（listen(lfd, 20)）。
将 lfd 关联到 global_events[1024]（数组最后一位专门用于 lfd），绑定回调函数 acceptconn（处理新连接），并通过 eventadd 加入 epoll 监听 EPOLLIN 事件（等待新连接）。

#### 4.1.2. 事件循环（main 函数核心）
服务器进入无限循环，不断处理两类任务：超时连接清理 和 就绪事件处理：
```
while (1) {
    // 1. 超时检查：每轮检查100个连接，超过60秒无活动则关闭
    for (i=0; i<100; i++, checkpos++) {
        if (global_events[checkpos].status == 1 && (now - last_active) >= 60) {
            eventdel(global_efd, &global_events[checkpos]); // 从epoll移除
            close(global_efd); // 关闭连接
        }
    }

    // 2. 等待事件就绪（超时1000ms，避免永久阻塞）
    int nfd = epoll_wait(global_efd, events, MAX_EVENTS+1, 1000);

    // 3. 处理就绪事件
    for (i=0; i<nfd; i++) {
        struct myevent_s* ev = (struct myevent_s*)events[i].data.ptr;
        if (ev->events & EPOLLIN) ev->call_back(...); // 读事件回调
        if (ev->events & EPOLLOUT) ev->call_back(...); // 写事件回调
    }
}
```

#### 4.1.3. 事件处理（三大回调函数）
回调函数是 “反应堆” 的 “反应” 核心，根据事件类型（新连接、读、写）自动触发：

| 回调函数   | 触发场景               | 核心逻辑                                                                                                                                                                                                 |
|------------|------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| acceptconn | lfd 触发 EPOLLIN       | 循环调用 accept 获取所有新连接（ET 模式要求），为每个 cfd 初始化 global_events 空闲位置，设置为非阻塞，绑定 recvdata 回调，加入 epoll 监听 `EPOLLIN | EPOLLET`。                                           |
| recvdata   | cfd 触发 EPOLLIN       | 从 epoll 移除 cfd，调用 recv 读取客户端数据；若读取成功，切换回调为 senddata，加入 epoll 监听 `EPOLLOUT | EPOLLET`；若连接关闭/出错，关闭 cfd 并释放资源。                                                   |
| senddata   | cfd 触发 EPOLLOUT      | 从 epoll 移除 cfd，调用 write 回复客户端数据；若发送成功，切换回调为 recvdata，加入 epoll 监听 `EPOLLIN | EPOLLET`；若暂时无法发送（EAGAIN），继续监听 EPOLLOUT；否则关闭 cfd。                               |

#### 4.1.4. 资源管理（eventadd /eventdel）

_eventadd_ ：封装 epoll_ctl 的 ADD/MOD 操作，根据 myevent_s.status 判断是新增（status=0）还是修改（status=1）事件，确保 epoll_event.data.ptr 指向 myevent_s 结构体（关联上下文）。

_eventdel_ ：封装 epoll_ctl 的 DEL 操作，将 myevent_s.status 设为 0（标记未监听），从 epoll 红黑树中移除该 fd，避免无效事件通知。

### 4.2 设计亮点

1. 所有操作（新连接、读写）均由事件触发，无主动轮询，CPU 利用率极高，符合高并发服务器设计原则。

2. ET 模式 + 非阻塞 I/O 结合：

lfd 和所有 cfd 均设为非阻塞（fcntl(F_SETFL, O_NONBLOCK)），避免 I/O 操作阻塞事件循环。

采用 ET 模式（EPOLLET），每个事件仅通知一次，减少 epoll 通知次数，提升性能。

3. 上下文完整绑定：
通过 epoll_event.data.ptr 关联 myevent_s 结构体，每个 fd 的缓冲区（buf）、状态（status）、回调函数等信息 “随身携带”，事件处理时无需额外查找，逻辑清晰。

4. 超时自动清理：
每轮循环检查 100 个连接的 last_active 时间，超过 60 秒无活动则自动关闭，避免僵尸连接占用资源。

5. 资源复用：
global_events 数组固定大小，通过 status 字段标记是否可用，避免动态内存分配的开销和泄漏风险。

