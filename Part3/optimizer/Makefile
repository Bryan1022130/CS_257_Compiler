CXX = g++
CXXFLAGS = -std=c++17 -g -O0 -Wall -Wextra

LLVM_CONFIG = llvm-config-17
LLVM_CXXFLAGS = $(shell $(LLVM_CONFIG) --cxxflags)
LLVM_LDFLAGS  = $(shell $(LLVM_CONFIG) --ldflags)
LLVM_LIBS     = $(shell $(LLVM_CONFIG) --libs core irreader support)

LLVM_C_INCLUDES = -I /usr/include/llvm-c-17/

all: local_optimizer global_optimizer

# Local passes only: constant folding, CSE, DCE
local_optimizer: local_optimizer.cpp
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS) $(LLVM_C_INCLUDES) \
		-o $@ $^ $(LLVM_LDFLAGS) $(LLVM_LIBS)

# Local + global passes: constant propagation (reaching defs) + live variable analysis
global_optimizer: global_optimizer.cpp
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS) $(LLVM_C_INCLUDES) \
		-o $@ $^ $(LLVM_LDFLAGS) $(LLVM_LIBS)

clean:
	rm -f local_optimizer global_optimizer *.o
