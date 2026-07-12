CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
LDFLAGS = 

# Source files
SOURCES = bplus_tree.cpp test_bplus_tree.cpp
HEADERS = bplus_tree.hpp
OBJECTS = $(SOURCES:.cpp=.o)
EXECUTABLE = bplus_tree_test

# Targets
all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^
	@echo "✓ Build successful: $(EXECUTABLE)"

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(EXECUTABLE)
	./$(EXECUTABLE)

clean:
	rm -f $(OBJECTS) $(EXECUTABLE) *.dat
	@echo "✓ Cleaned"

rebuild: clean all

.PHONY: all run clean rebuild
