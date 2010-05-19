/* entry_filter.c - 19.5.2010 - 19.5.2010 Ari & Tero Roponen */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "entrydb.h"
#include "entry_filter.h"

struct filter
{
	char *str;
	struct filter *next;
};

static struct filter *filters;

void add_filter (char *str)
{
	struct filter *filt = malloc (sizeof (*filt));
	if (filt)
	{
		filt->str = strdup (str);
		filt->next = NULL;

		/* Order matters. */
		if (! filters)
			filters = filt;
		else {
			struct filter *last = filters;
			while (last->next)
				last = last->next;
			last->next = filt;
		}
	}
}

extern int yyparse (void);
extern FILE *yyin;
extern double count, length, watchtime;
extern char *filter_result, *name, *catfile;

int get_total_watchcount(struct atrfs_entry *ent);
double get_total_watchtime(struct atrfs_entry *ent);
double get_total_length(struct atrfs_entry *ent);

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
	strcpy (buf, FILE_ENTRY(ent)->real_path);
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
					tmplog ("catfile: %s\n", catfile);
				}
				fclose (fp);
			}
		}
	}

	return catfile;
}

char *get_category (struct atrfs_entry *ent)
{
	struct filter *filt;
	char *cat = NULL;

	CHECK_TYPE (ent, ATRFS_FILE_ENTRY);

	count = get_total_watchcount (ent);
	length = get_total_length (ent);
	watchtime = get_total_watchtime (ent);
	name = ent->name;
	catfile = get_catfile (ent);

	for (filt = filters; !cat && filt; filt = filt->next)
	{
		FILE *fp = fmemopen (filt->str, strlen (filt->str), "r");
		if (fp)
		{
			yyin = fp;
			filter_result = NULL;
			if (yyparse () == 0 && filter_result)
			{
				cat = filter_result;
				tmplog ("%s: %s -> %s\n", FILE_ENTRY(ent)->real_path,
					filt->str, cat);
			}
			fclose (fp);
		}
	}

	return cat;
}
