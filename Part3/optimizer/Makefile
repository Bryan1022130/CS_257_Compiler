CXX = g++
CXXFLAGS = -std=c++17 -g -O0 -Wall -Wextra

LLVM_CONFIG = llvm-config-17
LLVM_CXXFLAGS = $(shell $(LLVM_CONFIG) --cxxflags)
LLVM_LDFLAGS  = $(shell $(LLVM_CONFIG) --ldflags)
LLVM_LIBS     = $(shell $(LLVM_CONFIG) --libs core irreader support)

LLVM_C_INCLUDES = -I /usr/include/llvm-c-17/

TARGET = optimizer
SRC = optimizer.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS) $(LLVM_C_INCLUDES) -o $@ $^ $(LLVM_LDFLAGS) $(LLVM_LIBS)

clean:
	rm -f $(TARGET) *.o
