#include <sys/epoll.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <arpa/inet.h>
#include <ctype.h>

#define MAXLINE 8192
#define OPEN_MAX 5000

#define SERV_PORT 9527

void sys_err(const char *str) {
    perror(str);
    exit(1);
}


int main() 
{ 
    int i, j, maxi, lfd, cfd, sockfd;
    int n,num=0;
    ssize_t nready,efd,res;
    char buf[MAXLINE],str[INET_ADDRSTRLEN];
    socklen_t clilen;

    
    struct sockaddr_in cliaddr, servaddr;
    struct epoll_event tep; //用于epoll_ctl()的参数
    struct epoll_event ep[OPEN_MAX]; //用于epoll_wait()的参数
    
    //socket()创建lfd
    lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1) {
        sys_err("socket error");
    }
    
    //设置端口复用
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    //bind
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    if (bind(lfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1) {
        sys_err("bind error");
    }
    
    //listen
    if (listen(lfd, 128) == -1) {
        sys_err("listen error");
    }
    
    //创建epoll模型,efd指向红黑树根节点,OPEN_MAX是红黑树的size
    efd = epoll_create(OPEN_MAX);
     if(efd==-1)sys_err("epoll_create error");

    tep.events=EPOLLIN;  tep.data.fd = lfd; //指定lfd的监听事件为“读”

    //将lfd及对应的结构体设置 挂到以efd为根的红黑树
    res=epoll_ctl(efd,EPOLL_CTL_ADD,lfd,&tep); 
    if(res==-1)sys_err("1.epoll_ctl_error");


    while(1)
    {
        nready = epoll_wait(efd,ep,OPEN_MAX,-1);
     //epoll_wait()的参数介绍:ep是struct epoll_event类型的数组,OPEN_MAX是数组容量,-1是阻塞监听
      //返回nready是事件数 
        if(nready==-1) sys_err("epoll_wait error");

        for(i=0;i<nready;i++)
        {
            if( !(ep[i].events & EPOLLIN) ) //如果不是“读“事件,继续循环
            {
                continue;
            }

            if(ep[i].data.fd == lfd) //如果满足事件的fd是lfd
            {
             clilen = sizeof(cliaddr);

             cfd = accept(lfd,(struct sockaddr*)&cliaddr,&clilen); //接受连接

             printf("connected from %s:%d  ",
                    inet_ntop(AF_INET,&cliaddr.sin_addr,str,sizeof(str)),
                    ntohs(cliaddr.sin_port));

             printf("第%d个client: client[%d]\n",++num,cfd);   
             
             tep.events = EPOLLIN; 
             tep.data.fd =cfd;

             res = epoll_ctl(efd,EPOLL_CTL_ADD,cfd,&tep);//通过tep将cfd加入红黑树
             if(res==-1) sys_err("2.epoll_ctl error");
             
            }
            else //满足事件的fd不是lfd
            {
                sockfd = ep[i].data.fd;

                n=read(sockfd,buf,sizeof(buf));

                if(buf[0]=='.')
		         { //收到"."，说明客户端那边自己调用close()关闭自己。想主动断连
		            write(sockfd,"bye",4); //回复他 bye
	
                  //getpeername()用于获取已连接套接字对端的地址信息。
                  getpeername(sockfd, (struct sockaddr*)&cliaddr, &clilen);
                  inet_ntop(AF_INET, &cliaddr.sin_addr, str, sizeof(str));
                  printf("ip: %s port: %d 主动断开...\n", str, ntohs(cliaddr.sin_port));

                res = epoll_ctl(efd,EPOLL_CTL_DEL,sockfd,NULL);//将此描述符从红黑树摘除
                 if(res==-1) sys_err("3.epoll_ctl error");  
                  close(sockfd);

                  printf("client[%d] closed\n",sockfd);
               }
                else if (n < 0) 
                {

                  printf("n=%d,read error \n",n);
                res = epoll_ctl(efd,EPOLL_CTL_DEL,sockfd,NULL);//将此描述符从红黑树摘除
                 if(res==-1) sys_err("4.epoll_ctl error"); 
                    close(sockfd);
                printf("client[%d] 强制关闭\n",sockfd);
                } 
                else if (n == 0) 
                {
                printf("read()的返回 n=%d\n",n);
                printf("client[%d] 意外 disconnected,\n",sockfd);
                res = epoll_ctl(efd,EPOLL_CTL_DEL,sockfd,NULL);//将此描述符从红黑树摘除
                 if(res==-1) sys_err("5.epoll_ctl error"); 
                    close(sockfd);
                } 
                else if (n > 0) 
                {   
                    printf("read的 n=%d\n",n);
                    printf("received %d bytes from client[%d]: %s\n", n,sockfd,buf);
                    for (j = 0; j < n; j++) 
                    {
                        buf[j] = toupper(buf[j]);
                    }
                    write(sockfd, buf, n);
                     
                }

            }
        }
    }
    
 close(lfd);
 return 0;
}

