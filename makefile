CC	= gcc
CF	= -Wall -Wextra -g

BIN	= rip \
	  write

all:	$(BIN)

rip:	src/rip.c
	$(CC) $< -o $@ $(CF) -ljpeg $(shell sdl-config --cflags --libs) -lm

write:	src/write.c
	$(CC) $< -o $@ $(CF)

.PHONY:	clean

clean:
	rm -f $(BIN) out.mid tmp/*
