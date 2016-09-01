CC=g++
CFLAGS= -g -Wall -Werror
ADDR=src/

all: proxy

proxy: src/proxy.c
	$(CC) $(CFLAGS) -o proxy_parse.o -c src/proxy_parse.c
	$(CC) $(CFLAGS) -o proxy.o -c src/proxy.c
	$(CC) $(CFLAGS) -o bin/proxy proxy_parse.o proxy.o
	rm proxy*

clean:
	rm -f bin/proxy *.o

tar:
	tar -cvzf ass1.tgz proxy.c README Makefile proxy_parse.c proxy_parse.h
