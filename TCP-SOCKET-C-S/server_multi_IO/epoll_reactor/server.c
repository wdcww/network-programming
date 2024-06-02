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


void sys_err(const char *str) //封装perror()
{
    perror(str);
    exit(1);
}

//eventset()可以给【单个的结构体变量】初始化
void eventset(struct myevent_s* ev,int fd,void (*call_back)(int,int,void*),void* arg) //很多赋值操作
{
    ev->fd=fd;
    ev->call_back=call_back;
    ev->events=0;
    ev->arg=arg;
    ev->status=0;

    ev->last_active=time(NULL);
    return;
}


//--------------封装epoll_ctl----------------

void eventadd(int efd,int events,struct myevent_s* ev) //试图封装epoll_ctl(*,EPOLL_CTL_ADD,*,*)
{
    int op=-1;
    
    struct epoll_event epv ={0,{0}}; //epoll_event在epoll.h中定义
    ev->events = events;
    epv.data.ptr=ev; //用上了扩展接口
    
    epv.events = events; 

    if(ev->status == 0) 
    {
        op = EPOLL_CTL_ADD;
        ev->status = 1;  // 标识此描述符为监听态
    } 
    else 
    {
        op = EPOLL_CTL_MOD;
    }
    
    if(epoll_ctl(efd,op,ev->fd,&epv)<0)
      printf("event add failed [fd=%d],events[%d]\n",ev->fd,events);
    else
      // printf("event add OK [fd=%d],op=%d,events[%0X]\n",ev->fd,op,events);
       // 向epoll监听的efd树 add一个 文件描述符

return;
}


void eventdel(int efd,struct myevent_s* ev) //试图封装epoll_ctl(*,EPOLL_CTL_DEL,*,*)
{
  if(ev->status !=1)
    { 
      printf("该事件早已不在监听 [fd=%d]\n",ev->fd);
      return;
    }

  ev->status =0; //要删掉了，置为非监听态

  struct epoll_event epv;
  epv.data.ptr = NULL;
  
  if(epoll_ctl(efd,EPOLL_CTL_DEL,ev->fd,&epv)<0) 
     {
      printf("event delete failed [fd=%d] \n",ev->fd);
      perror("EPOLL_CTL_DEL: ");
     }
  else
    //  printf("event delete OK [fd=%d] \n",ev->fd);
     // 从epoll监听的红黑树(efd) del一个 文件描述符

 return;
}


//---------------回调-----------------------

void senddata(int fd, int events, void* arg) // cfd的回调（去监听写事件时）
{
    struct myevent_s* ev = (struct myevent_s*) arg;
    int len;
    
    eventdel(global_efd, ev); // 该节点从红黑树摘除

    len = write(fd, ev->buf, ev->len); // 直接将数据写给客户端，”复读机“
    
        if(len > 0) 
        {
        ev->len = len;
        ev->buf[len] = '\0';
        printf("回复Client[%d]:%s\n",fd,ev->buf);
        eventset(ev, fd, recvdata, ev); // 将该fd对应的回调函数设置为recvdata()
        eventadd(global_efd, EPOLLIN | EPOLLET, ev); // 将该fd加入红黑树,去监听‘读’事件
        } 
        else 
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK) 
           {
             eventset(ev, fd, senddata, ev); // 重新设置为发送数据
             eventadd(global_efd, EPOLLOUT | EPOLLET, ev); // 继续监听写事件
           } 
           else 
           {
             close(ev->fd);
             printf("send[fd=%d] error\n", fd);
           }
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
    eventadd(global_efd,EPOLLOUT|EPOLLET,ev); //将该fd加入红黑树,去监听‘写‘事件
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

    do{ 
        for(i=0;i<MAX_EVENTS;i++) //这里是MAX_EVENTS,i=0~1023,因为global_events[1024]是lfd
        {
          if(global_events[i].status ==0)
           break; //找到第一个不是在监听的位置i,跳出当前for
        }

        if(i==MAX_EVENTS)
        {
          printf("max connect limit\n");
          break;//跳出do-while(0)
        }

        //cfd设置为非阻塞
        fcntl(cfd,F_SETFL,O_NONBLOCK);

        /*cfd设置一个myevent_s结构体,回调函数设置为recvdata()*/
        eventset(&global_events[i],cfd,recvdata,&global_events[i]);
        
        //cfd添加到红黑树去监听读事件
        eventadd(global_efd,EPOLLIN|EPOLLET,&global_events[i]);
     }while(0);

    printf(
          "连接成功的cfd[%d],[%s:%d][time:%ld],pos[%d]\n",
           cfd,
           inet_ntoa(cin.sin_addr),
           ntohs(cin.sin_port),
           global_events[i].last_active,
           i
          );
 
 return;
}

