# CS 257 Compiler – Part 1: Frontend (Lexical Analysis, Parsing & Semantic Analysis)

## Overview

This part implements the **frontend** of a compiler for **miniC**, a small subset of the C programming language. The frontend covers three phases:

1. **Lexical Analysis** – tokenizing source input using Flex (`minic.l`)
2. **Syntax Analysis** – parsing tokens and building an Abstract Syntax Tree (AST) using Bison (`minic.y`)
3. **Semantic Analysis** – validating variable declarations and scoping rules (`semantic.cpp`)

**Input:** A miniC source file
**Output:** A pointer to the root node of the AST (syntax phase), plus an integer error code (semantic phase)

---

## Directory Structure

```
Part1/
├── ast/
│   ├── ast.h               # AST node type definitions and API declarations
│   ├── ast.c               # AST node creation, printing, and freeing
│   └── example.c           # Example of building an AST programmatically
├── minic_frontend/
│   ├── minic.l             # Flex lexer specification
│   ├── minic.y             # Bison parser + AST construction
│   ├── semantic.h          # Semantic analysis interface
│   ├── semantic.cpp        # Semantic analysis implementation
│   ├── main.cpp            # Main driver (parse → print AST → semantic check)
│   └── Makefile            # Build system
├── parser_tests/           # Test inputs for parser validation
│   ├── p1.c – p5.c         # Valid miniC programs
│   └── p_bad.c             # Syntactically invalid program
├── semantic_analysis_tests/
│   ├── p1_good.c – p3_good.c   # Semantically valid programs
│   ├── p1_bad.c – p4_bad.c     # Programs with semantic errors
│   └── README                  # Description of expected test outcomes
└── README.md
```

---

## miniC Language

miniC is a restricted subset of C with the following rules:

- **Single data type:** `int`
- **Program structure:** Two required `extern` declarations (`print` and `read`) followed by exactly one function definition
- **Function definition:** One function with an optional single `int` parameter
- **Statements:** variable declaration, assignment, `if`/`else`, `while`, `return`, function calls (`print`, `read`)
- **Expressions:** arithmetic (`+`, `-`, `*`, `/`), unary minus (`-`), comparisons (`<`, `>`, `<=`, `>=`, `==`, `!=`)
- **Scoping:** Function scope plus arbitrarily nested block scopes (`{...}`)

### Example Program

```c
extern void print(int);
extern int read();

int main(int x) {
    int a;
    a = x + read();
    if (a > 10) {
        print(a);
    }
    return a;
}
```

---

## Frontend Components

### 1. Lexer – `minic.l`

Implemented with **Flex**. Recognizes the following token classes:

| Category    | Tokens                                              |
|-------------|-----------------------------------------------------|
| Keywords    | `int`, `void`, `if`, `else`, `while`, `return`, `extern`, `print`, `read` |
| Operators   | `+`, `-`, `*`, `/`, `=`, `==`, `!=`, `<`, `>`, `<=`, `>=` |
| Delimiters  | `(`, `)`, `{`, `}`, `;`, `,`                        |
| Literals    | Integer constants: `[0-9]+`                         |
| Identifiers | `[A-Za-z_][A-Za-z0-9_]*`                           |
| Comments    | Single-line `// ...` (consumed, not tokenized)      |
| Whitespace  | Spaces, tabs, newlines (consumed)                   |

---

### 2. Parser – `minic.y`

Implemented with **Bison** (LALR(1)). Constructs the AST during parsing using the `create*` functions defined in `ast.h`.

#### Grammar Summary

```
program     → externs funcdef
externs     → extern_decl extern_decl
extern_decl → EXTERN VOID PRINT ( INT ) ;
            | EXTERN INT READ  ( ) ;
funcdef     → INT ID ( param_opt ) block
param_opt   → ε | INT ID
block       → { stmt_list }
stmt_list   → ε | stmt_list stmt
stmt        → decl_stmt | asgn_stmt | if_stmt | while_stmt
            | ret_stmt | call_stmt | block
decl_stmt   → INT ID ;
asgn_stmt   → ID = aexpr ;
if_stmt     → IF ( rexpr ) stmt
            | IF ( rexpr ) stmt ELSE stmt
while_stmt  → WHILE ( rexpr ) stmt
ret_stmt    → RETURN aexpr ;
call_stmt   → PRINT ( aexpr ) ; | READ ( ) ;
rexpr       → aexpr RELOP aexpr
aexpr       → aexpr + term | aexpr - term | term
term        → term * factor | term / factor | factor
factor      → - factor | ( aexpr ) | ID | NUM | call_expr
```

Union types used in the grammar:
- `ival` – integer values
- `sval` – identifier/string values
- `node` – `astNode*` (single AST node)
- `vec`  – `std::vector<astNode*>*` (list of nodes, used for statement lists)

---

### 3. AST – `ast/ast.h` and `ast/ast.c`

The AST uses a single tagged union `astNode` with a `type` field to distinguish node kinds.

