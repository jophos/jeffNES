all: ./bin/jeffNES ./bin/NEShead

./bin/jeffNES: ./obj/libNEShead.o jeffNES.c
	mkdir -p ./bin/
	gcc -g -o ./bin/jeffNES jeffNES.c ./obj/libNEShead.o -I./ `pkg-config --cflags --libs gtk+-3.0 sdl2`

./obj/libNEShead.o: libNEShead.c
	mkdir -p ./obj/
	gcc -g -o ./obj/libNEShead.o -c libNEShead.c -I./

./bin/NEShead: ./obj/libNEShead.o
	mkdir -p ./bin/
	gcc -g -o  ./bin/NEShead NEShead.c ./obj/libNEShead.o -I./
