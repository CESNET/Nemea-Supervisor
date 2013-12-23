CC = gcc
CFLAGS = `/usr/bin/xml2-config --libs --cflags`
OBJS = objs/supervisor.o
BIN = ./supervisor
CONFIG_FILE = supervisor_config.xml


all: $(OBJS)
	make compile

run: $(BIN)
	$(BIN) $(CONFIG_FILE)

compile: $(BIN)

clean:
	rm -rf objs $(BIN)

$(BIN): $(OBJS)
	$(CC) $(OBJS) $(CFLAGS) -o $(BIN)

objs/supervisor.o: ./supervisor.c
	mkdir -p objs
	$(CC) -c supervisor.c $(CFLAGS) -o objs/supervisor.o
