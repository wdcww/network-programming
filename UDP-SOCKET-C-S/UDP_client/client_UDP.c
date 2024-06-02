#include<string.h>
#include<stdio.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<ctype.h>
#include <sys/types.h>
#include <sys/socket.h>

int main()
{
   struct sockaddr_in serv_addr;
   int sfd,n;
   char buf[BUFSIZ];

   sfd = socket(AF_INET,SOCK_DGRAM,0);

   bzero(&serv_addr,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    inet_pton(AF_INET,"127.0.0.1",&serv_addr.sin_addr);
    serv_addr.sin_port = htons(9527);
    
   //  bind(sfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr));

    while(fgets(buf,BUFSIZ,stdin)!=NULL)
    {
       n = sendto(sfd,buf,strlen(buf),0,(struct sockaddr*)&serv_addr,sizeof(serv_addr));
       if(n==-1) perror("sendto error");

       n = recvfrom(sfd,buf,BUFSIZ,0,NULL,0); //NULL,0表示不关心对端信息
       if(n==-1) perror("recvfrom error");

       printf("收到回复： %s \n",buf);
    }
    
    printf("断开\n");
    close(sfd);

    return 0;
}