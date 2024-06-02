
server:
  
     lfd = socket(AF_INET,SOCK_DGRAM,0);

     bind();

     while(1)
     {
        recvfrom();
        处理
        sendto();
     }

     close()