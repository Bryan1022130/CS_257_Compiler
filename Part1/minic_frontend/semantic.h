#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "../ast/ast.h"

/*
 * Perform semantic analysis on the AST rooted at 'root'.
 * Checks:
 *   1. A variable is declared before it is used.
 *   2. A variable is declared at most once per scope.
 *
 * Returns 0 if no errors were found, non-zero otherwise.
 */
int semantic_check(astNode* root);

#endif
