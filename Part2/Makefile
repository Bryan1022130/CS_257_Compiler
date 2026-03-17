CXX = g++
CC  = gcc

CXXFLAGS = -std=c++17 -g -O0 -Wall -Wextra
CFLAGS   = -g -O0 -Wall -Wextra

LLVM_CONFIG = llvm-config-17
LLVM_CXXFLAGS = $(shell $(LLVM_CONFIG) --cxxflags)
LLVM_LDFLAGS  = $(shell $(LLVM_CONFIG) --ldflags)
LLVM_LIBS     = $(shell $(LLVM_CONFIG) --libs core support)

LLVM_C_INCLUDES = -I /usr/include/llvm-c-17/

# ---- Part1 paths ----
P1 = ../Part1/minic_frontend
P1_AST = ../Part1/ast

# ---- Targets ----
TARGET = ir_builder
OUT_DIR = out
TEST_DIR = builder_tests
TESTS = p1 p2 p3 p4 p5 p6

# ---- Sources ----
P2_SRCS = main.cpp ir_builder.cpp

P1_PARSER_SRCS = $(P1)/minic.tab.c $(P1)/lex.yy.c
P1_DRIVER      = $(P1)/driver.cpp
P1_AST_SRC     = $(P1_AST)/ast.c

# ---- Build ----
all: $(TARGET)

$(TARGET): $(P2_SRCS) $(P1_PARSER_SRCS) $(P1_AST_SRC)
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS) $(LLVM_C_INCLUDES) \
	-I$(P1) -I$(P1_AST) \
	-o $@ $^ \
	$(LLVM_LDFLAGS) $(LLVM_LIBS) -lfl

# ---- Output directory ----
$(OUT_DIR):
	mkdir -p $(OUT_DIR)

# ---- Generate LLVM IR ----
ll-%: $(TARGET) | $(OUT_DIR)
	./$(TARGET) $(TEST_DIR)/$*.c > $(OUT_DIR)/$*.ll

# ---- Compile IR with test harness and run ----
# clang is required to compile LLVM IR (.ll) files
run-%: ll-% $(TEST_DIR)/main.c
	clang $(CFLAGS) $(TEST_DIR)/main.c $(OUT_DIR)/$*.ll -o $(OUT_DIR)/run_$*
	./$(OUT_DIR)/run_$*

# ---- Clean ----
clean:
	rm -f $(TARGET) *.o
	rm -rf $(OUT_DIR)
