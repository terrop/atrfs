/* entry_filter.c - 19.5.2010 - 9.7.2010 Ari & Tero Roponen */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "entrydb.h"
#include "entry_filter.h"

extern char *get_sha1_fast (char *filename);
GHashTable *sha1_to_entry_map;

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

char *get_category (struct atrfs_entry *ent)
{
	struct filter *filt;
	char *cat = NULL;

	int filter_cb (void *data, int ncols, char **values, char **names)
	{
		if (ncols != 2)
			tmplog ("get_category: ncols != 2\n");

		char *c = values[0];
		struct atrfs_entry *e = g_hash_table_lookup (sha1_to_entry_map, values[1]);
		if (ent == e)
			cat = strdup (c);
		return 0;
	}

	ASSERT_TYPE (ent, ATRFS_FILE_ENTRY);

	/* Category file cat.txt will always take precedence. */
	cat = get_catfile (ent);
	if (cat)
		return strdup (cat);

	for (filt = filters; !cat && filt; filt = filt->next)
	{
		/* select "uudet", sha1 from Files where count = 0; */
		if (entrydb_exec (filter_cb, filt->str) && cat)
			return cat;
	}

	return cat;
}
