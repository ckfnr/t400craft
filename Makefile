CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Isrc
LIBS = -lSDL2 -lGL -lGLEW -lm

SRCS = src/main.c \
       src/Shaders/shader_loader.c \
       src/Mesh/mesh.c \
       src/stb.c \
       src/Camera/Camera.c \
       src/World/chunk.c \
       src/World/world.c \
       src/World/chunk_mesh.c

t400craft: $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o t400craft $(LIBS)
