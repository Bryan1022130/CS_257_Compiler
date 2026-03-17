#include "semantic.h"

#include <cstdio>
#include <string>
#include <vector>
#include <unordered_set>

static std::vector<std::unordered_set<std::string>> scopeStack;
static int hadError = 0;

static void enterScope() {
    scopeStack.push_back({});
}

static void exitScope() {
    scopeStack.pop_back();
}

static bool declaredInCurrentScope(const std::string& name) {
    if (scopeStack.empty()) return false;
    return scopeStack.back().count(name) > 0;
}

static bool declaredInAnyScope(const std::string& name) {
    for (int i = (int)scopeStack.size() - 1; i >= 0; --i) {
        if (scopeStack[i].count(name) > 0) return true;
    }
    return false;
}

static void declareName(const std::string& name) {
    if (declaredInCurrentScope(name)) {
        fprintf(stderr, "Semantic error: duplicate declaration of '%s' in same scope\n",
                name.c_str());
        hadError = 1;
        return;
    }
    scopeStack.back().insert(name);
}

static void useName(const std::string& name) {
    if (!declaredInAnyScope(name)) {
        fprintf(stderr, "Semantic error: variable '%s' used before declaration\n",
                name.c_str());
        hadError = 1;
    }
}

static void checkNode(astNode* n, bool isTopLevelFuncBlock);

static void checkStmtNode(astNode* n, bool isTopLevelFuncBlock) {
    astStmt* s = &n->stmt;

    switch (s->type) {
        case ast_decl: {
            declareName(std::string(s->decl.name));
            break;
        }

        case ast_asgn: {

            if (s->asgn.lhs && s->asgn.lhs->type == ast_var) {
                useName(std::string(s->asgn.lhs->var.name));
            } else {
                checkNode(s->asgn.lhs, false);
            }
            checkNode(s->asgn.rhs, false);
            break;
        }

        case ast_ret: {
            checkNode(s->ret.expr, false);
            break;
        }

        case ast_call: {
            if (s->call.param) checkNode(s->call.param, false);
            break;
        }

        case ast_if: {
            checkNode(s->ifn.cond, false);
            checkNode(s->ifn.if_body, false);
            if (s->ifn.else_body) checkNode(s->ifn.else_body, false);
            break;
        }

        case ast_while: {
            checkNode(s->whilen.cond, false);
            checkNode(s->whilen.body, false);
            break;
        }

        case ast_block: {
            if (!isTopLevelFuncBlock) enterScope();

            for (astNode* st : *(s->block.stmt_list)) {
                checkNode(st, false);
            }

            if (!isTopLevelFuncBlock) exitScope();
            break;
        }

        default:
            break;
    }
}

static void checkExprNode(astNode* n) {
    switch (n->type) {
        case ast_var:
            useName(std::string(n->var.name));
            break;

        case ast_cnst:
            break;

        case ast_bexpr:
            checkNode(n->bexpr.lhs, false);
            checkNode(n->bexpr.rhs, false);
            break;

        case ast_uexpr:
            checkNode(n->uexpr.expr, false);
            break;

        case ast_rexpr:
            checkNode(n->rexpr.lhs, false);
            checkNode(n->rexpr.rhs, false);
            break;

        case ast_stmt:
            checkStmtNode(n, false);
            break;

        default:
            break;
    }
}

static void checkNode(astNode* n, bool isTopLevelFuncBlock) {
    if (!n) return;

    switch (n->type) {
        case ast_prog: {
            if (n->prog.func) checkNode(n->prog.func, false);
            break;
        }

        case ast_func: {
            enterScope();

            if (n->func.param && n->func.param->type == ast_var) {
                declareName(std::string(n->func.param->var.name));
            }

            checkNode(n->func.body, true);

            exitScope();
            break;
        }

        case ast_stmt:
            checkStmtNode(n, isTopLevelFuncBlock);
            break;

        default:
            checkExprNode(n);
            break;
    }
}

int semantic_check(astNode* root) {
    hadError = 0;
    scopeStack.clear();

    if (!root) return 1;
    checkNode(root, false);
    return hadError;
}
