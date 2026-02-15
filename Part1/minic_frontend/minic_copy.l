%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minic.tab.h"
extern YYSTYPE yylval;
%}

%%
/*Rules*/

/*whitespace*/
[ \t\r\n]+ ;

/*comment*/
"//".* ;


/*kw*/
"int"   {return INT;}
"if"    {return IF;}
"else"    {return ELSE;}
"while"    {return WHILE ;}
"return"    {return RETURN;}
"extern"    {return EXTERN;}
"print"    {return PRINT;}
"read"    {return READ;}

/*"<"">"*/
"=="    {return EQ;}
"!="    {return NEQ;}
"<="    {return LE;}
">="    {return GE;}
"<"    {return LT;}
">"    {return GT;}

/*math*/
"="    {return ASSIGN;}
"+"    {return PLUS;}
"-"    {return MINUS;}
"*"    {return MUL;}
"/"    {return DIV;}

/*punct*/
"("    {return LPAREN;}
")"    {return RPAREN;}
"{"    {return LBRACE;}
"}"    {return RBRACE;}
";"    {return SEMI;}
","    {return COMMA;}

/*integers*/
[0-9]+    {
            yylval.ival = atoi(yytext);
            return NUM;}

/*id*/
[A-Za-z_][A-Za-z0-9_]*    {
                            yylval.sval = strdup(yytext);
                            return ID;}

/*error*/
.     {
        fprintf (stderr,
            "lex error: unexpected character'%s'\n",
            yytext);
        return ERROR;}

%%


int yywrap() {
    return 1;
}