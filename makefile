all:
	gcc -c mylib.c
	gcc OSproject.c -o app1 mylib.o
run:
	app1