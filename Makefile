CXX = c++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wpedantic

.PHONY: all clean
all: f1predict

f1predict: src/main.cpp src/model.cpp src/model.hpp src/types.hpp src/rules.hpp
	$(CXX) $(CXXFLAGS) src/main.cpp src/model.cpp -o $@

clean:
	$(RM) f1predict
