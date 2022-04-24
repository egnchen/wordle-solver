.PHONY: all clean

CXXFLAGS = -std=c++17 -O2 -g -Wall
all: solve

clean:
	rm -f *.o solve

solve: solve.cpp