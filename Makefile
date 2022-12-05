CXX=gcc
CXXFLAGS= -Werror 

all: benchmark 

benchmark: benchmark.c
	$(CXX) $(CXXFLAGS) $^ -o $@

%.o: %.c
	$(CXX) $(CXXFLAGS) $^ -o $@


clean:
	rm -f *.o benchmark
