#include<string.h>
#include<stdio.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<ctype.h>
#include <sys/types.h>
#include <sys/socket.h>

#define SERV_PORT 9527

int main()
{  
    struct sockaddr_in serv_addr,clie_addr;
    socklen_t clie_addr_len;
    int lfd;
    char buf[BUFSIZ];
    char str[INET_ADDRSTRLEN];
    int i,n;
    
    //socket()
    lfd = socket(AF_INET,SOCK_DGRAM,0);
    
    //bind()
    bzero(&serv_addr,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(SERV_PORT);
    bind(lfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr));

    while(1)
    {
        clie_addr_len =sizeof(clie_addr);
        //recvfrom()
        n = recvfrom(lfd,buf,BUFSIZ,0,(struct sockaddr*)&clie_addr,&clie_addr_len);
        if(n==-1) perror("recvfrom error");

        printf("received from %s:%d : %s \n",inet_ntop(AF_INET,&clie_addr.sin_addr,str,sizeof(str)),
                                        ntohs(clie_addr.sin_port),
                                        buf);
        for(i=0;i<n;i++)
           buf[i]=toupper(buf[i]);

        n = sendto(lfd,buf,n,0,(struct sockaddr*)&clie_addr,sizeof(clie_addr));
        if(n==-1) perror("sendto error");
        
    }

    close(lfd);
    


    return 0;
}