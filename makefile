# Compiler
CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -g

# Executable names
TARGETS = readCollection.exe merge.exe index.exe

# Default rule
all: $(TARGETS)

readCollection.exe: readCollection.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

merge.exe: mergeTempFiles.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

index.exe: index.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

# Clean rule
clean:
	del /Q $(TARGETS) 2>nul || true
