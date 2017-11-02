
CFLAGS = -g -Wall -std=c++11
LDFLAGS = -pthread 
TAR_NAME = ex3.tar

FWLIB_SOURCES = FrameControl.cpp MapReduceFramework.cpp
EXEC_SOURCES = $(FWLIB_SOURCES) Search.cpp

HEADERS = FrameControl.h MapReduceFramework.h MapReduceClient.h
		  

EXEC_OBJS = $(EXEC_SOURCES:.cpp=.o)
FWLIB_OBJS = $(FWLIB_SOURCES:.cpp=.o)

IMAGES = FirstComeFirstServeGanttChart.jpg PrioritySchedulingGanttChart.jpg\
		RoundRobinGanttChart.jpg ShortestRemainingTimeFirstGanttChart.jpg
EXTRA_FILES = README Makefile FrameControl.h
TAR_FILES = $(EXEC_SOURCES) $(EXTRA_FILES) $(IMAGES) 

EXECUTABLE = Search
FWLIB = MapReduceFramework.a
 
all: $(EXECUTABLE) $(FWLIB) 


$(EXECUTABLE): $(EXEC_OBJS)
	$(CXX) $(LDFLAGS) $^ -o $@

$(FWLIB): $(FWLIB_OBJS)
	ar rcs $@ $^

%.o: %.cpp $(HEADERS)
	$(CXX) -c $(CFLAGS) $<

tar:
	tar -cvf $(TAR_NAME) $(TAR_FILES)


clean:
	rm -rf $(TAR_NAME) $(EXEC_OBJS) $(FWLIB) $(EXECUTABLE)


.PHONY:
	all tar clean
