#include<stdio.h>
#include<string.h>
#include <strings.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<sys/epoll.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<dirent.h>

#define MAXSIZE 2048

int init_listen_fd(int port,int epfd)
{
    //创建监听套接字lfd
    int lfd = socket(AF_INET,SOCK_STREAM,0);
    if(lfd==-1){perror("socket error");exit(1);}
    
    
//------------------------------------------------------------
char* ip_address = "127.0.0.1"; //我想绑定的服务器IP  
  
// 使用 inet_pton 函数将点分十进制 IP 地址转换为网络字节序  
struct in_addr ip;  
if (inet_pton(AF_INET, ip_address, &ip) <= 0) {  
    perror("inet_pton error");  
    exit(1);  
}  
  
struct sockaddr_in srv_addr;//创建服务器地址结构
bzero(&srv_addr,sizeof(srv_addr));
srv_addr.sin_family=AF_INET;
srv_addr.sin_port=htons(port);

// srv_addr.sin_addr.s_addr=htonl(INADDR_ANY);  //这个是我之前的绑定IP方法,现在用这行                             
srv_addr.sin_addr.s_addr = ip.s_addr;//将转换后的 IP 地址赋值给 srv_addr.sin_addr.s_addr 

//------------------------------------------------------------

    //设置要绑定的端口可以复用
    int opt=1;
    setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    //设置lfd为非阻塞
    fcntl(lfd,F_SETFL,O_NONBLOCK);

    //给lfd绑定地址结构
    int ret =bind(lfd,(struct sockaddr*)&srv_addr,sizeof(srv_addr));
    if(ret==-1){perror("bind error");exit(1);}

    //设置同时监听上限
    ret=listen(lfd,120);
    if(ret==-1){perror("listen error");exit(1);} 

    //lfd添加至epoll树上
    struct epoll_event ev;
    ev.events = EPOLLIN|EPOLLET;
    ev.data.fd=lfd;
    ret =epoll_ctl(epfd,EPOLL_CTL_ADD,lfd,&ev);
    if(ret==-1){perror("epoll_ctl add lfd error");exit(1);}

    
    return lfd;
}

void do_accept(int lfd,int epfd)//连接新的browser
{
    struct sockaddr_in clt_addr;
    socklen_t clt_addr_len=sizeof(clt_addr);
    int cfd=accept(lfd,(struct sockaddr*)&clt_addr,&clt_addr_len);
    if(cfd==-1){perror("accept error");exit(1);}

    //打印客户端
    char client_IP[30];
    printf("---------new client[%d], ip:%s[port%d]\n",
           cfd,
           inet_ntop(AF_INET,&clt_addr.sin_addr.s_addr,client_IP,sizeof(client_IP)),
           ntohs(clt_addr.sin_port)
           );

    //cfd设置非阻塞
    fcntl(cfd,F_SETFL,O_NONBLOCK);//非阻塞模式
    
    //将新的cfd也挂到epoll监听树上
    struct epoll_event ev;
    ev.data.fd=cfd;
    ev.events=EPOLLIN|EPOLLET;//ET(边沿模式)

    int ret=epoll_ctl(epfd,EPOLL_CTL_ADD,cfd,&ev);
    if(ret==-1){perror("epoll_ctl add cfd error");exit(1);}

} 

void disconnect(int cfd,int epfd)
{
    int ret =epoll_ctl(epfd,EPOLL_CTL_DEL,cfd,NULL);//先从树上摘除
    if(ret!=0){perror("disconnect epoll_ctl del error");exit(1);}
    
    close(cfd);//再关闭连接

}

