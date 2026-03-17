# CS 257 Compiler – Part 3: Optimizer

## Overview

This part implements the **optimizer** phase of the compiler. It takes LLVM IR as input (produced by Part 2 or by running `clang -S -emit-llvm` on a miniC program) and applies a series of optimization passes to produce more efficient LLVM IR.

**Input:** LLVM IR file (`.ll`)
**Output:** Optimized LLVM IR printed to stdout

Two optimizer binaries are provided:

| Binary             | Passes Included                                               |
|--------------------|---------------------------------------------------------------|
| `local_optimizer`  | Constant Folding, CSE, Dead Code Elimination                  |
| `global_optimizer` | All local passes + Constant Propagation + Live Variable Analysis / Dead Store Elimination |

---

## Directory Structure

```
Part3/
├── optimizer/
│   ├── local_optimizer.cpp     # Local optimization passes (CF, CSE, DCE)
│   ├── global_optimizer.cpp    # Local passes + global constant propagation + live variable analysis
│   ├── Core.h                  # LLVM C API header
│   └── Makefile
├── opt_tests/                  # Test input files (LLVM IR + C source)
│   ├── cfold_add.ll            # Constant folding: addition
│   ├── cfold_mul.ll            # Constant folding: multiplication
│   ├── cfold_sub.ll            # Constant folding: subtraction
│   ├── p2_common_subexpr.ll    # Common subexpression elimination
│   ├── p3_const_prop.ll        # Global constant propagation (simple)
│   ├── p4_const_prop.ll        # Global constant propagation (loop)
│   ├── *.c                     # Corresponding C source files
│   └── main.c                  # Test harness (print/read implementations)
├── optimizer_test_results/     # Gold standard expected outputs
│   ├── *_opt.ll                # Expected optimized IR for each test
│   └── README                  # Test documentation
└── out/                        # Generated test outputs and diffs
```

---

## Building

```bash
cd optimizer
make
```

Produces two binaries: `local_optimizer` and `global_optimizer`.

### Usage

```bash
./local_optimizer  <input.ll>    # Run local passes only
./global_optimizer <input.ll>    # Run all passes including global
```

### Generate LLVM IR from C source

```bash
clang -S -emit-llvm <file>.c -o <file>.ll
```

### Compile optimized IR and run with test harness

```bash
clang <optimized.ll> opt_tests/main.c -o run_test
./run_test
```

### Clean

```bash
make clean
```

---

## Local Optimizations

All three local passes run on individual basic blocks and are applied in a **fixpoint loop**: the full set of passes repeats until no further changes occur.

### 1. Constant Folding

Finds arithmetic instructions (`add`, `sub`, `mul`) where **both operands are compile-time constants** and replaces them with the pre-computed constant result. The original instruction is left for DCE to remove.

**GEN / KILL for analysis:**
- GEN: any `add`/`sub`/`mul` instruction whose both operands are `LLVMConstInt` values
- KILL: the original instruction after its uses are replaced by the folded constant

**Example:**
```llvm
; Before
%9 = add nsw i32 10, 20

; After constant folding
; all uses of %9 now point to i32 30
; %9 itself is left for DCE
```

**LLVM API:** `LLVMConstAdd`, `LLVMConstSub`, `LLVMConstMul`, `LLVMReplaceAllUsesWith`

---

### 2. Common Subexpression Elimination (CSE)

Within each basic block, identifies pairs of instructions A (earlier) and B (later) that have the **same opcode and same operands**. Replaces all uses of B with A (B is left for DCE).

**GEN / KILL for analysis:**
- GEN: the first occurrence of each unique `(opcode, operands)` pair seen in the basic block
- KILL: any subsequent instruction with the same `(opcode, operands)` pair — its uses are redirected to the GEN instruction

**Safety rule for load instructions:** Because a store to the same address between A and B would make the replacement unsafe, loads are only CSE'd if no intervening store writes to the same pointer. A seen-loads cache (`seenLoadByPtr`) is invalidated whenever a store to that address is encountered.

