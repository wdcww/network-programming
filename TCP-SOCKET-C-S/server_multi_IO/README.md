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



## 4 epoll reactor