const char* get_file_type(const char* name)
{
    char* dot;
    dot = strrchr(name,'.');//自右向左从name中找'.'，不存在返回NULL
    //strrchr()returns a pointer to the last occurrence of the character in the string.
    if(dot==NULL) return "text/plain; charset=utf-8";
    if( strcmp(dot,".html")==0||strcmp(dot,".htm")==0 ) return "text/html; charset=utf-8";
    if( strcmp(dot,".jpg")==0||strcmp(dot,".jpeg")==0 ) return "image/jpeg";
    if( strcmp(dot,".png")==0 ) return "image/png";
    if (strcmp(dot, ".webm") == 0) return "video/webm";  
    if( strcmp(dot,".pdf")==0 ) return "application/pdf";


    return "text/plain; charset=iso-8859-1";
}

void send_error_page(int cfd,char* message)//发自己写的not found
{  
    char buf[4096]={0};
    sprintf(buf+strlen(buf),"HTTP/1.1 404 Not Found\r\n");
    sprintf(buf+strlen(buf),"Content-Type: text/html\r\n");
    sprintf(buf+strlen(buf),"Content-Length: -1\r\n");
    sprintf(buf+strlen(buf),"\r\n");
    sprintf(buf+strlen(buf),"<html><head><title>404</title></head>");
    sprintf(buf+strlen(buf),"<body><h1>");
    strcat(buf, message);
    sprintf(buf+strlen(buf),"</h1><table></table></body></html>");
    // printf("%s\n",buf);
    send(cfd,buf,strlen(buf),0);

    printf("处理client[%d]请求\n",cfd);
}  

void send_response_first_part(int cfd, int num, const char* num_discribe, const char* type, int len) //回发http协议应答第一部分：首行、消息报头、空行
{  
   char buf[2048]; // 根据需要增加缓冲区大小  
   int offset = 0;  
  
   // 使用snprintf和偏移量来拼接头部信息  
   offset += snprintf(buf + offset, sizeof(buf) - offset, "HTTP/1.1 %d %s\r\n", num, num_discribe);  
   offset += snprintf(buf + offset, sizeof(buf) - offset, "Content-Type: %s\r\n", type);  
   offset += snprintf(buf + offset, sizeof(buf) - offset, "Content-Length: %d\r\n", len);  
   offset += snprintf(buf + offset, sizeof(buf) - offset, "\r\n"); // 空行表示头部结束  
   printf("will send to client[%d]:\n%s\n", cfd, buf); //将发送的整个头部信息
   int n = send(cfd, buf, offset, 0);  
   //if(n!= -1) printf("success send_response_first_part\n");   
   if(n==-1) 
   {  
      perror("send_response_first_part error");  
   }  

}

void send_file(int cfd, const char* file)//请求file,回发file内容(http协议响应正文)
{  
    int filed = open(file, O_RDONLY);  
    if(filed == -1)//访问文件存在,但是打不开
    {  
        perror("send_file file not open!");  
        send_error_page(cfd,"the file is exist but is not opened");//发自己写的not found
        return;  
    }  
    
    //访问文件存在,去read出来----------------
    char buf[1024];  
    ssize_t n; 
    while ((n = read(filed, buf, sizeof(buf))) > 0) 
    {  
        // printf("to client[%d]:%.*s\n", cfd, (int)n, buf); // 限制printf的输出长度  
        send(cfd, buf, n, 0);  
        // printf("send_file\n");  
    }  
  
    if (n == -1) 
    {  
        perror("send_file read error");  
    }
    else
    {
        printf("给予client[%d]请求\n",cfd);
    }  
    //-------------------------------------

    close(filed);  
}

