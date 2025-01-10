# Makefile for compiling prog.c into prog

CC = gcc           # 指定編譯器
CFLAGS = -Wall     # 編譯選項，啟用所有警告

# 目標: 依賴
prog: prog.c
	$(CC) $(CFLAGS) -o prog prog.c

# 清理編譯生成的文件
clean:
	rm -f prog
