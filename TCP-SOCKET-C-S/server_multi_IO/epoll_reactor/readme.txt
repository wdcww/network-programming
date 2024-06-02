  大名鼎鼎的 epoll reactor (epoll 反应堆)
不但 监听cfd的读事件，还要 监听cfd的写事件！！！

server 思路：
         【epoll ET模式】 + 【lfd、所有cfd都 非阻塞】 + 【利用void* ptr】


一、
   当向epoll_ctl() 添加、修改或删除一个事件时，要传递一个epoll_event的结构。
在epoll.h中定义的 struct epoll_event 如下：

    struct epoll_event
    {
     uint32_t events;	/* Epoll events */
     epoll_data_t data;	/* User data variable */
    };
  
其中的data是一个union epoll_data，如下：

  typedef union epoll_data
         {
            void *ptr;
            int fd;
            uint32_t u32;
            uint64_t u64;
         } epoll_data_t;

*如果定一个上述结构体的变量，即 struct epoll_event epv; 
一般只使用epv.events和epv.data.fd (在../epoll/server_epoll.c中就是这样)

*epv.data.ptr相当于是留出了一个可自定义的"拓展接口",
当epv.data.ptr额外关联了一些fd相关的数据,比如ptr指向一个包含fd信息的结构体(见"二、")，
那么就可以epoll_ctl(*,*,fd,&epv)


二、
   自己申明了一个struct myevent_s ，用于维护整个程序逻辑。

   struct myevent_s 
    { /*就绪文件描述符的相关信息*/
     int fd;                                         //要监听的文件描述符
     int events;                                     //对应的监听事件
     void* arg;                                      //泛型参数
     void (*call_back)(int fd,int events,void* arg); //回调函数
     int status;                                     //是否在监听(1：在红黑树上监听，0：不在监听)
     char buf[BUFLEN];                               //该描述符的buf[]
     int len;                                        //sizeof(buf)
     long last_active;                               //记录每次加入global_efd指向的红黑树的时间值
   };

维护逻辑：
   当向 epoll 注册事件时，通过 epoll_event 结构的 data 成员关联一些自定义数据。
用来在 epoll_wait 返回时能够方便地识别哪个文件描述符（fd）产生了事件，并包含了与该事件处理相关的其他信息。

   定义了该结构体的【全局】结构体数组global_events[MAX_EVENTS+1]，其中MAX_EVENTS+1=1024+1
   global_events[i] 记录第i个文件描述符对应的一系列信息，(i=0,1,...,1024)
global_events[0] ~ global_events[1023] : 这1024个去记录有连接的 cfd
                   global_events[1024] : 最后一个记录 lfd 


------------------------------------

epoll是Linux中的一个高效I/O事件通知机制，它允许应用程序监视多个文件描述符，
以查看这些文件描述符是否准备好进行读、写等操作，而无需使用传统的轮询方法。

   epoll 有两种触发模式：LT（Level Triggered）和 ET（Edge Triggered）。
LT（Level Triggered）模式是默认模式，它类似于传统的 select/poll 机制。
当某个文件描述符可读或可写时，epoll 会一直通知应用程序，直到应用程序读取或写入该文件描述符。
这可能导致一些不必要的唤醒和上下文切换，因为应用程序可能在第一次读取/写入后没有立即处理完所有数据，
但 epoll 仍然会继续通知它。

   ET（Edge Triggered）模式则更加高效。它只在文件描述符的状态从不可读/不可写变为可读/可写时通知一次。
这要求应用程序在接收到通知后，必须读取/写入所有可用的数据，直到没有更多数据可读/可写为止。否则，如果
应用程序没有处理完所有数据，并在下次 epoll_wait 调用之前再次有数据到达，那么这些新数据将不会被通知给应用程序，
直到下一个状态变化发生。

   epoll ET 模式通常与 非阻塞套接字 一起使用而，
这是因为 ET 模式要求应用程序能够处理可能的多余数据（即，在单次通知中到达但未能全部处理的数据）。
如果使用阻塞套接字，那么当应用程序读取数据时，它可能会阻塞在 read() 或 recv() 调用上，
直到没有更多数据可读为止。
但是，在 ET 模式下，应用程序不能依赖 read() 或 recv() 调用来阻塞并等待所有数据到达，因为 epoll 只会通知一次状态变化。
因此，为了充分利用 ET 模式的优势（即减少不必要的通知和上下文切换），通常需要将套接字设置为非阻塞模式，
以便应用程序可以主动控制读取/写入的数据量，并在必要时进行多次读取/写入操作，以确保处理完所有可用的数据。
这样，应用程序就可以更加高效地响应 I/O 事件，减少资源浪费，并提高整体性能。