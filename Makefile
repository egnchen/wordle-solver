.PHONY: all clean

CXXFLAGS = -std=c++17 -O2 -g -Wall
all: solve

clean:
	rm -rf solve *.o *.dSYM/

solve: solve.cpp