void send_dir(int cfd,const char* dirname)//请求是目录时
{
  int i,ret;

  char buf[4096]={0};
  sprintf(buf,"<html><head><title>目录名：%s</title></head>",dirname);//浏览器标签标题
  sprintf(buf+strlen(buf),"<body><h1>当前目录：%s</h1><table>",dirname);//page标题

  char path[1024]={0};

  struct dirent** ptr;
  int num =scandir(dirname,&ptr,NULL,alphasort);
  //scandir读取dirname目录下的所有文件和子目录，并将它们存储在 ptr指针数组中
  
  for(i=0;i<num;i++)
  {
    char* name=ptr[i]->d_name;

    sprintf(path,"%s/%s",dirname,name);
    // printf("path = %s --------------\n",path);
    
    struct stat st;
    stat(path,&st);
    
    // encode_str(enstr,sizeof(enstr),name);

    if( S_ISREG(st.st_mode) )
    {
        sprintf(buf+strlen(buf),
        "<tr><td><a href=\"%s\">%s</a></td></tr>",
        name,name);
    }
    else if( S_ISDIR(st.st_mode) )
    {
        sprintf(buf+strlen(buf),
        "<tr><td><a href=\"%s/\">%s/</a></td></tr>",
        name,name);//path是目录时,就是多拼了一个'/',即%s-->%s/
    }

    printf("%s\n",buf);

    ret=send(cfd,buf,strlen(buf),0); //这里是循环里的send一次
    if(ret==-1)
    {
        perror("send_dir error");
        continue;
    }
    
    memset(buf,0,sizeof(buf));
  }

  sprintf(buf+strlen(buf),"</table></body></html>");
  send(cfd,buf,strlen(buf),0);//最后send "</table></body></html>"

  printf("dir send\n");

#if 0
   DIR* dir = opendir(dirname);
   if(dir == NULL){perror("opendir error");exit(1);}

   struct dirent* ptr=NULL;
   while( (ptr=readdir(dir)) !=NULL )
   {
    char* name=ptr->d_name;
   }
   closedir(dir);
#endif   
}

void deal_http_request(const char* file,int cfd)//判断文件是否存在(存在时get_file_type),调一些send开头的函数
{
    struct stat sbuf;
    int ret = stat(file,&sbuf); //判断浏览器请求的文件是否存在
    if(ret!=0)//请求文件不存在
    { 
      printf("client[%d]请求:%s \n",cfd,file);
      perror("stat error");

      send_error_page(cfd,"the file is not exist");//发自己写的not found
    }
    
    // printf("client[%d]请求文件:%s  type: %s\n",cfd,file,get_file_type(file));

    if(S_ISREG(sbuf.st_mode))//请求的是普通文件
    { 
      printf("client[%d]请求文件:%s\n",cfd,file);
      send_response_first_part(cfd,200,"OK",get_file_type(file),sbuf.st_size);
      send_file(cfd,file);
    }
    else if(S_ISDIR(sbuf.st_mode))//请求的是目录
    { 
      
      printf("client[%d]请求的是目录\n",cfd);
      send_response_first_part(cfd,200,"OK",get_file_type(".html"),-1);//len=-1,浏览器自己去算
      send_dir(cfd,file);

    }
    
}
    
