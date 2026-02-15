#include <cstdio>
#include <cstdlib>

#include "../ast/ast.h"

int yyparse();
extern FILE* yyin;
extern astNode* ast_root;

// link to semantic
int semantic_check(astNode* root);

int main(int argc, char** argv) {
  //retunrs non-zero on error
  if (argc != 2) {
    fprintf(stderr, "usage: %s <input_file>\n", argv[0]);
    return 1;
  }

  // open input file
  yyin = fopen(argv[1], "r");
  // lex reads it with yyin
  if (!yyin) {
    perror("fopen");
    return 1;
  }

  // runs parser
  // if the file does not exist, yyparse will fail
  int rc = yyparse();
  if (rc != 0 || ast_root == NULL) {
    fprintf(stderr, "Parse failed.\n");
    return 1;
  }

  printNode(ast_root, 0);

  // call to semantic analysis
  int sem = semantic_check(ast_root);
  if (sem != 0) {
    fprintf(stderr, "Semantic analysis failed.\n");
    freeNode(ast_root);
    return 1;
  }

  freeNode(ast_root);
  return 0;
}
