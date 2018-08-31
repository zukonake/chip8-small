chip8 : chip8.c
	gcc $^ -lSDL2 -Ofast -Wall -Wextra -o $@
