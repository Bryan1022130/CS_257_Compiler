#include <string>
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <cstring>   // strdup

#include "../Part1/ast/ast.h"

#include <llvm-c/Types.h>
#include <llvm-c/Core.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Support.h>
#include <llvm-c/Analysis.h>

#include <cstdio>
#include <cstdlib>


using std::string;

struct RenameCtx {
  std::vector<std::unordered_map<string,string>> scopes;
  std::unordered_map<string,int> counters;

  std::unordered_set<string> allVarsUnique; // locals + param (unique names)
  string paramUnique; // filled if param exists
};

static void pushScope(RenameCtx& ctx) {
  ctx.scopes.push_back({});
}

static void popScope(RenameCtx& ctx) {
  ctx.scopes.pop_back();
}

static bool lookupName(RenameCtx& ctx, const string& base, string& outUnique) {
  for (int i = (int)ctx.scopes.size() - 1; i >= 0; --i) {
    auto it = ctx.scopes[i].find(base);
    if (it != ctx.scopes[i].end()) {
      outUnique = it->second;
      return true;
    }
  }
  return false;
}

static string makeUnique(RenameCtx& ctx, const string& base) {
  int k = ++ctx.counters[base];
  return base + "$" + std::to_string(k);
}

static void renameDecl(RenameCtx& ctx, astNode* declNode) {
  // declNode->type should be ast_stmt with stmt.type == ast_decl
  string base = declNode->stmt.decl.name;
  string unique = makeUnique(ctx, base);

  // record in current scope
  ctx.scopes.back()[base] = unique;

  // mutate AST char*
  declNode->stmt.decl.name = strdup(unique.c_str());

  // collect for allocas
  ctx.allVarsUnique.insert(unique);
}

static void renameVarUse(RenameCtx& ctx, astNode* varNode) {
  // varNode->type == ast_var
  string base = varNode->var.name;
  string unique;
  if (lookupName(ctx, base, unique)) {
    varNode->var.name = strdup(unique.c_str());
  } else {
    // If this happens, semantic analysis should have caught it.
    // But don't crash; leave it unchanged or assert.
    // fprintf(stderr, "Unbound var use: %s\n", base.c_str());
  }
}

static void renameExpr(RenameCtx& ctx, astNode* e);
static void renameStmtNode(RenameCtx& ctx, astNode* s);

static void renameExpr(RenameCtx& ctx, astNode* e) {
  if (!e) return;
  switch (e->type) {
    case ast_var:
      renameVarUse(ctx, e);
      break;
    case ast_cnst:
      break;
    case ast_uexpr:
      renameExpr(ctx, e->uexpr.expr);
      break;
    case ast_bexpr:
      renameExpr(ctx, e->bexpr.lhs);
      renameExpr(ctx, e->bexpr.rhs);
      break;
    case ast_rexpr:
      renameExpr(ctx, e->rexpr.lhs);
      renameExpr(ctx, e->rexpr.rhs);
      break;
    case ast_stmt:
      // expressions won't be stmt normally, ignore
      break;
    default:
      break;
  }
}

static void renameBlock(RenameCtx& ctx, astNode* blockNode) {
  // blockNode->stmt.type == ast_block
  pushScope(ctx);

  auto* list = blockNode->stmt.block.stmt_list;
  for (astNode* stmtNode : *list) {
    renameStmtNode(ctx, stmtNode);
  }

  popScope(ctx);
}

static void renameStmtNode(RenameCtx& ctx, astNode* s) {
  if (!s) return;
  if (s->type != ast_stmt) return;

  switch (s->stmt.type) {
    case ast_decl:
      renameDecl(ctx, s);
      break;

    case ast_asgn:
      // lhs should be ast_var in this grammar
      renameExpr(ctx, s->stmt.asgn.lhs);
      renameExpr(ctx, s->stmt.asgn.rhs);
      break;

    case ast_call:
      // print(expr) or read() (but stmt-call likely print only)
      if (s->stmt.call.param) renameExpr(ctx, s->stmt.call.param);
      break;

    case ast_ret:
      renameExpr(ctx, s->stmt.ret.expr);
      break;

    case ast_if:
      renameExpr(ctx, s->stmt.ifn.cond);
      renameStmtNode(ctx, s->stmt.ifn.if_body);
      if (s->stmt.ifn.else_body) renameStmtNode(ctx, s->stmt.ifn.else_body);
      break;

    case ast_while:
      renameExpr(ctx, s->stmt.whilen.cond);
      renameStmtNode(ctx, s->stmt.whilen.body);
      break;

    case ast_block:
      renameBlock(ctx, s);
      break;

    default:
      break;
  }
}

