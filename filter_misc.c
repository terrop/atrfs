/* filter_misc.c - 19.5.2010 - 19.5.2010 Ari & Tero Roponen */

/* C-code for lexer. */

#include "entry.h"

char *filter_result;
static double count, length, watchtime;
static char *name, *catfile;

static char *get_catfile (struct atrfs_entry *ent)
{
	static char *catfile = NULL;
	if (catfile)
	{
		free (catfile);
		catfile = NULL;
	}

	char buf[256];
	char *s;
	strcpy (buf, REAL_NAME(ent));
	s = strrchr (buf, '/');
	if (s)
	{
		strcpy (s + 1, "cat.txt");
		if (access (buf, R_OK) == 0)
		{
			FILE *fp = fopen (buf, "r");
			if (fp)
			{
				if (fgets (buf, sizeof (buf), fp))
				{
					catfile = strndup (buf, strlen (buf) - 1);
				}
				fclose (fp);
			}
		}
	}

	return catfile;
}

void filter_init (struct atrfs_entry *ent)
{
#ifndef FILTER_TEST
	count = get_total_watchcount (ent);
	length = get_total_length (ent);
	watchtime = get_total_watchtime (ent);
	catfile = get_catfile (ent);
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
