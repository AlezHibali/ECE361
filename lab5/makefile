TARGET = server client
DEPS = helper.h
LDFLAGS = -pthread

all: ${TARGET}
server: server.c helper.h ${DEPS}
	gcc server.c -o server ${LDFLAGS}
deliver: client.c helper.h ${DEPS}
	gcc client.c -o client ${LDFLAGS}
clean:  
	rm -f ${TARGET}