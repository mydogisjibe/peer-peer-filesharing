CC      = gcc
CFLAGS  = -O
LDFLAGS  = -O -g

all: tracker peer initial_peer primes.txt

initial_peer: initial_peer.o
	$(CC) -o $@ $^ $(LDFLAGS)

peer: peer.o file_network.o
	$(CC) -o $@ $^ $(LDFLAGS)

tracker: tracker.o
	$(CC) -o $@ $^ $(LDFLAGS)

setPrimes: setPrimes.o
	$(CC) -o $@ $^ $(LDFLAGS)

primes.txt: setPrimes
	./setPrimes

clean:
	rm -f *.o
	rm -f tracker
	rm -f peer
	rm -f initial_peer
	rm -f setPrimes
	rm -f primes.txt
	rm -f download.txt
