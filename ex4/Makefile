
CFLAGS = -g -std=c++11 -DNDEBUG -Wall -D_FILE_OFFSET_BITS=64
FUSE_FLAGS = `pkg-config fuse --cflags --libs`

TAR_NAME = ex4.tar
SOURCES = CachingFileSystem.cpp LogFile.cpp Cache.cpp Block.cpp
HEADERS = LogFile.h Cache.h Block.h
OBJS = $(SOURCES:.cpp=.o)
EXTRA_FILES = README Makefile
TAR_FILES = $(SOURCES) $(HEADERS) $(EXTRA_FILES)

EXEC = CachingFileSystem

.DEFAULT_GOAL = $(EXEC)
 

all: $(EXEC) tar

$(EXEC): $(OBJS)
	$(CXX) $(FUSE_FLAGS) $^ -o $@


%.o: %.cpp $(HEADERS)
	$(CXX) -c $(CFLAGS) $<

tar:
	tar -cvf $(TAR_NAME) $(TAR_FILES)


clean:
	rm -rf $(TAR_NAME) $(OBJS) $(EXEC)


.PHONY:
	all tar clean
