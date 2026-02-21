
CFLAGS=-std=c17 -Wall -Wextra -Werror # use C17 and enable warnings and extras

all:
	gcc main.c -o main $(CFLAGS) `sdl2-config --cflags --libs`
