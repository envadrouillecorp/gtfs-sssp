.PHONY:clean

CXXFLAGS=-std=gnu++14 -ggdb3
LDLIBS=-lpthread

parse:

clean:
	rm -f parse
