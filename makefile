# Compiler
CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -g

# Target executable name
TARGET = readCollection

# Source files
SRC = readCollection.cpp

# Default rule
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

# Clean rule
clean:
	rm -f $(TARGET) *.o
