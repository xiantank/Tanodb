all:
	gcc -Wall Tdb.c -lcrypto -g -o odb
init:
	./odb -i -p db
clean:
	rm -f db*
