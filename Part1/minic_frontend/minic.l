%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minic.tab.h"
extern YYSTYPE yylval;
%}

%%

[ \t\r\n]+ ;

"//".* ;


"int"   {return INT;}
"void" {return VOID;}
"if"    {return IF;}
"else"    {return ELSE;}
"while"    {return WHILE ;}
"return"    {return RETURN;}
"extern"    {return EXTERN;}
"print"    {return PRINT;}
"read"    {return READ;}

"=="    {return EQ;}
"!="    {return NEQ;}
"<="    {return LE;}
">="    {return GE;}
"<"    {return LT;}
">"    {return GT;}

"="    {return ASSIGN;}
"+"    {return PLUS;}
"-"    {return MINUS;}
"*"    {return MUL;}
"/"    {return DIV;}

"("    {return LPAREN;}
")"    {return RPAREN;}
"{"    {return LBRACE;}
"}"    {return RBRACE;}
";"    {return SEMI;}
","    {return COMMA;}

[0-9]+    {
            yylval.ival = atoi(yytext);
            return NUM;}

[A-Za-z_][A-Za-z0-9_]*    {
                            yylval.sval = strdup(yytext);
                            return ID;}

.     {
        fprintf (stderr,
            "lex error: unexpected character'%s'\n",
            yytext);
        return ERROR;}

%%


int yywrap() {
    return 1;
}