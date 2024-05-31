poll 借助定义在poll.h里的结构体pollfd，

  struct pollfd
  {
    int fd;			        /* File descriptor to poll.  */
    short int events;		/* Types of events poller cares about.  */
    short int revents;		/* Types of events that actually occurred.  */
  };


struct pollfd client[OPEN_MAX];

思路：
    核心逻辑：while(1)里面一个if、一个for，if只看lfd，for处理其他的cfd

---------------------------------------------------------
   //结构体数组的第一个元素client[0]，存放lfd相关的信息。
   client[0].fd = lfd;            
   client[0].events = POLLIN;
   
   //其余的fd都置 -1 表没有描述符
   for(i = 1; i < OPEN_MAX; i++) client[i].fd = -1;
   
   while (1) 
    {   
        nready = poll(client,maxi+1,-1); //TIMEOUT=-1, block until an event occurs.
        
        if (client[0].revents & POLLIN) //单独看lfd所在的结构体,若满足是有新连接
        {   
            cfd = accept();
            
            //... ...
            
            if (--nready == 0) //这里的“--nready”：处理完了lfd。
              continue;       //如果nready为0,此次没有其他事件,直接下一次poll
        }


        //进入下面这个for说明：除了lfd外，仍然有文件描述符有了变化(故i=1开始)
        for (i = 1; i <= maxi; i++) 
        {   
            sockfd = client[i].fd;

            if (sockfd  < 0) //跳过“没有描述符”的pollfd结构体数组的元素
              continue;
            
            if (client[i].revents & POLLIN)  
            { 
                //有变化，那就进来read()

                if(--nready == 0) //每处理完一个,让nready-1
                   break;         //处理完了出for
            }
        }
    }
-----------------------------------------------------------