void get_request_first_line(int cfd, int epfd)//获得访问资源信息,调deal_http_request()去处理 
{       
    char line[1024] = {0};    
  
    // 从cfd读取数据到line数组中，最多读取sizeof(line)-1个字节（留一个位置给字符串结束符'\0'）  
    ssize_t len = read(cfd, line, sizeof(line) - 1);   
    
    // printf("client[%d]:\n%s",cfd,line);
    if (len == -1) //read函数返回实际读取的字节数,或者在出错时返回-1
    {    
        perror("get_request_first_line, read error");    
        disconnect(cfd, epfd);    
        return;   
    }    
    // 在读取到的数据line中查找换行符'\n'  
    // strchr函数返回指向首次出现指定字符的指针，如果没有找到则返回NULL  
    char *newline = strchr(line, '\n');   
  
    if (newline != NULL) // 如果找到了换行符
    {    
        *newline = '\0'; //用null字符'\0'替换换行符，这样字符串就被正确地终止了  
        len = newline-line;//更新len的值,newline指向'\0', line指向开始位置
    }   
    else if(len == sizeof(line)-1) // 如果没有找到换行符，但line已经满了 
    {    
        // 可能客户端发送了一行非常长的数据，或者数据被截断了   
        fprintf(stderr, "get_request_first_line, Line too long or no newline found\n");    
        disconnect(cfd, epfd);    
        return; 
    }    


    if (len==0) //len为0，这意味着客户端可能已经关闭了连接
    {    
        printf("检测到client[%d]关闭\n",cfd);    
        disconnect(cfd, epfd);    
    }   
    else//len不为0,打印HTTP请求行并且尝试去解析
    {    
        // printf("client[%d]发来的http头(request首行):%s\n", cfd, line);    
        char method[16], path[256], protocol[16];    
        if (sscanf(line, "%[^ ] %[^ ] %[^ ]", method, path, protocol) == 3)//解析HTTP请求首行成功
        {    
          //printf("%s %s %s\n", method, path, protocol); 
            
            if(strncasecmp(method,"GET",3)==0)//如果是GET请求
             { 
                //此时method="GET", path="/xxx", protocol="HTTP/1.1"
               char* file;
               if(strcmp(path,"/")==0)//如果浏览器没有指定访问资源,默认显示资源目录中的内容
                 {
                    file="./";
                 }
                 else//否则,去取出指定的资源的文件名字
                 {
                   file = path+1; //去掉"/xxx"的‘/’,只留下"xxx"
                 }  
                 
               deal_http_request(file,cfd);//获得了访问资源名file,去处理

             }

        } 
        else // 否则，打印解析HTTP请求首行失败
        {
            printf("get_request_first_line,解析HTTP请求行失败\n");
        }    
       
    }  

}

void epoll_run(int port)//封装epoll机制
{
    int i=0;
    struct epoll_event all_events[MAXSIZE];

    int epfd = epoll_create(MAXSIZE);//创建一个epoll监听树
    if(epfd==-1){perror("epoll_create error");exit(1);}

    int lfd = init_listen_fd(port,epfd);//返回放到epoll监听树上的lfd
    
    while(1)
   {  
      int nready=0;
      nready= epoll_wait(epfd,all_events,MAXSIZE,1000);//The "timeout" parameter(1000ms) specifies the maximum wait time in milliseconds 
      if(nready==-1){perror("epoll_wait error");exit(1);}

      for(i=0;i<nready;i++)
      { 
        struct epoll_event* pev = &all_events[i];
        if(!(pev->events & EPOLLIN) )//不是读事件时
        {
          continue;
        }

        if(pev->data.fd == lfd)
        { 
           do_accept(lfd,epfd); //接受连接请求
        }
        else
        {
          get_request_first_line(pev->data.fd,epfd); //获得浏览器发来的http请求消息的首行
        }

      }
      
   }

}

int main(int argc,char* argv[])
{  
   
   if(argc<3)
   {
    printf("you should follow: ./http_server your_port your_path\n");
    return 1; //当参数不正确时，应该返回一个非零值
   }

   //在终端里面的pwd目录下，用“./http_server 9527 /home/dc/show”启动
   //argv[0]="./http_server" 
   //argv[1]="9527"
   //argv[2]="/home/dc/show"   
   int port = atoi(argv[1]);//获得端口
   int ret = chdir(argv[2]);//改变进程工作目录
   if(ret!=0){printf("%s\n",argv[2]);perror("chdir error");exit(1);}
   printf("This is a http-server program\n which is working show the contect of %s \nand using port[%d]\n",argv[2],port);

#if 0
//当launch.json 的"args": ["http_server","9527","/home/dc/show"],
// argv[0]="/home/dc/tcp_socket/TCP-SOCKET-B-S/http_server" 
// argv[1]="http_server" 
// argv[2]="9527" 
// argv[3]="/home/dc/show
   int port = atoi(argv[2]);//获得端口
   int ret = chdir(argv[3]);//改变进程工作目录
   if(ret!=0){printf("%s\n",argv[3]);perror("chdir error");exit(1);}
#endif

   epoll_run(port);//启动epoll监听

   return 0; 
}

