# NAME:	Brian Be
# EMAIL:	bebrian458@gmail.com
# ID: 	204612203

CC = gcc
CFLAGS = 
DEBUG =
LDLIBS = -lmraa -lm -lpthread
DIST = lab4c_tcp.c lab4c_tls.c Makefile README

default: lab4c_tcp lab4c_tls

lab4c_tcp: lab4c_tcp.c
	$(CC) $(LDLIBS) -o $@ $<

lab4c_tls: lab4c_tls.c
	$(CC) $(LDLIBS) -o $@ $<

clean:
	rm -f lab4c_tcp lab4c_tls *.tar.gz

dist:
	tar -czf lab4c-204612203.tar.gz $(DIST)