#### Node Types

| Node Type     | Description                                      |
|---------------|--------------------------------------------------|
| `ast_prog`    | Root node: externs + function definition         |
| `ast_extern`  | Extern declaration                               |
| `ast_func`    | Function definition (name, param, body)          |
| `ast_var`     | Variable reference (identifier)                  |
| `ast_cnst`    | Integer constant                                 |
| `ast_bexpr`   | Binary arithmetic expression                     |
| `ast_uexpr`   | Unary expression (negation)                      |
| `ast_rexpr`   | Relational (comparison) expression               |
| `ast_call`    | Function call (`print` or `read`)                |
| `ast_stmt`    | Statement node (tagged union of statement types) |

#### Statement Types (within `ast_stmt`)

| Statement Type | Description              |
|----------------|--------------------------|
| `ast_decl`     | Variable declaration     |
| `ast_asgn`     | Assignment statement     |
| `ast_if`       | If / if-else statement   |
| `ast_while`    | While loop               |
| `ast_ret`      | Return statement         |
| `ast_block`    | Block of statements      |

#### Key API

```c
astNode* createProg(astNode* externs, astNode* func);
astNode* createFunc(char* name, char* param, astNode* body);
astNode* createExtern(char* name);
astNode* createVar(char* name);
astNode* createConst(int val);
astNode* createBExpr(astNode* lhs, astNode* rhs, op_type op);
astNode* createUExpr(astNode* expr, op_type op);
astNode* createRExpr(astNode* lhs, astNode* rhs, op_type op);
astNode* createCall(char* name, astNode* arg);
astNode* createDecl(char* name);
astNode* createAsgn(char* name, astNode* expr);
astNode* createIf(astNode* cond, astNode* then_s, astNode* else_s);
astNode* createWhile(astNode* cond, astNode* body);
astNode* createRet(astNode* expr);
astNode* createBlock(vector<astNode*>* stmts);
void     printNode(astNode* node, int indent);
void     freeNode(astNode* node);
```

---

### 4. Semantic Analysis – `semantic.cpp`

Performs two checks:

1. **No undeclared variable use** – every variable must be declared before it is referenced
2. **No duplicate declarations in the same scope** – each variable name may appear at most once per scope

#### Algorithm

A **pre-order (DFS) tree traversal** is used to process nodes in program order. Scope is managed with a **stack of symbol tables**:

```
stack: [ function-scope | block-scope | nested-block-scope | ... ]
```

Each symbol table is an `unordered_set<string>` of declared names at that scope level.

| Event                   | Action                                             |
|-------------------------|----------------------------------------------------|
| Enter function          | Push new scope; register parameter if present      |
| Enter block (non-root)  | Push new scope                                     |
| Exit function / block   | Pop scope                                          |
| Declaration statement   | Check for duplicate in current scope; insert name  |
| Variable use            | Check that name exists in any enclosing scope      |

**Note:** The function's own block does not push an additional scope—it shares the function's scope so that parameters and top-level locals are in the same namespace.

#### Interface

```cpp
// semantic.h
int semantic_check(astNode* root);
// Returns 0 if no errors; non-zero count of errors otherwise.
```

---

## Building and Running

```bash
cd minic_frontend
make
```

This runs Flex and Bison to generate the lexer/parser, compiles all sources with `g++ -std=c++17`, and links the `minic_parser` binary.

### Usage

```bash
./minic_parser <input_file.c>
```

The program will:
1. Lex and parse the input, building the AST
2. Print the AST to stdout
3. Run semantic analysis and report any errors

### Clean

```bash
make clean
```

---

## Testing

### Parser Tests (`parser_tests/`)

| File       | Description                          |
|------------|--------------------------------------|
| `p1.c`     | Simple function, no parameters       |
| `p2.c`     | Function with parameter              |
| `p3.c`     | If/else statements                   |
| `p4.c`     | While loop                           |
| `p5.c`     | Nested blocks and multiple stmts     |
| `p_bad.c`  | Syntactically invalid – parse error  |

### Semantic Tests (`semantic_analysis_tests/`)

| File         | Expected Result | Violation                                           |
|--------------|-----------------|-----------------------------------------------------|
| `p1_good.c`  | Pass            | –                                                   |
| `p2_good.c`  | Pass            | –                                                   |
| `p3_good.c`  | Pass            | –                                                   |
| `p1_bad.c`   | Error           | Duplicate declaration (param + local same name)     |
| `p2_bad.c`   | Error           | Use of undeclared variable                          |
| `p3_bad.c`   | Error           | Variable used outside its declared scope            |
| `p4_bad.c`   | Error           | Variable used in nested scope before outer declaration |

---

## Implementation Notes

- The parser uses Bison's `%union` directive and typed `%type` rules to pass AST node pointers and statement vectors up the grammar.
- Statement lists are accumulated into a `std::vector<astNode*>` and passed to `createBlock()`.
- The semantic analyzer returns the **count of errors** found to allow full error reporting in a single pass.

