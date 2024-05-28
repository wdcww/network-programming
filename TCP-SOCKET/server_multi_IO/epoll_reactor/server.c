/*
“收到啥，回复啥”
epoll基于非阻塞I/O事件驱动
*/
#include<stdio.h>
#include<sys/socket.h>
#include<sys/epoll.h>
#include<arpa/inet.h>
#include<fcntl.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<stdlib.h>
#include<time.h>

#define MAX_EVENTS 1024 //监听上限数
#define BUFLEN 4096
#define SERV_PORT 9527

void recvdata(int fd,int events,void* arg);
void senddata(int fd,int events,void* arg);
void sys_err(const char *str);

/*就绪文件描述符的相关信息*/
struct myevent_s {
    int fd; //要监听的文件描述符
    int events; //对应的监听事件
    void* arg; //泛型参数
    void (*call_back)(int fd,int events,void* arg); //回调函数
    int status; //是否在监听(1：在红黑树上监听，0：不在监听)
    char buf[BUFLEN];
    int len;
    long last_active; //记录每次加入global_efd指向的红黑树的时间值
};


int global_efd; //保存epoll_create()返回的文件描述符
struct myevent_s global_events[MAX_EVENTS+1]; //一个上述结构体的结构体数组


//eventset()可以给【单个的结构体变量】初始化
void eventset(struct myevent_s* ev,int fd,void (*call_back)(int,int,void*),void* arg)
{
    ev->fd=fd;
    ev->call_back=call_back;
    ev->events=0;
    ev->arg=arg;
    ev->status=0;
    memset(ev->buf,0,sizeof(ev->buf));
    ev->len=0;
    ev->last_active=time(NULL);

    return;
}

//eventadd() 向epoll监听的红黑树 添加一个 文件描述符
void eventadd(int efd,int events,struct myevent_s* ev)
{
    struct epoll_event epv ={0,{0}}; //epoll_event在epoll.h中定义
    int op;
    epv.data.ptr=ev;
    epv.events = ev->events = events;
    if(ev->status == 0)
    {
        op=EPOLL_CTL_ADD;
        ev->status = 1;
    }
    
    if(epoll_ctl(efd,op,ev->fd,&epv)<0)
      printf("event add failed [fd=%d],events[%d]\n",ev->fd,events);
    else
      printf("event add OK [fd=%d],op=%d,events[%0X]\n",ev->fd,op,events);

return;
}

//eventdel() 从epoll监听的红黑树 摘除一个 文件描述符
void eventdel(int efd,struct myevent_s* ev)
{
  struct epoll_event epv={0,{0}};;
  if(ev->status!=1)
    return;
  
  epv.data.ptr = NULL;
  ev->status =0;
  epoll_ctl(efd,EPOLL_CTL_DEL,ev->fd,&epv);

 return;
}


void senddata(int fd,int events,void* arg) //cfd的回调（去监听写事件时）
{
  struct myevent_s* ev = (struct myevent_s*) arg;
  int len;

   write(fd,ev->buf,ev->len); //直接将数据写给客户端，”复读机“
  
   eventdel(global_efd,ev); //该节点从红黑树摘除

   if(len>0)
  {
    printf("给fd=%d send %d bytes:%s\n",fd,ev->len,ev->buf);
    printf("cfd[%d]回调函数设置为recvdata()\n",fd);
    eventset(ev,fd,recvdata,ev);     //将该fd对应的回调函数设置为recvdata()
    eventadd(global_efd,EPOLLIN,ev); //将该fd加入红黑树,去监听‘读’事件
  }
  else 
  {
     close(ev->fd);
     printf("send[给fd=%d] error ,closed\n",fd);
     sys_err("send error");
  }

  return;
}


void recvdata(int fd,int events,void* arg) //cfd的回调（去监听读事件时）
{
  struct myevent_s* ev = (struct myevent_s*) arg;
  int len;
  
  eventdel(global_efd,ev); //该节点从红黑树摘除

  len = recv(fd,ev->buf,sizeof(ev->buf),0);
  
  if(len>0)
  {
    ev->len =len;
    ev->buf[len]='\0';
    printf("Client[%d]:%s\n",fd,ev->buf);

    eventset(ev,fd,senddata,ev);      //将该fd对应的回调函数设置为senddata()
    printf("此时,cfd[%d]回调函数设置为senddata()\n",fd);
    eventadd(global_efd,EPOLLOUT,ev); //将该fd加入红黑树,去监听‘写‘事件
  }
  else if(len == 0)
  {
    close(ev->fd);
    printf("Client[%d] closed\n",fd);
  }
  else
  {
     close(ev->fd);
     printf("Client[%d] recv error ,closed\n",fd);
  }

return;
}


