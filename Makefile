linker: linker.c
	gcc -g -Wall -O linker.c -o linker

clean:
	rm -rf linker *~
