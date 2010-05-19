%{
#include "filter_parse.h"

double count, length, watchtime;
char *filter_result, *name, *catfile;

/*
filter=if catfile: cat = catfile
filter=if count == 0: cat = "uudet"
filter=if length < 60: cat = "lyhyet"
filter=if length > 300: cat = "pitkät"
filter=if name: cat = str((100 * watchtime / count) / length)
*/

%}

%option noyywrap

int [0-9]+
string \"[^\"]+\"

%%

{int}	  { yylval.num = atof (yytext); return NUM; }
{string}  { yylval.str = strndup (yytext + 1, strlen (yytext + 1) -1); return STRING; }
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
