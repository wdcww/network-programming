src=$(wildcard *.c)                #找到当前目录中所有的.c文件
obj=$(patsubst %.c,%.o,$(src))     #把这些.c变成.o文件

ALL:client                     # ALL用于指定规则的最终目标

client:$(obj)
	gcc $(obj) -o client

%.o : %.c
	gcc -c $< -o $@   # 这里"$@"(规则里的目标),指的是“%.o”匹配的"%.o"

clean:
	-rm -rf $(obj) client
# “-rm”的“-”是为了出错仍然执行

# 之所以从.c到.o，再.o到可执行，是因为多个.c一起编译，如果某次仅有某个.c改变，那不用
# 把其他的.c再次编译，这十分耗时。
#
# 如果直接.c到可执行,makefile可以是下面这样：
# src=$(wildcard *.c)              
# obj=$(patsubst %.c,% ,$(src))   
# ALL:$(obj) 
# % : %.c
#	gcc $< -o $@  

