%{
#include <stdio.h>
#include <string.h>
extern char *filter_result;

/*
filter=if catfile: cat = catfile
filter=if count == 0: cat = "uudet"
filter=if length < 60: cat = "lyhyet"
filter=if length > 300: cat = "pitkät"
filter=if name: cat = str((100 * watchtime / count) / length)
*/

%}

%union {
	double num;
	char *str;
}

%token CAT EQ GE GT IF LE LT NE STR
%token <str> CATFILE NAME STRING
%token <num> COUNT NUM LENGTH WATCHTIME

%type <num> boolexp cmp num
%type <str> string

%debug
%expect 4

%%

filter: IF boolexp ':' CAT '=' string { if ($2) filter_result = strdup ($6); return 0; }
;

boolexp: cmp
|        num
|        string { $$ = ($1 != NULL); }
;

cmp: num EQ num { $$ = ($1 == $3); }
|    num LT num { $$ = ($1 < $3); }
|    num LE num { $$ = ($1 <= $3); }
|    num GT num { $$ = ($1 > $3); }
|    num GE num { $$ = ($1 >= $3); }
|    num NE num { $$ = ($1 != $3); }
;

num: NUM
|    COUNT
|    LENGTH
|    WATCHTIME
|    '(' num ')' {$$ = $2;}
|    num '*' num { $$ = $1 * $3; }
|    num '/' num { $$ = $1 / $3; }
;

string: STRING
|       CATFILE
|       NAME
|       STR '(' num ')' { char buf[20]; sprintf (buf, "%0.0lf", $3); $$ = strdup (buf); }
;

%%

int yyerror (char *msg)
{
	fprintf (stderr, "%s\n", msg);
}

#ifdef FILTER_TEST
int main (void)
{
	extern FILE *yyin;
	yyin = stdin;
	filter_init (NULL);

//	yydebug = 1;
	filter_result = NULL;
	if (yyparse() == 0)
		printf ("%s\n", filter_result);
	free (filter_result);
	return 0;
}
#endif /* FILTER_TEST */
