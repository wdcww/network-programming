2024.5.23

epoll()实现IO多路转接

lfd=socket();
bind();
listen();

int epfd = epoll_create(1024);  //创建监听红黑树的树根

epoll_ctl(epfd,EPOLL_CTL_ADD,lfd); //将lfd add到树上 

epoll_wait();