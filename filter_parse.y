%{
#include <stdio.h>
#include <string.h>
extern char *filter_result;
%}

%union {
	double num;
	char *str;
}

%token CAT IF STR
%token <str> CATFILE NAME STRING
%token <num> COUNT NUM
%token <num> LENGTH WATCHTIME

%type <num> expr cmp num catfile
%type <str> string strexp

%debug

%%

filter: IF expr ':' CAT '=' string { if ($2) filter_result = strdup ($6); return 0; } ;

expr: cmp | catfile | num | NAME { $$ = 1 }

cmp: num '=' '=' num { $$ = ($1 == $4); }
|    num '<' num { $$ = ($1 < $3); }
|    num '<' '=' num { $$ = ($1 <= $4); }
|    num '>' num { $$ = ($1 > $3); }
|    num '>' '=' num { $$ = ($1 >= $4); }
|    num '!' '=' num { $$ = ($1 != $4); }
;

num: NUM | COUNT | LENGTH | WATCHTIME
| '(' num ')' {$$ = $2;}
| num '*' num { $$ = $1 * $3; }
| num '/' num { $$ = $1 / $3; }
;

catfile: CATFILE { $$ = $1 != NULL; }

string: STRING | CATFILE | strexp
;

strexp: STR '(' num ')' {char buf[20]; sprintf (buf, "%0.0lf", $3); $$ = strdup (buf); } ;

%%

int yyerror (char *msg)
{
	fprintf (stderr, "%s\n", msg);
}

double count, length, watchtime;
char *filter_result, *name, *catfile;

#ifdef FILTER_TEST

int main (void)
{
	extern FILE *yyin;
	yyin = stdin;
	count = 1.0, length = 120.0, watchtime = 60.0;

//	yydebug = 1;
	filter_result = NULL;
	if (yyparse() == 0)
		printf ("%s\n", filter_result);
	free (filter_result);
	return 0;
}
#endif /* FILTER_TEST */
