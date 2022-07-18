CC	= gcc
CF	= -Wall -Wextra -g

BIN	= notes \
	  write

all:	$(BIN)

notes:	src/notes.c
	$(CC) $< -o $@ $(CF) -ljpeg $(shell sdl-config --cflags --libs) -lm

write:	src/write.c
	$(CC) $< -o $@ $(CF)

.PHONY:	clean

clean:
	rm -f $(BIN) tmp/*
