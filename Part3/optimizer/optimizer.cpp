// optimizer.cpp
//
//
// 1) Constant Folding
//    - Ifsee add/sub/mul where BOTH operands are constants,
//      compute the result now (compile-time) and replace the instruction’s uses
//      with that constant value.
//
// 2) Common Subexpression Elimination (CSE)
//    - Within EACH basic block, iffind two instructions A then B
//      with the same opcode and operands,replace ALL uses of B with A.
//    -do NOT delete B here; Dead Code Elimination will remove it later.
//    - Special rule for LOAD: only safe if no STORE to the same pointer occurred
//      between A and B.
//
// 3) Dead Code Elimination (DCE)
//    - Delete instructions with no uses (LLVMGetFirstUse(inst) == NULL)
//      BUT do NOT delete side-effect instructions (store/call/terminator/alloca).
//
// The whole point: LLVM IR is SSA (Single Static Assignment), so non-load
// computations are safe to CSE inside a basic block for miniC IR.
//
// Build/run:
//   make
//   ./optimizer ../opt_tests/cfold_add.ll > /tmp/out.ll
//   diff -u ../optimizer_test_results/cfold_add_opt.ll /tmp/out.ll
//
// NOTE:also normalize ModuleID so it matches the gold files’ style.

#include <llvm-c/Core.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Support.h>

#include <cstdio>      // printf, fprintf
#include <cstdlib>     // exit codes
#include <cstring>     // strlen
#include <string>      // std::string
#include <unordered_map> // hash maps for CSE

// Helpers

// Check if a value is a constant integer (used by constant folding).
static bool isConstInt(LLVMValueRef V) {
  return LLVMIsAConstantInt(V) != nullptr;
}

// Check if an instruction is a terminator (br, ret, etc).
// Terminators must remain in a basic block; removing them breaks control flow.
static bool isTerminatorInst(LLVMValueRef I) {
  return LLVMIsATerminatorInst(I) != nullptr;
}

// Check if an instruction is "must keep" for DCE.
// These are instructions with side effects or special structural roles.
static bool isSideEffectOrMustKeep(LLVMValueRef I) {
  LLVMOpcode op = LLVMGetInstructionOpcode(I);           // what kind of instruction is it?

  // Store changes memory → side effect → cannot delete.
  if (op == LLVMStore) return true;

  // Call may do I/O (like read/print) or change memory → assume side effects.
  if (op == LLVMCall) return true;

  // Invoke is a call+exception control flow → must keep.
  if (op == LLVMInvoke) return true;

  // Do NOT eliminate allocas.
  if (op == LLVMAlloca) return true;

  // Terminators (br/ret/...) are required to end each basic block.
  if (isTerminatorInst(I)) return true;

  // Otherwise, it's safe to delete IF it has no uses.
  return false;
}

// Check if an instruction's result is unused (no one reads its value).
// DCE deletes these (unless side-effect/must-keep).
static bool hasNoUses(LLVMValueRef I) {
  return LLVMGetFirstUse(I) == nullptr;
}

// Convert an LLVM value to a stable-ish string socan make CSE “keys”.
//must free the message using LLVMDisposeMessage.
static std::string valToStr(LLVMValueRef V) {
  char* s = LLVMPrintValueToString(V);                   // get a printable representation
  std::string out = s ? s : "";                          // copy into std::string
  LLVMDisposeMessage(s);                                 // free LLVM-allocated string
  return out;
}

// Convert an LLVM type to a string by printing a temporary null constant
// of that type and reusing `valToStr` to get a stable representation.
static std::string typeToStr(LLVMTypeRef T) {
  LLVMValueRef tmp = LLVMConstNull(T);
  return valToStr(tmp);
}

//want the ModuleID header to match the gold file style.
// Gold uses:  ; ModuleID = 'opt_tests/<basename>.ll'
static std::string basenameOnly(const char* path) {
  std::string s(path);                                   // copy input path
  size_t slash = s.find_last_of("/\\");                  // find last path separator
  if (slash != std::string::npos) {                      // iffound one
    s = s.substr(slash + 1);                             // keep only after the slash
  }
  return s;                                              // return basename
}

