2024.4.30

这个目录里是服务端(server)

lfd用于监听

然后，弄了2层while

    一旦有某个连接成功的客户端，则一直在内层while，(使用此时的cfd通信)
直到这个连接的客户端主动断连，才退出内层while，回到外层while

    在外层while里，重复 cfd = accept(), 成功后，用当前的cfd通信

把close(lfd)写在了2层while之外，意味着永不主动结束server


2024.5.


