CC      = gcc
CFLAGS  = -O
LDFLAGS  = -O 

setPrimes: setPrimes.o
	$(CC) -o $@ $^ $(LDFLAGS)

primes.txt: setPrimes
	./setPrimes

clean:
	rm *.o
	rm setPrimes
	rm primes.txt