// ----------------------------
// Local Optimization #1: Constant Folding
// ----------------------------
//
//scan instructions and look for arithmetic ops: add/sub/mul
// where BOTH operands are constants.
//
// If found:
//   folded = LLVMConstAdd/Sub/Mul(op0, op1)
//   LLVMReplaceAllUsesWith(inst, folded)
//
//do NOT erase inst here; DCE will remove it after its uses are replaced.
//
static bool constantFoldInFunction(LLVMValueRef F) {
  bool changed = false;                                  // track ifmade any edits

  // Loop over basic blocks in the function.
  for (LLVMBasicBlockRef BB = LLVMGetFirstBasicBlock(F);
       BB;
       BB = LLVMGetNextBasicBlock(BB)) {

    // Loop over instructions in the basic block.
    for (LLVMValueRef I = LLVMGetFirstInstruction(BB);
         I; ) {

      LLVMValueRef Next = LLVMGetNextInstruction(I);      // save next now (safe ifmodify I)

      LLVMOpcode op = LLVMGetInstructionOpcode(I);        // what instruction is this?

      // Only fold arithmetic operations 
      if (op == LLVMAdd || op == LLVMSub || op == LLVMMul) {

        // Binary ops should have at least 2 operands.
        if (LLVMGetNumOperands(I) >= 2) {

          LLVMValueRef a = LLVMGetOperand(I, 0);          // operand 0
          LLVMValueRef b = LLVMGetOperand(I, 1);          // operand 1

          // Constant folding only when BOTH operands are constant ints.
          if (isConstInt(a) && isConstInt(b)) {

            LLVMValueRef folded = nullptr;                // will hold the computed constant

            // Compute constant result using LLVM’s constant operators.
            if (op == LLVMAdd) folded = LLVMConstAdd(a, b);
            if (op == LLVMSub) folded = LLVMConstSub(a, b);
            if (op == LLVMMul) folded = LLVMConstMul(a, b);

            // Ifsuccessfully built a folded constant, replace all uses.
            if (folded) {
              LLVMReplaceAllUsesWith(I, folded);          // redirect all users of I to folded
              changed = true;                             // record thatchanged the IR
            }
          }
        }
      }

      I = Next;                                           // move to next instruction
    }
  }

  return changed;                                         // diddo any folding?
}

// Local Optimization #2: Common Subexpression Elimination (CSE)
//
// Within each basic block:
//
//look for instructions A and later B such that:
//   - same opcode
//   - same operands (same LLVMValueRef operands)
//   - (andskip certain opcodes: call/store/terminator/alloca)
//
// If found:
//   LLVMReplaceAllUsesWith(B, A)
//
// NOT deleteing B here (DCE will remove it later if it becomes unused).
//
// IMPORTANT special case: LOAD
//   Two loads from the same pointer are only safe to CSE if NO store to the same
//   pointer happened between them.enforce this with a simple invalidation rule:
//
//   - Track "most recent load from ptr P" in seenLoadByPtr[P]
//   - Whensee "store ..., ptr P",erase seenLoadByPtr[P] (invalidate).
//   - Whensee "load ptr P":
//       if seenLoadByPtr[P] exists, replace uses of this load with the earlier load.
//       otherwise remember this load as the most recent load from P.
//
static bool skipCSEForOpcode(LLVMOpcode op, LLVMValueRef I) {
  // not eliminating call, store, terminator, alloc instructions.
  if (op == LLVMAlloca) return true;
  if (op == LLVMStore)  return true;
  if (op == LLVMCall || op == LLVMInvoke) return true;
  if (isTerminatorInst(I)) return true;
  return false;
}

// Create a “key” for non-load instructions based on opcode + operands + result type.
static std::string makeCSEKey(LLVMValueRef I) {
  LLVMOpcode op = LLVMGetInstructionOpcode(I);            // instruction opcode
  std::string key = std::to_string((int)op);              // start key with opcode integer

  // Add each operand’s printed representation to the key.
  unsigned nOps = LLVMGetNumOperands(I);
  for (unsigned i = 0; i < nOps; i++) {
    key += "|";
    key += valToStr(LLVMGetOperand(I, i));
  }

  // Add result type to reduce accidental collisions.
  key += "|T:";
  key += typeToStr(LLVMTypeOf(I));

  return key;
}

