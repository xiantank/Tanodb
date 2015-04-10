all:
	gcc -Wall 2odb.c -lcrypto -g -o odb
init:
	./odb -i -p db
clean:
	rm -f db*
