.PHONY:clean

CXXFLAGS=-std=gnu++11 -ggdb3
LDLIBS=-lpthread

parse:

clean:
	rm -f parse