static bool cseInFunction(LLVMValueRef F) {
  bool changed = false;                                  // track ifrewired any uses

  // For each basic block,do a local scan from top to bottom.
  for (LLVMBasicBlockRef BB = LLVMGetFirstBasicBlock(F);
       BB;
       BB = LLVMGetNextBasicBlock(BB)) {

    // Map: expression key -> first instruction A that computes it.
    std::unordered_map<std::string, LLVMValueRef> seenExpr;

    // Map: pointer string -> last load instruction from that pointer (for load CSE).
    std::unordered_map<std::string, LLVMValueRef> seenLoadByPtr;

    // Walk instructions in order.
    for (LLVMValueRef I = LLVMGetFirstInstruction(BB);
         I; ) {

      LLVMValueRef Next = LLVMGetNextInstruction(I);      // save next before rewriting

      LLVMOpcode op = LLVMGetInstructionOpcode(I);        // opcode of I

      // If I is store, invalidate any remembered load from that same ptr.
      // store operands in LLVM IR: store <val>, <ptr>
      if (op == LLVMStore) {
        LLVMValueRef ptr = LLVMGetOperand(I, 1);          // operand 1 is the pointer
        std::string p = valToStr(ptr);                    // stringify pointer for map key
        seenLoadByPtr.erase(p);                           // store changes memory at p, so old load isn't safe
        I = Next;
        continue;                                         // nothing else to do for store
      }

      // Skip instructions
      if (skipCSEForOpcode(op, I)) {
        I = Next;
        continue;
      }

      // Special handling for LOAD.
      // In LLVM-C: load ptr is operand 0 of the load instruction.
      if (op == LLVMLoad) {
        LLVMValueRef ptr = LLVMGetOperand(I, 0);          // load address
        std::string p = valToStr(ptr);                    // stringify pointer

        auto it = seenLoadByPtr.find(p);                  // didsee a previous load from same pointer?
        if (it != seenLoadByPtr.end()) {
          LLVMValueRef A = it->second;                    // A = earlier load
          LLVMReplaceAllUsesWith(I, A);                   // replace uses of B(I) with A
          changed = true;                                 // record change
          // DCE will remove I if unused.
        } else {
          seenLoadByPtr[p] = I;                           // first load from p since last store
        }

        I = Next;
        continue;
      }

      // Non-load instruction: SSA means operands are stable,
      // socan CSE within this basic block without extra safety checks.
      std::string key = makeCSEKey(I);                    // key = opcode+operands+type

      auto it = seenExpr.find(key);                       // check ifsaw same expression earlier
      if (it != seenExpr.end()) {
        LLVMValueRef A = it->second;                      // A = earlier instruction
        LLVMReplaceAllUsesWith(I, A);                     // replace uses of B(I) with A
        changed = true;                                   // record change
        //  DCE will handle I.
      } else {
        seenExpr[key] = I;                                // remember first timesee this expression
      }

      I = Next;                                           // move forward
    }
  }

  return changed;                                         // didperform any CSE?
}

// Local Optimization #3: Dead Code Elimination (DCE)
//
//remove instructions with no uses (dead) as long as they are not:
// - store (side effects)
// - call/invoke (assume side effects)
// - alloca 
// - terminators (control flow)
//
static bool deadCodeElimInFunction(LLVMValueRef F) {
  bool changedAny = false;                                // diddelete anything overall?

  // Run DCE to a fixpoint because removing one dead instruction can
  // make earlier instructions become dead too.
  while (true) {
    bool changedThisRound = false;                        // diddelete anything in this iteration?

    for (LLVMBasicBlockRef BB = LLVMGetFirstBasicBlock(F);
         BB;
         BB = LLVMGetNextBasicBlock(BB)) {

      for (LLVMValueRef I = LLVMGetFirstInstruction(BB);
           I; ) {

        LLVMValueRef Next = LLVMGetNextInstruction(I);    // save next before potential deletion

        // If instruction is safe to delete AND no one uses it, delete it.
        if (!isSideEffectOrMustKeep(I) && hasNoUses(I)) {
          LLVMInstructionEraseFromParent(I);              // remove instruction from IR
          changedThisRound = true;                        //made progress this round
          changedAny = true;                              // and overall
        }

        I = Next;                                         // continue scanning
      }
    }

    // Ifdidn’t delete anything this round,reached a fixpoint.
    if (!changedThisRound) break;
  }

  return changedAny;                                      // diddelete anything at all?
}

