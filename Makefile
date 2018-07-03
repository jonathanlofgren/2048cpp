CC=g++-8
CFLAGS=-fopenmp -I.
DEPS = bitboard.h types.h search.h
OBJ = main.o bitboard.o search.o 

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

2048cpp: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f *.o 2048cpp