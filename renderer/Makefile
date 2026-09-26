CXX ?= g++
CXXFLAGS ?= -O3 -march=native -ffast-math -fopenmp -std=c++17 -Wall -Wno-misleading-indentation
BIN = bin/zawal

$(BIN): src/*.h src/render.cpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) src/render.cpp -o $(BIN)

clean:
	rm -rf bin
