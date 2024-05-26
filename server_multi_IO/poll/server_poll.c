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

    client[0].fd = lfd;            
    client[0].events = POLLIN;

    for (i = 1; i < OPEN_MAX; i++) 
    {
        client[i].fd = -1;
    }

    maxi = 0;

    while (1) 
    {   

        printf("poll....\n");
        nready = poll(client, maxi + 1, -1);

        printf("1. nready=%d\n",nready);
        
        if (client[0].revents & POLLIN)  //client[0]是lfd所在的,某一次lfd没有变化,直接跳过这个if
        {   
            printf("是lfd有变化，\n");

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

            printf("继lfd:%d(标号0) 之后，又写入到了一个到(标号:%d), \n",lfd,i);
             

            if (i == OPEN_MAX) sys_err("too many clients");


            if (i > maxi) {
                maxi = i;
            }
            
            printf("2.maxi=%d\n",maxi);


            if (--nready == 0) 
            {
                continue;
            }
        }

        for (i = 1; i <= maxi; i++) //这个for就是去看 哪个除了lfd外 的文件描述符有了变化
        {   
            printf("看看i=%d是否与变化？\n",i);
            
            sockfd = client[i].fd;
            
            if (sockfd  < 0) //要跳过那些“离散”的，“没有描述符”的 pollfd结构体数组的元素
            {printf("%d已经关闭了，看下一个\n",i); continue;}
            

            if (client[i].revents & POLLIN)  //如果当前的i对应的描述符没有变化,跳过这个if
            { //有变化，那就进来read()
                printf(" %d有变化,进入了if，去read() \n",i);

                n = read(sockfd, buf, sizeof(buf)-1); // leave space for null terminator
                
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
                    buf[n] = '\0'; // null-terminate the string
                    printf("received %zd bytes: %s\n", n, buf);
                    for (j = 0; j < n; j++) 
                    {
                        buf[j] = toupper(buf[j]);
                    }
                    write(sockfd, buf, n);
                    
                     // 清除事件状态
                    client[i].revents = 0;
                     
                }


                if (--nready == 0) 
                {   printf("处理完了，那就break了\n");
                    break;
                }

            }
            else
            {
                printf(" %d没有变化，下一个\n",i);
            }
            
        }


    }
    
//  close(lfd);
 return 0;
}


// 2024.5.22. log by dc@hasee
// 
// 
// poll....
// 
// 1. nready=1
// 是lfd有变化，
// -------- connected from: client ip:127.0.0.1 port:41620
// 继lfd:3(标号0) 之后，又写入到了一个到(标号:1), 
// 2.maxi=1
// poll....
// 
// 1. nready=1
// 看看i=1是否与变化？
// 3. nready=1
// received 1023 bytes: m
// 处理完了，那就break了
// poll....
// 1. nready=1
// 看看i=1是否与变化？
// 3. nready=1
// received 1 bytes: 
// 处理完了，那就break了
// poll....
// 
