CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Isrc
LIBS = -lSDL2 -lGL -lGLEW -lm

t400craft: src/main.c src/Shaders/shader_loader.c src/Mesh/mesh.c src/stb.c src/Camera/Camera.c src/World/chunk.c src/World/chunk_mesh.c
	$(CC) $(CFLAGS) src/main.c src/Shaders/shader_loader.c src/Mesh/mesh.c src/stb.c src/Camera/Camera.c src/World/chunk.c src/World/chunk_mesh.c -o t400craft $(LIBS)