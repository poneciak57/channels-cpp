
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2

benchmark: 
	@echo "Usage: make benchmark/<name>"

example:
	@echo "Usage: make example/<name>"

benchmark/%: ./benchmarks/%.cpp
	@mkdir -p ./bin/benchmarks
	@$(CXX) $(CXXFLAGS) $< -o ./bin/benchmarks/$* -I./include -I./third_party
	@./bin/benchmarks/$*

example/%: ./examples/%.cpp
	@mkdir -p ./bin/examples
	@$(CXX) $(CXXFLAGS) $< -o ./bin/examples/$* -I./include -I./third_party
	@./bin/examples/$*

%: %.cpp
	@mkdir -p ./bin
	@$(CXX) $(CXXFLAGS) $< -o ./bin/$@ -I./include -I./third_party
	@./bin/$@