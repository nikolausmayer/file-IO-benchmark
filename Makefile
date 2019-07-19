##
#
# Author: Nikolaus Mayer, 2019 (mayern@cs.uni-freiburg.de)
#
##

## Where to look for includes (default is 'here')
INCLUDE_DIRS = -I. 

## Compiler
CXX = g++

## Compiler flags; extended in 'debug'/'release' rules
CXXFLAGS = -Wall -Wextra -std=c++17 -pthread

## Linker flags
LDFLAGS = -pthread

## Default name for the built executable
TARGET = iobench

## Every *.cc/*.cpp file is a source file
SRCS = $(wildcard src/*.cpp)
HEADERS = $(wildcard src/*.h)


## Build a *.o object file for every source file
OBJS = $(addsuffix .o, $(basename $(SRCS)))


## Tell make that e.g. 'make clean' is not supposed to create a file 'clean'
##
## "Why is it called 'phony'?" -- because it's not a real target. That is, 
## the target name isn't a file that is produced by the commands of that target.
.PHONY: all clean debug release


## Default is release build mode
all: release
	
## When in debug mode, don't optimize, and create debug symbols
debug: CXXFLAGS += -O0 -g
debug: $(TARGET)
	
## When in release mode, optimize
release: CXXFLAGS += -O2
release: $(TARGET)

## Remove built object files and the main executable
## The dash ("-") in front of "rm" tells make to ignore errors. In this
## case, executing "make clean" does not error-terminate when no object
## file or executable is found (which would be the usual behaviour).
clean:
	$(info ... deleting built object files and executable  ...)
	-rm src/*.o $(TARGET)

## The main executable depends on all object files of all source files
$(TARGET): $(OBJS)
	$(info ... linking $@ ...)
	$(CXX) $^ $(LDFLAGS) -o $@

## Every object file depends on its source and the makefile itself,
## and on all header files (just to make sure that header changes
## prompt recompilation; actually this is totally overkill)
%.o: %.cpp Makefile $(HEADERS)
	$(info ... compiling $@ ...)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@




data: example-data/make-random-example-files.sh
	$(info ... generating random files ...)
	cd example-data && bash make-random-example-files.sh

dataclean: example-data/cleanup.sh
	$(info ... deleting random files ...)
	cd example-data && bash cleanup.sh


