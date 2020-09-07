CC = icc
CFLAGS = -O3 -g
LIBS = -lpthread -lm

all: benchmark

skeleton: skeleton.c
	$(CC) $(CFLAGS) -c skeleton.c $(LIBS)

benchmark: benchmark.c skeleton
	$(CC) $(CFLAGS) -o benchmark benchmark.c skeleton.o $(LIBS)

clean:
	rm -fr skeleton.o benchmark
