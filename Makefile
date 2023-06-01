.PHONY: all clean

CXXFLAGS = -MMD -std=c++17 -Wall

all: debug

debug: CXXFLAGS += -g -Og -DDEBUG
debug: solve
release: CXXFLAGS += -O3
release: solve

clean:
	rm -rf solve *.o *.d *.dSYM/

solve: solve.o wordle.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $? -o $@
