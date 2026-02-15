%{
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include "../ast/ast.h"

extern int yylex();
int yyerror(const char *s);
extern FILE *yyin;
astNode* ast_root = NULL;
%}

%code requires {
#include <vector>
#include "../ast/ast.h"
}

/*yylval type*/
%union{
    int ival;
    char* sval;
    astNode* node;
    std::vector<astNode*>* vec;
}

/*tokens*/
%token <ival> NUM
%token <sval> ID

%token INT VOID IF ELSE WHILE RETURN EXTERN PRINT READ

%token EQ NEQ LE GE LT GT
%token ASSIGN PLUS MINUS MUL DIV
%token LPAREN RPAREN LBRACE RBRACE SEMI COMMA
%token ERROR

%type <node> program externs extern_decl funcdef param_opt block stmt
%type <node> decl_stmt asgn_stmt call_stmt ret_stmt if_stmt while_stmt
%type <node> expr aexpr term factor rexpr
%type <vec>  stmt_list

/*start sym*/
%start program
%%

program
    : externs funcdef{
        ast_root = createProg($1->prog.ext1,$1->prog.ext2, $2);
        $$ = ast_root;
    }
    ;

/*externs*/

externs
    : extern_decl extern_decl{
        astNode* tmp = createProg($1,$2, NULL);
        $$ = tmp;
    }
    ;

extern_decl
    : EXTERN VOID PRINT LPAREN INT RPAREN SEMI{
        $$ = createExtern("print");
    }
    | EXTERN INT READ LPAREN RPAREN SEMI{
        $$ = createExtern("read");
    }
    ;
    
/*funct def*/
funcdef
    : INT ID LPAREN param_opt RPAREN block {
        $$ = createFunc($2, $4, $6);
    }
    ;

param_opt
    : /*empty*/ { $$ = NULL;}
    | INT ID { $$ = createVar($2);}
    ;

/*block*/

block
    : LBRACE stmt_list RBRACE{
        $$ = createBlock($2);
        }
    ;

stmt_list
    : /*empty*/ { $$ = new std::vector<astNode*>();}
    | stmt_list stmt {$1->push_back($2); $$ = $1;}
    ;

stmt
    : decl_stmt
    | asgn_stmt
    | if_stmt
    | while_stmt
    | ret_stmt
    | call_stmt
    | block
    ;

decl_stmt
    : INT ID SEMI {$$ = createDecl($2);}
    ;

asgn_stmt
    : ID ASSIGN expr SEMI{
        $$ = createAsgn(createVar($1), $3);
        }
    ;

call_stmt
    : PRINT LPAREN expr RPAREN SEMI { $$ = createCall("print", $3); }
    ;

ret_stmt
    : RETURN expr SEMI {
        $$ = createRet($2);
        }
    ;

if_stmt
    : IF LPAREN rexpr RPAREN stmt {
        $$ = createIf($3, $5, NULL);
        }
    | IF LPAREN rexpr RPAREN stmt ELSE stmt {
        $$ = createIf($3, $5, $7);
        }
    ;

while_stmt
    : WHILE LPAREN rexpr RPAREN stmt {
        $$ = createWhile($3, $5);
        }
    ;

/*espressions*/

rexpr
    : expr LT expr { $$ = createRExpr($1, $3, lt); }
    | expr GT expr { $$ = createRExpr($1, $3, gt); }
    | expr LE expr { $$ = createRExpr($1, $3, le); }
    | expr GE expr { $$ = createRExpr($1, $3, ge); }
    | expr EQ expr { $$ = createRExpr($1, $3, eq); }
    | expr NEQ expr { $$ = createRExpr($1, $3, neq); }
    ;

expr
    : aexpr {
        $$ = $1;
        }
    ;

aexpr
    : aexpr PLUS term { $$ = createBExpr($1, $3, add); }
    | aexpr MINUS term { $$ = createBExpr($1, $3, sub); }
    | term { $$ = $1;}
    ;

term
    : term MUL factor { $$ = createBExpr($1, $3, mul); }
    | term DIV factor { $$ = createBExpr($1, $3, divide); }
    | factor {$$ = $1;}
    ;

factor
    : NUM {$$ = createCnst($1);}
    | ID {$$ = createVar($1);}
    | READ LPAREN RPAREN { $$ = createCall("read", NULL);}
    | MINUS factor { $$ = createUExpr($2, uminus);}
    | LPAREN expr RPAREN { $$ = $2;}
    ;
%%

int yyerror(const char *s) {
    fprintf(stderr, "Parse error: %s\n", s);
    return 0;
}