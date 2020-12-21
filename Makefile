# 3201 Makefile

PROG =	3201
OBJS =	3201.o as3201.o

CFLAGS = -g -O0

all: ${OBJS}
	${CC} ${LDFLAGS} -o ${PROG} 3201.o
	${CC} ${LDFLAGS} -o as3201 as3201.o
	./as3201 fib.asm fib.com

clean:
	rm -f ${PROG} ${OBJS} as3201 as3201.o fib.com *.core
