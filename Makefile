transport: zad.o
	gcc -o transport -Wall -Wextra -std=c17 zad.o

zad.o:
	gcc -c zad.c

clean:
	rm ./zad.o

disclean:
	rm ./zad.o ./transport