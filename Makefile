main: main.c
	$(CC) main.c -o main -Wall -Wextra -pedantic -std=c99
db: main.c
	gcc -g main.c -o db
