linker: linker.c
	gcc -g -Wall linker.c -o linker

clean:
	rm -rf linker *~
