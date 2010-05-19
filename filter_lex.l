%{
#include "filter_parse.h"
#include "filter_misc.c"
%}

%option noyywrap

int [0-9]+
string \"[^\"]+\"

%%

{int}	  { yylval.num = atof (yytext); return NUM; }
{string}  { yylval.str = strndup (yytext + 1, strlen (yytext + 1) -1); return STRING; }
"=="      { return EQ; }
"!="      { return NE; }
"<"       { return LT; }
"<="      { return LE; }
">"       { return GT; }
">="      { return GE; }
"if"	  { return IF; }
"catfile" { yylval.str = catfile; return CATFILE; }
"count"   { yylval.num = count; return COUNT; }
"length"  { yylval.num = length; return LENGTH; }
"name"    { yylval.str = name; return NAME; }
"cat"	  { return CAT; }
"watchtime" { yylval.num = watchtime; return WATCHTIME; }
"str" { return STR; }
[ ]	{ /* skip whitespace */ }
.	{ return yytext[0]; }