**Instructions excluded from CSE:** `call`, `store`, `alloca`, terminators (these have side effects or always produce distinct results).

**Example:**
```llvm
; Before
%8  = add nsw i32 %6, %7    ; A: first occurrence
%11 = add nsw i32 %9, %10   ; B: same opcode and operands as A

; After CSE
; all uses of %11 redirected to %8
```

**LLVM API:** `LLVMReplaceAllUsesWith`

---

### 3. Dead Code Elimination (DCE)

Iteratively removes instructions that have **no uses** and no observable side effects. Runs to fixpoint because deleting one instruction can expose new dead instructions.

**Instructions that are NEVER deleted (side effects / required):**
- `store` — writes to memory
- `call` / `invoke` — external side effects
- `alloca` — stack allocation
- All terminator instructions (`br`, `ret`, etc.)

**Example:**
```llvm
; After CSE, %11 has no uses:
%11 = add nsw i32 %9, %10   ; dead — removed by DCE
```

**LLVM API:** `LLVMInstructionEraseFromParent`

---

## Global Optimization

### 4. Constant Propagation (Reaching Definitions)

Propagates constants stored to memory locations through subsequent load instructions. Uses **reaching definitions dataflow analysis** (forward analysis) over the control flow graph.

#### Data Structures

- `StoreSet` — `unordered_set<LLVMValueRef>` of store instructions
- `var_map` — maps address key → set of reaching store instructions
- GEN[B], KILL[B], IN[B], OUT[B] — sets of `LLVMValueRef` (store instructions) per basic block

#### Computing GEN[B]

```
GEN[B] = {}
for each instruction I in B (in order):
    if I is a store:
        add I to GEN[B]
        remove from GEN[B] any earlier store to the same address as I
        (a later store to the same address kills the earlier one locally)
```

#### Computing KILL[B]

```
S = set of ALL store instructions in the function

KILL[B] = {}
for each instruction I in B:
    if I is a store:
        for each store J in S where J != I and J writes to the same address as I:
            add J to KILL[B]
```

#### Computing IN[B] and OUT[B] (Iterative Dataflow)

```
for each basic block B:
    IN[B]  = {}
    OUT[B] = GEN[B]

change = true
while change:
    change = false
    for each basic block B:
        IN[B]  = union( OUT[P] for all predecessors P of B )
        oldOUT = OUT[B]
        OUT[B] = GEN[B] ∪ (IN[B] − KILL[B])
        if OUT[B] ≠ oldOUT:
            change = true
```

#### Applying Constant Propagation

```
for each basic block B:
    R = IN[B]        // running set of reaching stores
    for each instruction I in B:
        if I is a store:
            add I to R
            remove from R all stores to the same address killed by I
        if I is a load from address %t:
            reaching = { s ∈ R | s writes to address %t }
            if every store in reaching is a constant store and all store the SAME constant:
                replace all uses of I (load) with that constant
                mark I for deletion
delete all marked load instructions
```

**Outer fixpoint loop:** After each round of constant propagation, run constant folding → CSE → DCE. Repeat until no changes occur (propagation can expose new constant-folding opportunities).

---

### 5. Live Variable Analysis & Dead Store Elimination

Identifies **store instructions whose value is never subsequently read** and removes them. Uses **live variable dataflow analysis** (backward analysis).

#### Definitions

- A variable (memory address) is **live** at a program point if there exists a path from that point to a load from that address before any intervening store to it.
- A store to address `%t` is **dead** at that point if `%t` is not live after the store.

#### Computing DEF[B] and USE[B] per Basic Block

```
DEF[B] = set of addresses written (stored to) in B before being read
USE[B] = set of addresses read (loaded from) in B before being written
```

Computed by scanning instructions in forward order:
- Load from `%t`: if `%t` not in DEF[B], add to USE[B]
- Store to `%t`: if `%t` not in USE[B], add to DEF[B]

