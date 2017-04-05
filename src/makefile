# source files
SRC = $(wildcard *.cpp)
OBJ = $(SRC:%.cpp=%.o)
EXE = $(SRC:%.cpp=%)

# include directories
INCLUDES = -I.

# C++ compiler flags (-g -o2 -Wall)
CCFLAGS = -O3 -std=c++11

# compiler
COMPILE = g++

# library paths
LIBS =

# compile flags
LDFLAGS = -std=c++11

.SUFFIXES : .cpp

.DEFAULT : $(EXE)

%.o : %.cpp
	$(COMPILE) $(INCLUDES) $(CCFLAGS) -c $< -o $@

$(EXE) : $(OBJ)
	$(COMPILE) $< $(LIBS) $(LDFLAGS) -o $@

clean :
	rm -f $(OBJ) $(EXE)
