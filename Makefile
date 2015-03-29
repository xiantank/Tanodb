all:
	gcc -Wall odb.c -lcrypto -g -o odb
clean:
	rm -f db*