static void renameFunction(RenameCtx& ctx, astNode* funcNode) {
  // funcNode->type == ast_func
  pushScope(ctx);

  // parameter introduces a name in the outermost function scope
  if (funcNode->func.param && funcNode->func.param->type == ast_var) {
    string base = funcNode->func.param->var.name;
    string unique = makeUnique(ctx, base);

    ctx.scopes.back()[base] = unique;
    funcNode->func.param->var.name = strdup(unique.c_str());

    ctx.paramUnique = unique;
    ctx.allVarsUnique.insert(unique);
  }

  // body is a statement node (likely a block)
  renameStmtNode(ctx, funcNode->func.body);

  popScope(ctx);
}

RenameCtx renameProgram(astNode* rootProg) {
  RenameCtx ctx;
  // prog scope isn’t needed; function-level is enough
  if (!rootProg || rootProg->type != ast_prog) return ctx;

  renameFunction(ctx, rootProg->prog.func);
  return ctx;
}

struct IRGen {
  LLVMModuleRef M = nullptr;
  LLVMBuilderRef B = nullptr;
  LLVMValueRef curFn = nullptr;

  LLVMValueRef printFn = nullptr;
  LLVMValueRef readFn = nullptr;

  std::unordered_map<std::string, LLVMValueRef> var_map; // uniqueName -> alloca
  LLVMValueRef ret_ref = nullptr;
  LLVMBasicBlockRef retBB = nullptr;

  // optional: keep types handy
  LLVMTypeRef i32 = LLVMInt32Type();
};

static LLVMValueRef genIRExpr(IRGen& g, astNode* e);

static LLVMIntPredicate ropToPred(rop_type op) {
  switch(op) {
    case lt:  return LLVMIntSLT;
    case gt:  return LLVMIntSGT;
    case le:  return LLVMIntSLE;
    case ge:  return LLVMIntSGE;
    case eq:  return LLVMIntEQ;
    case neq: return LLVMIntNE;
  }
  return LLVMIntEQ;
}

static LLVMValueRef genIRExpr(IRGen& g, astNode* e) {
  if (!e) return nullptr;

  switch (e->type) {
    case ast_cnst: {
      return LLVMConstInt(g.i32, (unsigned long long)e->cnst.value, /*sign*/ 1);
    }

    case ast_var: {
      std::string name = e->var.name;
      LLVMValueRef alloca = g.var_map.at(name);
      return LLVMBuildLoad2(g.B, g.i32, alloca, (name + "_ld").c_str());
    }

    case ast_uexpr: {
      // only unary minus
      LLVMValueRef v = genIRExpr(g, e->uexpr.expr);
      LLVMValueRef zero = LLVMConstInt(g.i32, 0, 1);
      return LLVMBuildSub(g.B, zero, v, "neg");
    }

    case ast_bexpr: {
      LLVMValueRef L = genIRExpr(g, e->bexpr.lhs);
      LLVMValueRef R = genIRExpr(g, e->bexpr.rhs);
      switch (e->bexpr.op) {
        case add:    return LLVMBuildAdd(g.B, L, R, "add");
        case sub:    return LLVMBuildSub(g.B, L, R, "sub");
        case mul:    return LLVMBuildMul(g.B, L, R, "mul");
        case divide: return LLVMBuildSDiv(g.B, L, R, "sdiv");
        default:     return nullptr;
      }
    }

    case ast_rexpr: {
      LLVMValueRef L = genIRExpr(g, e->rexpr.lhs);
      LLVMValueRef R = genIRExpr(g, e->rexpr.rhs);
      LLVMIntPredicate pred = ropToPred(e->rexpr.op);
      return LLVMBuildICmp(g.B, pred, L, R, "cmp"); // returns i1
    }

    case ast_stmt: {
      // read() might appear as an expression in this grammar via ast_call inside expr,
      // but in your AST, calls are statements (ast_stmt).
      // If your parser allows read() as expr, it might appear as ast_stmt with ast_call.
      if (e->stmt.type == ast_call) {
        // assume "read"
        return LLVMBuildCall2(g.B,
          LLVMGlobalGetValueType(g.readFn),
          g.readFn,
          nullptr, 0,
          "readcall");
      }
      return nullptr;
    }

    default:
      return nullptr;
  }
}

static LLVMBasicBlockRef genIRStmt(IRGen& g, astNode* s, LLVMBasicBlockRef startBB);

static bool hasTerminator(LLVMBasicBlockRef bb) {
  return LLVMGetBasicBlockTerminator(bb) != nullptr;
}

static LLVMBasicBlockRef genIRBlock(IRGen& g, astNode* blockNode, LLVMBasicBlockRef startBB) {
  LLVMBasicBlockRef prev = startBB;
  auto* list = blockNode->stmt.block.stmt_list;
  for (astNode* stmtNode : *list) {
    prev = genIRStmt(g, stmtNode, prev);
  }
  return prev;
}

