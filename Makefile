all:
	gcc -Wall src/*.c -o game -lSDL3

clean:
	rm game