// Driver: run all passes on a module
//
//run local optimizations to a fixpoint for each function:
//
// Suggested order:
//   1) Constant folding (creates more constants)
//   2) CSE (reuses equivalent computations)
//   3) DCE (removes unused leftovers)
//
// Andrepeat until nothing changes.
//
static void optimizeModule(LLVMModuleRef M) {
  for (LLVMValueRef F = LLVMGetFirstFunction(M);
       F;
       F = LLVMGetNextFunction(F)) {

    // Skip function declarations (no basic blocks means no body).
    if (LLVMCountBasicBlocks(F) == 0) continue;

    // Run passes until stable.
    while (true) {
      bool changed = false;

      changed |= constantFoldInFunction(F);               // local optimization: constant folding
      changed |= cseInFunction(F);                        // local optimization: common subexpression elimination
      changed |= deadCodeElimInFunction(F);               // local optimization: dead code elimination

      if (!changed) break;                                // stop when no pass made changes
    }
  }
}

// Main: read IR → optimize → print IR
int main(int argc, char** argv) {

  // Expect exactly one argument: the input .ll file.
  if (argc != 2) {
    std::fprintf(stderr, "Usage: %s <input.ll>\n", argv[0]);
    return 1;
  }

  const char* inputPath = argv[1];                        // path to LLVM IR file

  // Create a context (LLVM uses a context to own types/values).
  LLVMContextRef Ctx = LLVMContextCreate();

  // I'll read the input file into an LLVM memory buffer.
  LLVMMemoryBufferRef Buf = nullptr;
  char* errMsg = nullptr;

  // Load the entire file contents into Buf.
  if (LLVMCreateMemoryBufferWithContentsOfFile(inputPath, &Buf, &errMsg) != 0) {
    std::fprintf(stderr, "Error reading file: %s\n", errMsg ? errMsg : "(unknown)");
    LLVMDisposeMessage(errMsg);                           // free LLVM error string
    LLVMContextDispose(Ctx);                              // free context
    return 1;
  }

  // Parse the memory buffer as LLVM IR, producing an LLVMModuleRef.
  LLVMModuleRef M = nullptr;
  if (LLVMParseIRInContext(Ctx, Buf, &M, &errMsg) != 0) {
    std::fprintf(stderr, "Error parsing IR: %s\n", errMsg ? errMsg : "(unknown)");
    LLVMDisposeMessage(errMsg);                           // free LLVM error string
    LLVMDisposeMemoryBuffer(Buf);                         // parse failed, sostill own Buf
    LLVMContextDispose(Ctx);                              // free context
    return 1;
  }


  std::string base = basenameOnly(inputPath);             // extract "cfold_add.ll"
  std::string id = std::string("opt_tests/") + base;      // make "opt_tests/cfold_add.ll"
  LLVMSetModuleIdentifier(M, id.c_str(), id.size());      // set ModuleID string

  // Run local optimizations (folding, CSE, DCE).
  optimizeModule(M);

  // Print optimized module to stdout as text IR.
  char* outStr = LLVMPrintModuleToString(M);              // LLVM allocates a string
  std::printf("%s", outStr);                              // print it
  LLVMDisposeMessage(outStr);                             // free the string

  // Cleanup LLVM objects.
  LLVMDisposeModule(M);                                   // free module
  LLVMContextDispose(Ctx);                                // free context

  return 0;                                                // success
}