static LLVMBasicBlockRef genIRStmt(IRGen& g, astNode* s, LLVMBasicBlockRef startBB) {
  if (!s) return startBB;
  if (s->type != ast_stmt) return startBB;

  switch (s->stmt.type) {

    case ast_decl:
      // no IR emitted here; allocas already created in entry
      return startBB;

    case ast_asgn: {
      LLVMPositionBuilderAtEnd(g.B, startBB);
      // lhs is ast_var with unique name
      std::string lhsName = s->stmt.asgn.lhs->var.name;
      LLVMValueRef rhsV = genIRExpr(g, s->stmt.asgn.rhs);
      LLVMBuildStore(g.B, rhsV, g.var_map.at(lhsName));
      return startBB;
    }

    case ast_call: {
      LLVMPositionBuilderAtEnd(g.B, startBB);

      std::string fname = s->stmt.call.name;
      if (fname == "print") {
        LLVMValueRef arg = genIRExpr(g, s->stmt.call.param);
        LLVMValueRef args[1] = { arg };
        LLVMBuildCall2(g.B,
          LLVMGlobalGetValueType(g.printFn),
          g.printFn,
          args, 1,
          "");
      } else if (fname == "read") {
        // read() as a statement is weird but harmless: call and drop result
        LLVMBuildCall2(g.B,
          LLVMGlobalGetValueType(g.readFn),
          g.readFn,
          nullptr, 0,
          "");
      }
      return startBB;
    }

    case ast_ret: {
      LLVMPositionBuilderAtEnd(g.B, startBB);
      LLVMValueRef v = genIRExpr(g, s->stmt.ret.expr);
      LLVMBuildStore(g.B, v, g.ret_ref);
      LLVMBuildBr(g.B, g.retBB);

      // return a fresh continuation block (likely unreachable)
      LLVMBasicBlockRef endBB = LLVMAppendBasicBlock(g.curFn, "after_return");
      return endBB;
    }

    case ast_block: {
      return genIRBlock(g, s, startBB);
    }

    case ast_if: {
      LLVMPositionBuilderAtEnd(g.B, startBB);
      LLVMValueRef cond = genIRExpr(g, s->stmt.ifn.cond); // i1

      LLVMBasicBlockRef trueBB  = LLVMAppendBasicBlock(g.curFn, "if_true");
      LLVMBasicBlockRef falseBB = LLVMAppendBasicBlock(g.curFn, "if_false");

      LLVMBuildCondBr(g.B, cond, trueBB, falseBB);

      // generate if body starting at trueBB
      LLVMBasicBlockRef ifExit = genIRStmt(g, s->stmt.ifn.if_body, trueBB);

      if (s->stmt.ifn.else_body == nullptr) {
        // no else: ifExit should flow to falseBB if not terminated
        if (!hasTerminator(ifExit)) {
          LLVMPositionBuilderAtEnd(g.B, ifExit);
          LLVMBuildBr(g.B, falseBB);
        }
        return falseBB;
      } else {
        // else part exists
        LLVMBasicBlockRef elseExit = genIRStmt(g, s->stmt.ifn.else_body, falseBB);

        LLVMBasicBlockRef endBB = LLVMAppendBasicBlock(g.curFn, "if_end");

        if (!hasTerminator(ifExit)) {
          LLVMPositionBuilderAtEnd(g.B, ifExit);
          LLVMBuildBr(g.B, endBB);
        }
        if (!hasTerminator(elseExit)) {
          LLVMPositionBuilderAtEnd(g.B, elseExit);
          LLVMBuildBr(g.B, endBB);
        }

        return endBB;
      }
    }

    case ast_while: {
      // startBB -> condBB
      LLVMPositionBuilderAtEnd(g.B, startBB);

      LLVMBasicBlockRef condBB  = LLVMAppendBasicBlock(g.curFn, "while_cond");
      LLVMBasicBlockRef bodyBB  = LLVMAppendBasicBlock(g.curFn, "while_body");
      LLVMBasicBlockRef exitBB  = LLVMAppendBasicBlock(g.curFn, "while_exit");

      LLVMBuildBr(g.B, condBB);

      // condBB: condbr
      LLVMPositionBuilderAtEnd(g.B, condBB);
      LLVMValueRef cond = genIRExpr(g, s->stmt.whilen.cond); // i1
      LLVMBuildCondBr(g.B, cond, bodyBB, exitBB);

      // bodyBB: generate body
      LLVMBasicBlockRef bodyExit = genIRStmt(g, s->stmt.whilen.body, bodyBB);

      // loop back if body doesn't terminate
      if (!hasTerminator(bodyExit)) {
        LLVMPositionBuilderAtEnd(g.B, bodyExit);
        LLVMBuildBr(g.B, condBB);
      }

      return exitBB;
    }

    default:
      return startBB;
  }
}

