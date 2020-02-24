CC      = gcc
CFLAGS  = -O
LDFLAGS  = -O -g

initial_peer: initial_peer.o
	$(CC) -o $@ $^ $(LDFLAGS)

peer: peer.o
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
	rm -f setPrimes
	rm -f primes.txt
