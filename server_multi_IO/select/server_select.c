#include<sys/select.h>

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<arpa/inet.h>
#include<ctype.h>


void sys_err(const char* str)
{
   perror(str);
   exit(1);
}



int main(int argc,char* argv[])
{
   int i;
   int maxfd=0;
   int lfd,cfd;
   char buf[BUFSIZ];
   char client_IP[30];
   int j,n,sockfd;
   int nready;

   int client[1024]; //自定义数组client,防止遍历1024个文件描述符
   int maxi=-1;
   
   for(i=0;i<1024;i++)
    client[i]=-1; //初始化数组client


   struct sockaddr_in  clie_addr, serv_addr;
   socklen_t clie_addr_len;
   
   //socket
   lfd=socket(AF_INET, SOCK_STREAM,0);
     // printf("lfd: %d\n",lfd);

    if(lfd==-1){ sys_err("socket error"); }
   
   //使用setsockopt(),让绑定的端口可以复用
   int opt=1;
   setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    
   //bind
   bzero(&serv_addr,sizeof(serv_addr)); //初始化清零服务端地址结构
   serv_addr.sin_family=AF_INET;
   serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);
   serv_addr.sin_port=htons(9527);
   
   int bind_return=0;
   bind_return = bind( lfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr) );
   if(bind_return==-1){ sys_err("bind error"); }

   //listen 设置“同时”在线的上限
   if(listen(lfd,128) == -1) {sys_err("listen()");}


   fd_set readset,allset;

   maxfd =lfd; //一开始,最大的描述符就是仅有的lfd

   FD_ZERO(&allset); //初始化清零allset
   FD_SET(lfd,&allset);

   while(1)
   {
      readset= allset; //每次进来,都把allset赋值给readset，allset放我想去监听的,readset一会传出来就是确实有事件的
      nready = select(maxfd+1,&readset,NULL,NULL,NULL); 

      //printf("1----\n select()的返回是:%d\n",nready); 
      
      if(nready<0) sys_err("select error");

      if( FD_ISSET(lfd,&readset) ) //如果传出的readset里面还有lfd，说明有客户端连接，再accept()
      { 

         clie_addr_len=sizeof(clie_addr);
         cfd=accept(lfd,(struct sockaddr*)&clie_addr,&clie_addr_len); //有客户端,再去accept,不会阻塞
         if(cfd==-1) sys_err("accept error");
         
	     printf("-------- connected from: client ip:%s port:%d\n",inet_ntop( AF_INET,&(clie_addr.sin_addr.s_addr),client_IP,sizeof(client_IP) ),
                                         ntohs(clie_addr.sin_port) );
         
         for(i=0;i<1024;i++)
           if(client[i]<0)  // client[]是辅助，用来存放需要监听的描述符 
           {
              client[i] = cfd;
            //printf("新增 %d\n",client[i]);
              break;
           }  

         if(i==1024){sys_err("too many clients");}


         FD_SET(cfd,&allset);//并且把刚刚有的cfd放入allset,为下次去select准备
         
         if(maxfd<cfd) 
         maxfd = cfd;

         if(i>maxi) maxi=i;
         
         if(--nready == 0) 
         {
            //printf("2----\n select()的返回是:%d\n",nready); 
            continue;//nready=1时(即只有lfd有变化)时,没有客户有通信需求,就不用执行下面的for(/*与*/之间)
         }
         
      }
      
      // /*
        for( i=0; i<=maxi; i++ ) // 这个for遍历辅助数组client[]中存放的监听对象
        {   
            sockfd=client[i];

            if(sockfd<0)
              continue;

            if( FD_ISSET(sockfd,&readset) ) //如果某一次的select返回的集合中有你关心的那个描述符,再进入此if
            {
               n=read(sockfd,buf,sizeof(buf));

               if(buf[0]=='.')
		         { //收到"."，说明客户端那边自己调用close()关闭自己。想主动断连
		            write(sockfd,"bye",4); //回复他 bye
	
                  //getpeername()用于获取已连接套接字对端的地址信息。
                  getpeername(sockfd, (struct sockaddr*)&clie_addr, &clie_addr_len);
                  inet_ntop(AF_INET, &clie_addr.sin_addr, client_IP, sizeof(client_IP));
                  printf("ip: %s port: %d 主动断开\n", client_IP, ntohs(clie_addr.sin_port));
                    
                  close(sockfd);
                  FD_CLR(sockfd,&allset); //将代表已关闭的对端文件描述符从allset去掉
                  client[i]=-1; //存放要遍历的文件描述符的数组对应位，置回-1
               }
               else if(n==0)
               {
                  getpeername(sockfd, (struct sockaddr*)&clie_addr, &clie_addr_len);
                  inet_ntop(AF_INET, &clie_addr.sin_addr, client_IP, sizeof(client_IP));
                  printf("ip: %s port: %d 意外 断开\n", client_IP, ntohs(clie_addr.sin_port));
                  
                  close(sockfd);
                  FD_CLR(sockfd,&allset); //将代表已关闭的对端文件描述符从allset去掉
                  client[i]=-1; //存放要遍历的文件描述符的数组对应位，置回-1

               }
               else if(n>0)
                 {  
                    //getpeername()用于获取已连接套接字对端的地址信息。
                   getpeername(sockfd, (struct sockaddr*)&clie_addr, &clie_addr_len);
                    inet_ntop(AF_INET, &clie_addr.sin_addr, client_IP, sizeof(client_IP));
                    printf("ip: %s port: %d 发来的是：%s\n", client_IP, ntohs(clie_addr.sin_port),buf);
                      
                    for(j=0;j<n;j++)
                      buf[j]=toupper(buf[j]);   

                    write(sockfd,buf,n); 
                  }
                else if(n<0)
                 {
                  perror("read error");
                 }

               if(--nready == 0) break; // 如果没有更多的就绪描述符，退出/*与*/之间的for循环 

            }
        }
      // */

   }

  close(lfd);
  return 0;
}
