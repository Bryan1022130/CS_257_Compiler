# CS 257 Compiler – miniC Compiler Pipeline

## Overview

This project implements a compiler for **miniC**, a small subset of the C programming language. The compiler is built in three parts, each adding a new phase to the pipeline:

| Part   | Phase                        | Input            | Output                  |
|--------|------------------------------|------------------|-------------------------|
| Part 1 | Frontend (Lex, Parse, Semantic Analysis) | miniC source (`.c`) | AST root node |
| Part 2 | LLVM IR Builder              | AST root node    | LLVM IR module (`.ll`)  |
| Part 3 | Optimizer                    | LLVM IR (`.ll`)  | Optimized LLVM IR (`.ll`) |

---

## Directory Structure

```
CS_257_Compiler/
├── Makefile                    # Top-level build: builds all three parts
├── Part1/
│   ├── ast/                    # AST node definitions and API
│   ├── minic_frontend/         # Flex lexer, Bison parser, semantic analysis
│   ├── parser_tests/           # Syntax test inputs
│   └── semantic_analysis_tests/# Semantic test inputs
├── Part2/
│   ├── ir_builder.cpp          # Variable renamer + LLVM IR generator
│   ├── main.cpp                # Driver: parse → build IR → print IR
│   ├── Makefile
│   └── builder_tests/          # Test miniC programs + test harness
└── Part3/
    ├── optimizer/
    │   ├── local_optimizer.cpp     # Local passes: CF, CSE, DCE
    │   ├── global_optimizer.cpp    # Local passes + Constant Propagation + Live Variable Analysis
    │   └── Makefile
    ├── opt_tests/              # Test LLVM IR inputs
    └── optimizer_test_results/ # Gold standard expected outputs
```

---

## Building

### Build all parts at once

```bash
make
```

This builds, in order:
1. `Part1/minic_frontend/minic_parser`
2. `Part2/ir_builder`
3. `Part3/optimizer/local_optimizer` and `Part3/optimizer/global_optimizer`

### Build individual parts

```bash
make part1    # Frontend only
make part2    # IR builder (also builds Part 1 if needed)
make part3    # Optimizer only
```

### Clean

```bash
make clean          # Clean all parts
make clean-part1    # Clean Part 1 only
make clean-part2    # Clean Part 2 only
make clean-part3    # Clean Part 3 only
```

---

## Pipeline

### End-to-end: miniC source → optimized LLVM IR

```bash
# Step 1: Parse and check semantics, build AST (Part 1)
Part1/minic_frontend/minic_parser <input.c>

# Step 2: Generate LLVM IR from AST (Part 2)
Part2/ir_builder <input.c> > out.ll

# Step 3: Optimize the IR (Part 3)
Part3/optimizer/global_optimizer out.ll > out_opt.ll

# Step 4: Compile and run the optimized IR with a test harness
clang out_opt.ll Part2/builder_tests/main.c -o run_test
./run_test
```

### Shortcut: generate and run a Part 2 test directly

```bash
cd Part2
make run-p1    # builds IR for builder_tests/p1.c, links with main.c, runs it
```

---

## Part 1 – Frontend

**Binaries:** `Part1/minic_frontend/minic_parser`

Implements three compiler frontend phases:

1. **Lexical Analysis** (`minic.l`) — Flex tokenizer for miniC keywords, operators, identifiers, and literals
2. **Syntax Analysis** (`minic.y`) — Bison LALR(1) parser that builds an Abstract Syntax Tree using `create*` functions from `ast/ast.h`
3. **Semantic Analysis** (`semantic.cpp`) — DFS tree traversal enforcing two rules:
   - Every variable must be declared before use
   - No duplicate declarations within the same scope

Scope is tracked with a stack of `unordered_set<string>` symbol tables.

See [Part1/README.md](Part1/README.md) for full details.

---

## Part 2 – LLVM IR Builder

**Binary:** `Part2/ir_builder`

Converts the AST from Part 1 into LLVM IR using the LLVM C API. The builder has two phases:

1. **Variable Renaming** (`RenameCtx`) — Renames all variables to unique names (e.g., `a` → `a$1`, `a$2`) to handle nested scope shadowing before alloca generation
2. **IR Generation** — Traverses the AST and emits LLVM instructions:
   - All allocas are emitted together at the top of the `entry` block
   - A shared `return` basic block is created upfront for all exit paths
   - `genIRStmt` handles statements, returning the exit basic block for chaining
   - `genIRExpr` handles expressions, returning an `LLVMValueRef`
   - Unreachable basic blocks are pruned via BFS after generation

Supported control flow: assignment, if/else, while, return, print/read calls.

See [Part2/README.md](Part2/README.md) for full details.

---

## Part 3 – Optimizer

**Binaries:** `Part3/optimizer/local_optimizer`, `Part3/optimizer/global_optimizer`

| Binary             | Passes                                                                         |
|--------------------|--------------------------------------------------------------------------------|
| `local_optimizer`  | Constant Folding, CSE, Dead Code Elimination                                   |
| `global_optimizer` | All local passes + Constant Propagation (reaching defs) + Live Variable Analysis / Dead Store Elimination |

Both binaries implement **identical** local optimization logic. `global_optimizer` adds two global dataflow analyses on top.

### Local Passes (both binaries, applied in a fixpoint loop)

- **Constant Folding** — Folds `add`/`sub`/`mul` of two constants at compile time
- **CSE** — Eliminates duplicate computations within a basic block; load CSE is invalidated by intervening stores
- **DCE** — Removes unused instructions (preserving `store`, `call`, `alloca`, terminators)

### Global Passes (`global_optimizer` only)

- **Constant Propagation** — Forward reaching definitions analysis (GEN/KILL/IN/OUT over store instructions); replaces loads with constants when all reaching stores write the same constant
- **Live Variable Analysis / Dead Store Elimination** — Backward dataflow analysis (DEF/USE/LIVE\_IN/LIVE\_OUT); removes stores to addresses that are never subsequently read

The global optimizer runs `< constant propagation → constant folding → CSE → DCE >` in an outer fixpoint loop, since propagation can expose new folding opportunities.

See [Part3/README.md](Part3/README.md) for full details including GEN/KILL/IN/OUT pseudocode.

---

## miniC Language Summary

miniC is a restricted subset of C:

```c
extern void print(int);    // required extern declarations
extern int read();

int func(int p) {          // single function, optional parameter
    int a;                 // int only
    a = p + read();        // arithmetic: + - * /
    if (a > 0) {           // comparisons: < > <= >= == !=
        print(a);
    } else {
        while (a < 10)
            a = a + 1;
    }
    return a;
}
```

**Supported:** `int` type, single function, if/else, while, return, print/read calls, nested block scopes
**Not supported:** multiple functions, arrays, pointers, structs, float

---

## Dependencies

| Tool          | Version | Used by         |
|---------------|---------|-----------------|
| `g++`         | C++17   | All parts       |
| `flex`        | any     | Part 1          |
| `bison`       | any     | Part 1          |
| `llvm-config` | 17      | Parts 2 and 3   |
| `clang`       | any     | Running IR tests|
