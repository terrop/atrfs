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
extern char *filter_result;

char *get_category (struct atrfs_entry *ent)
{
	struct filter *filt;
	char *cat = NULL;

	CHECK_TYPE (ent, ATRFS_FILE_ENTRY);

	filter_init (ent);

	for (filt = filters; !cat && filt; filt = filt->next)
	{
		FILE *fp = fmemopen (filt->str, strlen (filt->str), "r");
		if (fp)
		{
			yyin = fp;
			filter_result = NULL;
			if (yyparse () == 0 && filter_result)
				cat = filter_result;
			fclose (fp);
		}
	}

	return cat;
}
