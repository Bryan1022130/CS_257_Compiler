#include <cstdio>
#include <cstdlib>

#include <llvm-c/Core.h>
#include <llvm-c/Support.h>

// Use the same AST header as Part1's parser
#include "../Part1/ast/ast.h"

int yyparse(void);
extern FILE* yyin;

// Part1 parser sets this global
extern astNode* ast_root;

// Part2 builder function (in ir_builder.cpp)
LLVMModuleRef buildIR(astNode* progRoot);

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <input.c>\n", argv[0]);
    return 1;
  }

  yyin = fopen(argv[1], "r");
  if (!yyin) {
    perror("fopen");
    return 1;
  }

  if (yyparse() != 0) {
    fprintf(stderr, "Parse failed\n");
    return 1;
  }

  if (!ast_root) {
    fprintf(stderr, "ERROR: ast_root is NULL (parser didn't set it)\n");
    return 1;
  }

  LLVMModuleRef M = buildIR(ast_root);
  char* s = LLVMPrintModuleToString(M);
  printf("%s", s);  // LLVMPrintModuleToString already ends with a newline
  LLVMDisposeMessage(s);

  return 0;
}
