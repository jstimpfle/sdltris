CFLAGS = -std=c89 -Wall -Wextra -pedantic

all: sdltris

sdltris: sdltris.c
	$(CC) $(CFLAGS) -I /usr/include/SDL2 -o $@ -lSDL2 -lrt $^

clean:
	rm -f sdltris
