CC = g++
CFLAGS = -std=c++17 -Wall -Wextra -g
LDFLAGS = -lglfw -lGLEW -lGL -lglm

all: OpenGL1

OpenGL1: main.o OpenGL1.o
	$(CC) $(CFLAGS) -o OpenGL1 main.o OpenGL1.o $(LDFLAGS)

main.o: main.cpp
	$(CC) $(CFLAGS) -c main.cpp

OpenGL1.o: OpenGL1.cpp
	$(CC) $(CFLAGS) -c OpenGL1.cpp

clean:
	rm -f *.o OpenGL1
