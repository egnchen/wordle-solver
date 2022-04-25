.PHONY: all clean

CXXFLAGS = -std=c++17 -Wall -lprofiler

all: debug

debug: CXXFLAGS += -g -DDEBUG
debug: solve
release: CXXFLAGS += -O3
release: solve

clean:
	rm -rf solve *.o *.dSYM/

solve: solve.cpp
