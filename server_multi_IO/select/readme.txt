2024.5.18

使用select()及其相关函数 实现多路I/O的服务端

【思路】
 	lfd = socket();
 	bind();
 	listen();
 	
 	
 	fd_set allset;    //创建集合allset
 	FD_ZERO(&allset); //清空 allset
 	
 	FD_SET(lfd,&allset); //将lfd添加至 集合allset
 	
 	
   while(1)
{
 	fd_set readset = allset; //每次都把allset赋值给readset 
 	nready = select( lfd+1, &readset, NULL, NULL, NULL ); //用readset去监听文件描述符
 	
 	if(nready>0) 
 	{ 
	   //接下来，再去检查nready个事件中,具体是哪些文件描述符

 	   if( FD_ISSET(lfd,&readset) )
 	    { 
 	       //传出的readset还有lfd，说明有客户端连接
 	       cfd = accept();       //此时给该客户端一个cfd
 	       FD_SET(cfd, &allset); //并且此cfd也加入到allset,下一次进入while,就要去监听这个cfd了
 	    }

		for(i=lfd+1; i<= 此     时最大的文件描述符; i++)
		{
			FD_ISSET(i,&readset ); //遍历去看传出的readset中,有哪些文件描述符i有处理需求
			
				read();
				小写-->大写
				write();
			
		}    
                             	
 	}
 	 
}
