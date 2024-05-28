	这个目录下是：
2024.5.13--------黑马程序员的【多线程】的服务端--------------


void* tfn(void* arg) //子线程去用这个函数
{
     通信
}



int main() 
{
    lfd=socket()
    bind(lfd, 本地的网络地址结构体变量,sizeof(该变量))
    listen(lfd,int数字)
    
    
    while(1)
    {
       cfd=accept(lfd,接受客户端的网络地址结构体变量,sizeof(该变量))
       
       //一、
        产生子线程
       
       //二、
        回收子线程
              
    }    

    
} 

-----------------