#### Computing LIVE\_IN[B] and LIVE\_OUT[B] (Iterative Dataflow)

```
for each basic block B:
    LIVE_IN[B]  = {}
    LIVE_OUT[B] = {}

change = true
while change:
    change = false
    for each basic block B (in reverse postorder):
        LIVE_OUT[B] = union( LIVE_IN[S] for all successors S of B )
        oldLIVE_IN  = LIVE_IN[B]
        LIVE_IN[B]  = USE[B] ∪ (LIVE_OUT[B] − DEF[B])
        if LIVE_IN[B] ≠ oldLIVE_IN:
            change = true
```

#### Removing Dead Stores

```
for each basic block B:
    live = LIVE_OUT[B]    // running live set, walk backward
    for each instruction I in B (in reverse order):
        if I is a store to address %t:
            if %t is NOT in live:
                mark I for deletion   // dead store
            remove %t from live       // store kills liveness
        if I is a load from address %t:
            add %t to live            // load makes address live
delete all marked store instructions
```

---

## Test Cases

### Local Optimization Tests (`opt_tests/`)

| File                    | Optimization Exercised | Expected Result                              |
|-------------------------|------------------------|----------------------------------------------|
| `cfold_add.ll`          | Constant Folding       | `add i32 10, 20` folded to constant `30`     |
| `cfold_mul.ll`          | Constant Folding       | Multiplication of constants folded           |
| `cfold_sub.ll`          | Constant Folding       | Subtraction of constants folded              |
| `p2_common_subexpr.ll`  | CSE                    | Duplicate `add` computation eliminated       |

### Global Optimization Tests

| File                 | Optimization Exercised            | Expected Result                              |
|----------------------|-----------------------------------|----------------------------------------------|
| `p3_const_prop.ll`   | Constant Propagation (simple)     | Function simplified to `ret i32 40`          |
| `p4_const_prop.ll`   | Constant Propagation (loop)       | Constant `20` propagated through loop        |

### Example: Constant Propagation (`p3_const_prop.c`)

```c
int func(int p) {
    int a, b, c1;
    a = 10;  b = 20;
    c1 = a + b;          // c1 = 30
    if (a < p) b = 30;
    else       b = c1;   // b = 30 in both branches
    return (b + a);      // always 30 + 10 = 40
}
```

After all passes: function body reduces to `ret i32 40`.

---

## Gold Standard Expected Outputs (`optimizer_test_results/`)

Each test has a corresponding `*_opt.ll` file showing the expected optimized IR. Files without the `_opt` suffix show the unoptimized IR for reference.

| Gold File                       | Passes Applied                    |
|---------------------------------|-----------------------------------|
| `cfold_add_opt.ll`              | CF, DCE                           |
| `cfold_mul_opt.ll`              | CF, DCE                           |
| `cfold_sub_opt.ll`              | CF, DCE                           |
| `p2_common_subexpr_opt.ll`      | CSE, DCE                          |
| `p3_const_prop_opt.ll`          | Const Prop, CF, CSE, DCE          |
| `p4_const_prop_opt.ll`          | Const Prop, CF, CSE, DCE          |
| `p5_const_prop_opt.ll`          | Const Prop, CF, CSE, DCE          |

---

## Implementation Notes

- All local passes operate on a single basic block at a time; no knowledge of the CFG is needed.
- The fixpoint loop for local optimizations ensures that, for example, CSE followed by DCE can then expose new constant-folding opportunities, which are caught on the next iteration.
- Constant propagation operates at the **memory level** (store/load pairs), not at the SSA def-use level. This is necessary because miniC programs access variables through memory (alloca/store/load), not directly through SSA values.
- The global optimizer's outer fixpoint loop interleaves constant propagation with local passes: propagating a constant can produce new foldable expressions, which when folded can propagate further constants.
- Dead store elimination is a backward dataflow analysis, in contrast to the forward analysis used for constant propagation.

