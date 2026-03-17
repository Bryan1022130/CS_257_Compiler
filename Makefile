# Top-level Makefile for CS 257 Compiler
#
# Builds all three parts of the compiler pipeline:
#   Part 1: minic_parser  (lexer + parser + semantic analysis)
#   Part 2: ir_builder    (LLVM IR code generator)
#   Part 3: local_optimizer / global_optimizer (optimization passes)

.PHONY: all part1 part2 part3 clean clean-part1 clean-part2 clean-part3

all: part1 part2 part3

# ---- Part 1: Frontend (Flex/Bison parser + semantic analysis) ----
part1:
	$(MAKE) -C Part1/minic_frontend

# ---- Part 2: IR Builder (LLVM IR code generator) ----
# Part 2 depends on Part 1 generated files (minic.tab.c, lex.yy.c)
part2: part1
	$(MAKE) -C Part2

# ---- Part 3: Optimizer (local + global passes) ----
part3:
	$(MAKE) -C Part3/optimizer

# ---- Clean targets ----
clean: clean-part1 clean-part2 clean-part3

clean-part1:
	$(MAKE) -C Part1/minic_frontend clean

clean-part2:
	$(MAKE) -C Part2 clean

clean-part3:
	$(MAKE) -C Part3/optimizer clean
