

	这个目录下是：
2024.5.13--------黑马程序员的【多进程】的服务端--------------


    lfd=socket()
    bind(lfd, 本地的网络地址结构体变量,sizeof(该变量))
    listen(lfd,int数字)
    
while(1)
{
       cfd=accept(lfd,接受客户端的网络地址结构体变量,sizeof(该变量))
       
       pid = fork()
       
       if(pid==0)
       { //子进程逻辑 1. 2. 3. 4.，四件事情。
    	  1. close(lfd)
    	  break;
       }
       else if(pid>0)
       { //父进程逻辑
         close(cfd)
                    回收结束的子进程(信号捕捉SIGCHLD)
         continue 父进程一直去监听
       }
       else if(pid<0)
       {fork error}               
              
}    

    
  //子进程逻辑
  2.通信
  
   char client_IP[30];
  
   while(1)
   {
     client_IP发送了 . 时，break,去 “3.”
   } 
         
  3.准备结束子进程
 close(cfd)
    	            
  4.子进程结束	
    return 0;

-----------------
