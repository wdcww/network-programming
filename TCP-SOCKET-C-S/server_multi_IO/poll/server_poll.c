#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>

#define MAXLINE 1024
#define OPEN_MAX 1024
#define SERV_PORT 9527

void sys_err(const char *str) {
    perror(str);
    exit(1);
}

int main() {
    int i, j, maxi, lfd, cfd, sockfd;
    int nready;
    ssize_t n;
    char client_IP[30];
    char buf[MAXLINE];
    socklen_t clilen;

    struct pollfd client[OPEN_MAX];
    struct sockaddr_in cliaddr, servaddr;

    lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1) {
        sys_err("socket error");
    }

    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    if (bind(lfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1) {
        sys_err("bind error");
    }

    if (listen(lfd, 128) == -1) {
        sys_err("listen error");
    }
    
    //--------init-----------------
    client[0].fd = lfd;            
    client[0].events = POLLIN;
    for (i = 1; i < OPEN_MAX; i++) 
    {
        client[i].fd = -1;
    }
    //-----------------------------
    
    maxi = 0;

    while (1) 
    {   
        nready = poll(client, maxi + 1, -1);//if TIMEOUT is -1, block until an event occurs.
        
        if (client[0].revents & POLLIN) //client[0]是lfd所在的,某一次lfd没有变化,直接跳过这个if
        {   
            clilen = sizeof(cliaddr);
            cfd = accept(lfd, (struct sockaddr*)&cliaddr, &clilen);
            if (cfd == -1) {
                sys_err("accept error");
            }

            printf("-------- connected from: client ip:%s port:%d\n",
                   inet_ntop(AF_INET, &cliaddr.sin_addr.s_addr, client_IP, sizeof(client_IP)),
                   ntohs(cliaddr.sin_port));

            for (i = 1; i < OPEN_MAX; i++) 
            {
                if (client[i].fd < 0) 
                {
                    client[i].fd = cfd;
                    client[i].events = POLLIN;
                    break;
                }
            }

            if (i == OPEN_MAX) 
              sys_err("too many clients");

            if (i > maxi) 
              maxi = i;
            
            if (--nready == 0) //这里的“--nready”：处理完了lfd。如果nready为0,此次没有其他事件,直接下一次poll
              continue;
        }


        //进入下面这个for说明：除了lfd外，仍然有文件描述符有了变化(故i=1开始)
        for (i = 1; i <= maxi; i++) 
        {   
            sockfd = client[i].fd;
            if (sockfd  < 0) //要跳过那些“离散”的，“没有描述符”的 pollfd结构体数组的元素
            {
                printf("client[%d]已经关闭了\n",i); 
                continue;
            }

            if (client[i].revents & POLLIN)  //如果当前的i对应的描述符没有变化,跳过这个if
            { //有变化，那就进来read()
                n = read(sockfd, buf, sizeof(buf)); // leave space for null terminator

                if(buf[0]=='.')
		         { //收到"."，说明客户端那边自己调用close()关闭自己。想主动断连
		            write(sockfd,"bye",4); //回复他 bye
                  //getpeername()用于获取已连接套接字对端的地址信息。
                  getpeername(sockfd, (struct sockaddr*)&cliaddr, &clilen);
                  inet_ntop(AF_INET, &cliaddr.sin_addr, client_IP, sizeof(client_IP));
                  printf("ip: %s port: %d 主动断开\n", client_IP, ntohs(cliaddr.sin_port));
                  close(sockfd);
                  client[i].fd = -1; 
               }
                else if (n < 0) 
                {
                    printf("read error: %zd\n", n);
                    close(sockfd);
                    client[i].fd = -1;
                } 
                else if (n == 0) 
                {
                    printf("client 意外 disconnected\n");
                    close(sockfd);
                    client[i].fd = -1; 
                } 
                else if (n > 0) 
                {
                    printf("client[%d]: %s\n",sockfd,buf);
                    for (j = 0; j < n; j++) 
                    {
                        buf[j] = toupper(buf[j]);
                    }
                    printf("回复client[%d]: %s\n",sockfd,buf);
                    write(sockfd, buf, n);
                }



                if (--nready == 0) //这里的“--nready”，每处理完一个,让nready-1,如果nready为0,证明处理结束
                   break;

            }
            
        }


    }
    
 close(lfd);
 return 0;
}