void acceptconn(int lfd,int events,void* arg)  //lfd的回调
{
  struct sockaddr_in cin;
  socklen_t len = sizeof(cin);
  int cfd,i;
  cfd=accept(lfd,(struct sockaddr*)&cin,&len);
   
   for(i=0;i<MAX_EVENTS;i++)
     {
      if(global_events[i].status ==0)
         break; //找到第一个不是在监听的位置i
     }

    //cfd设置为非阻塞
    fcntl(cfd,F_SETFL,O_NONBLOCK);


    /*cfd设置一个myevent_s结构体,回调函数设置为recvdata()*/
    eventset(&global_events[i],cfd,recvdata,&global_events[i]);

    //cfd添加到红黑树去监听读事件
    eventadd(global_efd,EPOLLIN,&global_events[i]);

    printf("new connect [%s:%d][time:%ld],pos[%d]\n",
           inet_ntoa(cin.sin_addr),ntohs(cin.sin_port),
           global_events[i].last_active,
           i);

      printf("连接成功的cfd[%d],回调函数为recvdata()\n",cfd);
 return;
}


void initlistensocket(int efd,short port)
{
    struct sockaddr_in sin;
    int lfd =socket(AF_INET,SOCK_STREAM,0);
    if (lfd == -1) {
        sys_err("socket error");
    }
    
    //设置端口复用
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    //设置lfd为非阻塞
    fcntl(lfd,F_SETFL,O_NONBLOCK); 

    memset(&sin,0,sizeof(sin));
    sin.sin_family =AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port =htons(port);

    bind(lfd,(struct sockaddr*)&sin,sizeof(sin));

    listen(lfd,20);
    
    // void eventset(struct myevent_s* ev,int fd,void (*call_back)(int,int,void*),void* arg)
    eventset(&global_events[MAX_EVENTS],lfd,acceptconn,&global_events[MAX_EVENTS]);
    //lfd 放到了最后一个元素global_events[MAX_EVENTS]上面
    printf("lfd[%d]的回调函数设置为acceptconn(),去监听EPOLLIN事件...\n",lfd);

    //void eventadd(int efd,int events,struct myevent_s* ev)
    eventadd(efd,EPOLLIN,&global_events[MAX_EVENTS]);

 return;
}


void sys_err(const char *str) 
{
    perror(str);
    exit(1);
}

int main(int argc,char* argv[])
{
   unsigned short port = SERV_PORT;

   if(argc == 2) port = atoi(argv[1]);
    
   global_efd = epoll_create(MAX_EVENTS+1);  //创建红黑树，返回给全局变量global_efd
   if(global_efd<0) sys_err("epoll_create error");

   initlistensocket(global_efd,port); // 初始化 lfd

   struct epoll_event events[MAX_EVENTS+1];  //struct epoll_event定义在epoll.h

   printf("“收到啥，回复啥”\n");
   printf("server running:port[%d]\n",port);
   printf("------------------\n");

   int checkpos = 0,i;
   while(1)
  {
      //超时验证，每次测试100个连接(不测试lfd),当客户端60s内没有和服务端通信,关闭该客户端
      long now =time(NULL);//当前时间
      for(i=0;i<100;i++,checkpos++) //checkpos控制检测对象
       {
         if(checkpos == MAX_EVENTS)
          checkpos =0;
         if(global_events[checkpos].status != 1) //不在红黑树上监听,
           continue;
         long duration = now - global_events[checkpos].last_active; //计算客户不活跃的时间
         if(duration >= 60)
         {   
             close(global_events[checkpos].fd);   //关闭连接
             printf("[fd=%d] timeout\n",global_events[checkpos].fd);
             eventdel(global_efd,&global_events[checkpos]); //从红黑树摘除             
         }
       }
     
     //监听,将满足事件的文件描述符移至events数组，1s没有事件满足，epoll_wait()返回0
     int nfd =epoll_wait(global_efd,events,MAX_EVENTS+1,1000);
     //"events" parameter is a buffer that will contain triggered events
      if(nfd < 0) 
      {
        sys_err("epoll_wait error,exit");
      }     

      for(i=0;i<nfd;i++)
      {
        struct myevent_s* ev = (struct myevent_s*)events[i].data.ptr;

        if((events[i].events & EPOLLIN)) // && (ev->events & EPOLLIN)) //读就绪
          { 
            printf("IN\n");
            ev->call_back(ev->fd,events[i].events,ev->arg);
          }
        if((events[i].events & EPOLLOUT) ) // && (ev->events & EPOLLOUT)) //写就绪
          {
            printf("OUT\n");
            ev->call_back(ev->fd,events[i].events,ev->arg);
          }
      }   

  }

//退出前释放所有资源
return 0;
}




