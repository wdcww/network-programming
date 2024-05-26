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

#include<signal.h>

#include <sys/types.h>
#include <sys/wait.h>

// Ctrl+N 可以自动补全

void sys_err(const char* str)
{
   perror(str);
   exit(1);
}

void catch_child(int signum)
{
while( waitpid(0,NULL,WNOHANG) > 0 );//waitpid()一次回收一个,放到while里
}

int main()
{    
    int lfd; //用于监听的文件描述符
   lfd=socket(AF_INET,SOCK_STREAM,0);
   if(lfd==-1){ sys_err("socket error"); }

   int opt=1;
   int ret_set=setsockopt( lfd,SOL_SOCKET,SO_REUSEADDR,(void*)&opt,sizeof(opt) );
   if(ret_set==-1){sys_err("setsockopt ");}

   //bind()绑定地址结构struct sockaddr_in serv_addr;
   struct sockaddr_in serv_addr;

   serv_addr.sin_family=AF_INET;
   serv_addr.sin_port = htons(9527);              //host-to-network-short()
   serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); //host-to-network-long()
   
   int bind_return=0;
   bind_return = bind( lfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr) );
   if(bind_return==-1){ sys_err("bind error"); }

   //printf("This is (ip:%s port:%d)\n");

   //listen()
   if(listen(lfd,2) == -1) {sys_err("listen()");}
    	
  	
   struct sockaddr_in  clit_addr; //struct sockaddr_in 地址结构体
   socklen_t clit_addr_len; 
   clit_addr_len = sizeof(clit_addr);

   int cfd; //通信文件描述符,用于接收accept()成功的客户端
      

         while(1)
        { 
          cfd = accept(lfd,(struct sockaddr*)&clit_addr ,&clit_addr_len); 
        	//if(cfd==-1) sys_err("accept error");
         if(cfd==-1)
	  { //父进程在一直调accept时，可能会被信号中断(比如因为子进程退出而触发的SIGCHLD信号时)，
	    //accept()会返回-1并设置errno为EINTR  
	  if (errno == EINTR)
	     {  
     		 // EINTR: Call was interrupted by a signal; try again  
                 continue;  
             }  
         	 sys_err("accept error");  
          }

          pid_t pid; 

	  pid = fork(); //创建子进程
        
		if(pid==0) //子进程逻辑 一、二、三、3件事情
		{     
	           break;
         	}   
	        else if(pid > 0) //父进程逻辑
                {
	           close(cfd);//1.父进程关闭cfd	
	           
		   struct sigaction act;
		   act.sa_handler = catch_child;
		   sigemptyset(&act.sa_mask);
		   act.sa_flags=0;
		   int sigaction_ret;
		  sigaction_ret = sigaction(SIGCHLD,&act,NULL); //2.注册信号捕捉函数	
	           if(sigaction_ret != 0) sys_err("sigaction error");
		 
		  continue; //3.父进程需要一直监听	
	        }
	        else if(pid < 0)
	        { 
	          sys_err("fork error");	   
	        }

         } 


//子进程会执行这里的--------------------------------------------------------------------
 //一、子进程只需要cfd 	
       close(lfd); //子进程不使用lfd
 //二、子进程的通信	       
        char client_IP[30];
        printf("-------- connected from:");
	printf(" client ip:%s port:%d\n",inet_ntop( AF_INET,&(clit_addr.sin_addr.s_addr),client_IP,sizeof(client_IP) ),ntohs(clit_addr.sin_port) );
      
    	char buf[1024];
    	int ret; //ret接受实际读到的字节数
        
	while(1)
        {
    	// read客户端的东西
    	ret = read(cfd,buf,sizeof(buf)); // read()
                
	printf(" client ip:%s port:%d",
  	inet_ntop( AF_INET,&(clit_addr.sin_addr.s_addr),client_IP,sizeof(client_IP) ),
	ntohs(clit_addr.sin_port) );
	printf(" 发过来的是 ：%s\n",buf);	
    		//write(STDOUT_FILENO,buf,ret); //写到屏幕上
        
		if(buf[0]=='.')
		{ //收到"."，说明客户端想主动断连
		   write(cfd,"bye",4); //回复他 bye
	 	   break;	
	 	}
                
	     // 没有收到"."，就处理消息内容
	  int i;
    	  for(i=0;i<ret;i++)
    	 {
    	    buf[i] = toupper(buf[i]);
    	 }
        write(cfd,buf,ret); //把处理结束的消息write回去
        }

 //三、执行到这里的时候,是因为收到了客户端的“.”而break 子进程的while()
	 close(cfd);//子进程已经关闭过lfd了，所以这里关闭这次的cfd	      

printf(" client ip:%s port:%d",
inet_ntop( AF_INET,&(clit_addr.sin_addr.s_addr),client_IP,sizeof(client_IP) ),
ntohs(clit_addr.sin_port) );
printf("断开连接\n");	
//-----------------------------------------------------------------------------------------      
 return 0;
}

