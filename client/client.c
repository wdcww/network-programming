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

//后面的改进，其实客户端就很少去改变了，主要是变服务端。
// 这是基于 server.c 改写的 client.c

void sys_err(const char* str)
	{
		perror(str);
		exit(1);
	}

int main()
{
   //--------------------------------------------------------------------------    	
   int cfd; //客户端只有通信的cfd
   int ret; //ret接受实际读到的字节数
   
   cfd=socket(AF_INET,SOCK_STREAM,0);//创建一个socket,并用文件描述符接受return
   if(cfd==-1)
   {sys_err("socket error");}


   //----client没有bind的必要---------------------------------------------------
   //也就不给struct sockaddr_in serv_addr;赋初值了
   
   //serv_addr.sin_family=AF_INET;
   //serv_addr.sin_port = htons(9527); //h-to-n-s() 本地to网络字节序,short，这个是端口
   //serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); //这个是ip,本地to网络,long,
   
   //int bind_return=0;
   //bind_return = bind( cfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr) );//bind
   //if(bind_return==-1)
   //{sys_err("bind error");}
   

   //-------client也不设置监听-------------------------------------------------
   //if(listen(cfd,2) == -1) {sys_err("listen()");}


   //--- client直接去connect()--------------------------------------------------------
   
   struct sockaddr_in serv_addr; //要连接的 服务器地址结构的 结构体变量serv_addr
 
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_port = htons(9527); //与定义的服务器保持一致
   //变量serv_addr.sin_addr.s_addr存放要连接的服务器ip，用inet_pton()写入该变量
                                        //inet_pton()，p-to-n, ip-to-网络字节序 
  inet_pton(AF_INET,"127.0.0.1",&(serv_addr.sin_addr.s_addr)); 

   ret = connect(cfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr) ); //connect()
   if(ret!=0) {sys_err("connect error");}

   //----client不用再建立一个socket用于通信
   //--------------------------------------------------------------------------
   //struct sockaddr_in  clit_addr; 
   //socklen_t clit_addr_len; 
   //clit_addr_len = sizeof(clit_addr);
 
   //int cfd1; 
    //cfd1 = accept(cfd,(struct sockaddr*)&clit_addr ,&clit_addr_len);
   //if(cfd1==-1){sys_err("accept error");}
    //
    //char client_IP[30];
    //printf("connected from:");
//printf(" client ip:%s port:%d\n",inet_ntop(AF_INET,&(clit_addr.sin_addr.s_addr),client_IP,sizeof(client_IP)),ntohs(clit_addr.sin_port) );
   //--------------------------------------------------------------------------
    
    char buf[1024];
    
    while(1) 
    {   
       //ret = read(cfd,buf,sizeof(buf)); 

       printf("在下面一行，输入你的发送信息(断开连接输入 . ):\n");
       fgets(buf, sizeof(buf), stdin);  
      
       write( cfd,buf,sizeof(buf));  //写，然后发给server	  
       //write( cfd,"abc\n",4);  //写abc，然后发给server	
       //write(STDOUT_FILENO,buf,ret); //写到屏幕上
        
       if(buf[0]=='.')
       {
         printf("你将调用close()断开与server的连接\n");
	 sleep(1);
	 ret = read(cfd,buf,sizeof(buf) ); //读server的回复       
         printf("server的回复：%s\n",buf);
         break;
       }

       ret = read(cfd,buf,sizeof(buf) ); //读server的回复       
       printf("server的回复：%s\n",buf);
       // write(STDOUT_FILENO,buf,ret); //写到屏幕上

    }

    printf("你已主动断开连接！"); 
    close(cfd);
       
 return 0;
}

