all: build run

build:
	gcc -std=c11 *.c -o lisp -lm

run:
	./lisp
