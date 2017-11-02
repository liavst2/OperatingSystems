
CFLAGS = -g -Wall -Wno-unused-value -std=c++11
TAR_NAME = ex1.tar
SOURCES = osm.cpp
HEADERS = osm.h
OSMLIB = libosm.a
OBJS = $(SOURCES:.cpp=.o)
EXTRA_FILES = README Makefile
TAR_FILES = $(SOURCES) $(EXTRA_FILES)

.DEFAULT_GOAL = $(OSMLIB)
 

all: $(OSMLIB) tar


$(OBJS): $(SOURCES) $(HEADERS)
	$(CXX) -c $(CFLAGS) $<


$(OSMLIB): $(OBJS)
	ar rcs $@ $^

tar:
	tar -cvf $(TAR_NAME) $(TAR_FILES)


clean:
	rm -f $(TAR_NAME) $(OBJS) $(OSMLIB)


.PHONY:
	all tar clean
