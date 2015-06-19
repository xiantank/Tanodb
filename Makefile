all:
	clear
	gcc -Wall Tdb.c -lcrypto -g -o odb
init:
	./odb -i -p db
clean:
	rm -f db*
re:
	make clean
	make
	make init
