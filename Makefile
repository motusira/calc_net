.PHONY: all

CFLAGS = -Wall -Wextra

all:
	g++ ${CFLAGS} src/server/server.cc -o build/server
	g++ ${CFLAGS} src/client/client.cc -o build/client