static void buildExterns(IRGen& g) {
  // declare void print(i32)
  LLVMTypeRef printTy = LLVMFunctionType(LLVMVoidType(), &g.i32, 1, 0);
  g.printFn = LLVMAddFunction(g.M, "print", printTy);

  // declare i32 read()
  LLVMTypeRef readTy = LLVMFunctionType(g.i32, nullptr, 0, 0);
  g.readFn = LLVMAddFunction(g.M, "read", readTy);
}

LLVMModuleRef buildIR(astNode* progRoot) {
  IRGen g;
  g.M = LLVMModuleCreateWithName("miniC");
  buildExterns(g);

  // rename first
  RenameCtx rctx = renameProgram(progRoot);

  // create function
  astNode* fnNode = progRoot->prog.func;
  std::string fnName = fnNode->func.name;

  bool hasParam = (fnNode->func.param != nullptr);

  LLVMTypeRef fnTy = nullptr;
  if (hasParam) {
    LLVMTypeRef args[1] = { g.i32 };
    fnTy = LLVMFunctionType(g.i32, args, 1, 0);
  } else {
    fnTy = LLVMFunctionType(g.i32, nullptr, 0, 0);
  }

  g.curFn = LLVMAddFunction(g.M, fnName.c_str(), fnTy);

  // entry block + builder
  LLVMBasicBlockRef entryBB = LLVMAppendBasicBlock(g.curFn, "entry");
  g.B = LLVMCreateBuilder();
  LLVMPositionBuilderAtEnd(g.B, entryBB);

  // allocas for all vars (unique names collected in renamer)
  for (const auto& uname : rctx.allVarsUnique) {
    LLVMValueRef a = LLVMBuildAlloca(g.B, g.i32, uname.c_str());
    g.var_map[uname] = a;
  }

  // return slot
  g.ret_ref = LLVMBuildAlloca(g.B, g.i32, "ret");

  // store parameter into its alloca
  if (hasParam) {
    LLVMValueRef p0 = LLVMGetParam(g.curFn, 0);
    LLVMBuildStore(g.B, p0, g.var_map.at(rctx.paramUnique));
  }

  // retBB
  g.retBB = LLVMAppendBasicBlock(g.curFn, "return");
  LLVMPositionBuilderAtEnd(g.B, g.retBB);
  LLVMValueRef rv = LLVMBuildLoad2(g.B, g.i32, g.ret_ref, "rv");
  LLVMBuildRet(g.B, rv);

  // generate body
  LLVMBasicBlockRef exitBB = genIRStmt(g, fnNode->func.body, entryBB);

  // if function body doesn't end with terminator, branch to retBB
  if (LLVMGetBasicBlockTerminator(exitBB) == nullptr) {
    LLVMPositionBuilderAtEnd(g.B, exitBB);
    LLVMBuildBr(g.B, g.retBB);
  }

  // Prune unreachable blocks via BFS from entryBB.
  // Any block not reachable from entry has no predecessors and can be deleted.
  {
    std::unordered_set<LLVMBasicBlockRef> visited;
    std::queue<LLVMBasicBlockRef> worklist;
    worklist.push(entryBB);
    visited.insert(entryBB);

    while (!worklist.empty()) {
      LLVMBasicBlockRef cur = worklist.front(); worklist.pop();
      LLVMValueRef term = LLVMGetBasicBlockTerminator(cur);
      if (!term) continue;
      unsigned nSucc = LLVMGetNumSuccessors(term);
      for (unsigned i = 0; i < nSucc; i++) {
        LLVMBasicBlockRef succ = LLVMGetSuccessor(term, i);
        if (visited.find(succ) == visited.end()) {
          visited.insert(succ);
          worklist.push(succ);
        }
      }
    }

    // Collect unreachable blocks (not visited by BFS).
    std::vector<LLVMBasicBlockRef> dead;
    for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(g.curFn);
         bb; bb = LLVMGetNextBasicBlock(bb)) {
      if (visited.find(bb) == visited.end()) dead.push_back(bb);
    }

    // Delete them. Each dead block's instructions are also deleted.
    for (LLVMBasicBlockRef bb : dead) {
      LLVMDeleteBasicBlock(bb);
    }
  }

  LLVMDisposeBuilder(g.B);
  return g.M;
}

// buildIR is defined above; this declaration is for external callers (main.cpp).
LLVMModuleRef buildIR(astNode* progRoot);
