build:
	gcc -o server main.c

run:	build
	./server

clean:
	rm -f server