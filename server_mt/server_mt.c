#include<sys/types.h>    
#include<sys/un.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<errno.h>
#include<ctype.h>
#include<pthread.h>

#include<arpa/inet.h>
#include<sys/socket.h>


struct s_info
{ //封装了一个结构体,将'客户端地址结构'和'与之通信的cfd'捆绑
  //便于给pthread_create()传参    
	struct sockaddr_in cliaddr;
    int cfd;
};

void sys_err(const char* str)
{
   perror(str);
   exit(1);
}

void* do_work(void* arg) //子线程执行函数
{ 
	
	int i,n;
    struct s_info* ts =(struct s_info*) arg;
    char buf[8192];
	char str[INET_ADDRSTRLEN]; //#define INET_ADDRSTRLEN 16
    
	while(1)
	{
		n=read(ts->cfd,buf,sizeof(buf));
		if(buf[0]=='.')
		{ //收到"."，说明客户端想主动断连
		   
		   printf("ip:%s port:%d 调用close()断连,n=%d\n",
		   inet_ntop(AF_INET,&(*ts).cliaddr.sin_addr,str,sizeof(str)),
		   ntohs((*ts).cliaddr.sin_port),n);

		   write(ts->cfd,"bye",4); //回复他 bye
	 	   break;	
	 	}
		else if(n>0)
		{ 
		printf(
			  "ip:%s port:%d 发过来的是 %s\n",
		      inet_ntop(AF_INET,&(*ts).cliaddr.sin_addr,str,sizeof(str)),
		      ntohs((*ts).cliaddr.sin_port),
			  buf
			  );
    	  
		  for(i=0;i<n;i++)
    	    buf[i] = toupper(buf[i]);
          write(ts->cfd,buf,n); //把处理结束的消息write回去
             
		}
		else if(n==0)
		{
			printf("n=%d, client 断开连接\n",n);
			break;
		}
		else if(n<0)
		{
			sys_err("read error");
		}
    
	}
    close(ts->cfd);
    return (void*) 0;
}

int main() //主线程执行函数
{    
   struct s_info ts[256]; //自定义的那个结构体的结构体数组
   pthread_t tid;

   int lfd; //用于监听的文件描述符
   lfd=socket(AF_INET,SOCK_STREAM,0);
   if(lfd==-1){ sys_err("socket error"); }

   //bind()绑定地址结构struct sockaddr_in serv_addr;
   struct sockaddr_in serv_addr;

   serv_addr.sin_family=AF_INET;
   serv_addr.sin_port = htons(9527);              //host-to-network-short()
   serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); //host-to-network-long()
   
   int bind_return=0;
   bind_return = bind( lfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr) );
   if(bind_return==-1){ sys_err("bind error"); }


   //listen()
   if(listen(lfd,2) == -1) {sys_err("listen()");}
    	
  	
   
  //---------------------------------------------------------------- 
    int i=0;

   struct sockaddr_in  clit_addr; //struct sockaddr_in 地址结构体
   socklen_t clit_addr_len; 
   clit_addr_len = sizeof(clit_addr);

   int cfd; //通信文件描述符,用于接收accept()成功的客户端

    while(1)
    { 
        cfd = accept(lfd,(struct sockaddr*)&clit_addr ,&clit_addr_len); 
        if(cfd==-1) sys_err("accept error");
    
	   ts[i].cliaddr =clit_addr;
	   ts[i].cfd =cfd;
       
	   pthread_create(&tid,NULL,do_work,(void*)&ts[i]); //创建子线程
	   pthread_detach(tid);                             //设置子线程分离,防止僵尸线程产生
       i++;
    }
   
   return 0;
}

