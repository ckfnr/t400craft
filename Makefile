

CC = gcc

CFLAGS = -Wall -Wextra -std=c11

LIBS = -lSDL2 -lGL -lGLEW -lm

t400craft: src/main.c src/Shaders/shader_loader.c src/Mesh/mesh.c src/stb.c
	$(CC) $(CFLAGS) src/main.c src/Shaders/shader_loader.c src/Mesh/mesh.c src/stb.c -o t400craft $(LIBS)