all: inf136767_s inf136767_k
inf136767_s: inf136767_s.o
	gcc -o inf136767_s inf136767_s.o

inf136767_k: inf136767_k.o
	gcc -o inf136767_k inf136767_k.o

inf136767_s.o:inf136767_s.c
	gcc -c -o inf136767_s.o inf136767_s.c

inf136767_k.o:inf136767_k.c
	gcc -c -o inf136767_k.o inf136767_k.c
