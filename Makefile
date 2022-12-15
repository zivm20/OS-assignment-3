CXX=gcc
CXXFLAGS= -Werror 

all: benchmark 

run: benchmark
	./benchmark
	./benchmark A
	./benchmark B
	./benchmark C
	./benchmark D
	./benchmark E
	./benchmark F
	./benchmark G
	

benchmark: benchmark.c
	$(CXX) $(CXXFLAGS) $^ -o $@

%.o: %.c
	$(CXX) $(CXXFLAGS) $^ -o $@


clean:
	rm -f *.o benchmark
