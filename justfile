all: build run

build:
	gcc -std=gnu11 *.c -o lisp -lm

run:
	./lisp
