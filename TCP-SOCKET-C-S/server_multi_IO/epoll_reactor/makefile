src=$(wildcard *.c)              
obj=$(patsubst %.c,%.o,$(src))    

ALL:server

server:$(obj)
	gcc $^ -o $@ -g
%.o:%.c
	gcc -c $^ -o $@ -g

clean:
	-rm -rf $(obj) server

# makefile语法规则
# [目标]:[依赖]
# [一个tab缩进][命令]

# 三个自动变量 $@ ，$^ ，$<只能出现在[命令]中，以简化[命令]的书写
# $@在[命令]中替换[目标]
# $^ 替换 所有的[依赖] ， $< 替换 第一个[依赖]
