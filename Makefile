CC = gcc
CFLAGS = -lrt
SRC = log.c client.c server.c
OBJ = main.o
OUT = main

$(OUT): $(OBJ) $(SRC)
	$(CC) -o $(OUT) $(OBJ) $(SRC) $(CFLAGS)

main.o: main.c
	$(CC) -c main.c -o main.o

clean:
	rm -f $(OBJ) $(OUT)

