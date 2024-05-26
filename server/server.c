#include<sys/types.h>    
#include<sys/socket.h>
#include<sys/un.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<errno.h>
#include<pthread.h>
#include<ctype.h>
#include<arpa/inet.h>

#include"try.h"

void sys_err(const char* str)
{
   perror(str);
   exit(1);
}



int main()
{
   try();  //#include"try.h"	
   //-----socket()创建监听套接字lfd------------------------------------------    
    int lfd; //用于监听的文件描述符
   
   lfd=socket(AF_INET,SOCK_STREAM,0);//创建一个socket,并用文件描述符接受return
   if(lfd==-1)
   {sys_err("socket error");}

   //-----bind()绑定地址结构struct sockaddr_in serv_addr;---------------------
   struct sockaddr_in serv_addr; //struct sockaddr_in 是地址的结构体(man 7 ip)
   // struct sockaddr_in
   // {
   //   sa_family_t    sin_family; // address family: AF_INET 
   //   in_port_t      sin_port;   // port in network byte order
   //   struct in_addr sin_addr;   // internet address
   //  };
   // 
   // struct in_addr
   // {
   //  uint32_t   s_addr;     // address in network byte order 
   //  };
   serv_addr.sin_family=AF_INET;
   serv_addr.sin_port = htons(9527); //h-to-n-s() 本地to网络字节序,short
   serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   
   int bind_return=0;
   bind_return = bind( lfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr) );
   if(bind_return==-1)
   {sys_err("bind error");}


   //-----开始listen()--------------------------------------------------------
   if(listen(lfd,2) == -1) {sys_err("listen()");}
   
   //-------------------------------------------------------------------------- 	
  	struct sockaddr_in  clit_addr; //struct sockaddr_in 地址结构体
   	socklen_t clit_addr_len; 
   	clit_addr_len = sizeof(clit_addr);

   	int cfd; //通信文件描述符,用于接收accept()成功的客户端
      
	while(1)
     { //外层while()去循环着accept()
    	cfd = accept(lfd,(struct sockaddr*)&clit_addr ,&clit_addr_len);
   	if(cfd==-1){sys_err("accept error");}
    
    	char client_IP[30];
    	printf("connected from:");
	
	printf(" client ip:%s port:%d\n",
  inet_ntop(AF_INET,&(clit_addr.sin_addr.s_addr),client_IP,sizeof(client_IP)),
	       ntohs(clit_addr.sin_port) );
       
    
    	char buf[1024];
    	int ret; //ret接受实际读到的字节数
    
    	while(1) 
      { //内层的while()是和某个连上的进程通信

    		ret = read(cfd,buf,sizeof(buf)); // read客户端进程
         
         if(ret==0)
         {  
            // printf("read()的返回ret = %d\n",ret);
            printf("client 意外断开\n");
            break;
         }
		   printf("client发过来的:%s\n",buf);	
    		//write(STDOUT_FILENO,buf,ret); //写到屏幕上
        
		   if(buf[0]=='.')
		   {
       	  	//收到客户端发"."，说明该客户端主动断开连接
            printf("client自己调用close(cfd)关闭连接 \n");
		      write(cfd,"bye",4); //客户端主动断开,回复他 bye
	 	      break;	
		   }
                
		   // 处理消息内容---------------
		   int i;
    		for(i=0;i<ret;i++)
    		{
    	   	   buf[i] = toupper(buf[i]);
    		}
         //----------------------------

       	write(cfd,buf,ret); //write回去
        
	   } //退出内层while
          
	   close(cfd);//关闭当前的cfd	      
           //还在外层循环中，是为了看看还有没有客户端连接...
	   //再有客户端连接，cfd就是新的了
	
     }

      
    close(lfd);

 return 0;
}


