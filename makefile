# Compiler
CXX = g++
# Flags
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 
# Executable
TARGET = log_analyzer
SOURCES = main.cpp LogAnalyzer.cpp
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp LogAnalyzer.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
