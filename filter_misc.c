/* filter_misc.c - 19.5.2010 - 19.5.2010 Ari & Tero Roponen */

/* C-code for lexer. */

#include "entry.h"

char *filter_result;
static double count, length, watchtime;
static char *name, *catfile;

void filter_init (struct atrfs_entry *ent)
{
#ifndef FILTER_TEST
	count = get_watchcount (ent);
	length = get_length (ent);
	watchtime = get_watchtime (ent);
	name = ent->name;
	filter_result = NULL;
#else
	count = 1;
	length = 120.0;
	watchtime = 60.0;
	catfile = strdup ("Natale 2009");
	name = "Test.flv";
	filter_result = NULL;
#endif
}