//----------------lfd-----------------------

void initlistensocket(int efd,short port) //把lfd放到 global_events[]的最后一个元素global_events[1024]上面
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

    listen(lfd,20); //只是设置同时可监听数
    
    // void eventset(struct myevent_s* ev,int fd,void (*call_back)(int,int,void*),void* arg)
    eventset(&global_events[MAX_EVENTS],lfd,acceptconn,&global_events[MAX_EVENTS]);
    //lfd 放到了最后一个元素global_events[MAX_EVENTS]上面
    printf("lfd[%d]去监听EPOLLIN事件...\n\n",lfd);

    //void eventadd(int efd,int events,struct myevent_s* ev)
    eventadd(efd, EPOLLIN ,&global_events[MAX_EVENTS]);

 return;
}


//---------------main------------------------

int main(int argc,char* argv[])
{
   unsigned short port = SERV_PORT;
   
   if(argc == 2) 
   {
    port = atoi(argv[1]);
    printf("use your server port: %d\n",port);
   }
   
   printf("   '收啥回啥'  服务器 ”\n");
   printf("              server running:port[%d]\n",port);
   printf("wait:-----------------------------------------\n");
   int i;
   
   for(i=0;i<MAX_EVENTS+1;i++)
      eventset(&global_events[i],0,NULL,NULL);

   global_efd = epoll_create(MAX_EVENTS+1);  //创建红黑树，返回给全局变量global_efd
   if(global_efd<0) sys_err("epoll_create error");

   initlistensocket(global_efd,port); // 初始化 lfd

   struct epoll_event events[MAX_EVENTS+1]; //用来接受epoll_wait()返回的buffer that will contain triggered events
   int checkpos = 0;

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
             printf("[fd=%d] timeout\n",global_events[checkpos].fd);
             write(global_events[checkpos].fd,"timeout,please connect again!",30);
             eventdel(global_efd,&global_events[checkpos]); //先从红黑树摘除
             close(global_events[checkpos].fd);   //再关闭连接             
         }
       }
     
     //epoll_wait()
     // 将满足事件的文件描述符移至events数组，
     // 若1000 ms 没有事件满足，epoll_wait()返回0
     int nfd = epoll_wait(global_efd,events,MAX_EVENTS+1,1000);
     //"events" parameter is a buffer that will contain triggered events
     //假如某次返回nfd=2，那么ep[0]和ep[1]就是那两个事件各自对应的struct epoll_event结构

      if(nfd < 0) 
      {
        sys_err("epoll_wait error,exit");
      }     

      for(i=0;i<nfd;i++) 
      {
        struct myevent_s* ev = (struct myevent_s*)events[i].data.ptr;

        if((ev->events & EPOLLIN)) //读就绪
          { //即使是lfd的读就绪，也能够正确的调用lfd的回调函数
            ev->call_back(ev->fd,events[i].events,ev->arg);
          }
        if((ev->events & EPOLLOUT)) //写就绪
          {
            ev->call_back(ev->fd,events[i].events,ev->arg);
          }
      }   

  }

//退出前释放所有资源
return 0;
}

