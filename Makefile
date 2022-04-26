.PHONY: all clean

CXXFLAGS = -std=c++14 -Wall

all: debug

debug: CXXFLAGS += -g -Og -DDEBUG
debug: solve
release: CXXFLAGS += -O3
release: solve

clean:
	rm -rf solve *.o *.dSYM/

solve: solve.cpp
