#include <sys/stat.h>
#include <errno.h>
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "atrfs_ops.h"
#include "entry.h"
#include "entry_filter.h"
#include "util.h"

extern char *language_list;
unsigned int stat_count = 20;
#define RECENT_COUNT 10

struct atrfs_entry *statroot;

void update_recent_file (struct atrfs_entry *ent)
{
	static struct atrfs_entry *recent_files[RECENT_COUNT];

	int i;
	struct atrfs_entry *recent = lookup_entry_by_name (statroot, "recent");
	if (! recent)
		return;
	ASSERT_TYPE (recent, ATRFS_VIRTUAL_FILE_ENTRY);

	if (! ent)
		abort ();
	if (recent_files[0] && ent == recent_files[0])
	{
	} else {
		for (i = RECENT_COUNT - 1; i > 0; i--)
			recent_files[i] = recent_files[i - 1];
		recent_files[0] = ent;
	}

	char *buf = NULL;
	size_t size;
	FILE *fp = open_memstream (&buf, &size);
	for (i = 0; i < RECENT_COUNT && recent_files[i]; i++)
	{
		fprintf (fp, "%s%c%s\n",
			recent_files[i]->parent->name,
			recent_files[i]->parent == root ? '\0' : '/',
			recent_files[i]->name);
	}
	fclose (fp);
	free (VIRTUAL_ENTRY(recent)->m_data);
	VIRTUAL_ENTRY(recent)->set_contents(recent, buf, size);
}

void update_stats (void)
{
	struct atrfs_entry *st_ents[2];
	st_ents[0] = lookup_entry_by_name(statroot, "top-list");
	st_ents[1] = lookup_entry_by_name(statroot, "last-list");

	struct atrfs_entry **entries;
	size_t count;
	int i, j;

	int compare_times (void *a, void *b)
	{
		struct atrfs_entry *e1, *e2;
		e1 = *(struct atrfs_entry **)a;
		e2 = *(struct atrfs_entry **)b;
		if (get_watchtime(e1) > get_watchtime(e2))
			return -1;
		return 1;
	}

	get_all_file_entries (&entries, &count);

	tmplog ("sort begins\n");
	qsort (entries, count, sizeof (struct atrfs_entry *),
	       (comparison_fn_t) compare_times);
	tmplog ("sort ends\n");

	for (j = 0; j < 2; j++)
	{
		char *stbuf = NULL;
		size_t stsize;
		FILE *stfp = open_memstream(&stbuf, &stsize);

		for (i = 0; i < stat_count && entries[i]; i++)
		{
			struct atrfs_entry *ent;
			if (j == 0)
				ent = entries[i];
			else
				ent = entries[count - 1 - i];

			double val = get_watchtime(ent);
			fprintf (stfp, "%s\t%s%c%s\n", secs_to_timestr (val),
				 ent->parent == root ? "" : ent->parent->name,
				 ent->parent == root ? '\0' : '/', ent->name);
		}

		fclose (stfp);
		free (VIRTUAL_ENTRY(st_ents[j])->m_data);
		VIRTUAL_ENTRY(st_ents[j])->set_contents(st_ents[j], stbuf, stsize);
	}

	free (entries);

	struct atrfs_entry *lang;
	lang = lookup_entry_by_name(statroot, "language");
	if (lang)
		VIRTUAL_ENTRY(lang)->set_contents(lang, language_list, strlen(language_list));

	struct atrfs_entry *statcount;
	statcount = lookup_entry_by_name(statroot, "stat-count");
	if (statcount)
	{
		static char buf[10];
		sprintf(buf, "%d\n", stat_count);
		VIRTUAL_ENTRY(statcount)->set_contents(statcount, buf, strlen(buf));
	}
}

static void move_to_named_subdir (struct atrfs_entry *ent, char *subdir)
{
	struct atrfs_entry *dir = lookup_entry_by_name (root, subdir);
	if (! dir)
	{
		dir = create_entry (ATRFS_DIRECTORY_ENTRY);
		attach_entry (root, dir, subdir);
	}
	move_entry (ent, dir);
}

void categorize_file_entry (struct atrfs_entry *ent)
{
	ASSERT_TYPE (ent, ATRFS_FILE_ENTRY);

	/* Handle file-specific configuration. */
	struct atrfs_entry *conf = NULL;
	int size = getxattr(REAL_NAME(ent), "user.mpconf", NULL, 0);
	if (size > 0)
	{
//		tmplog ("File-specific config for %s:%d\n", REAL_NAME(ent), size);
		char cfgname[strlen (ent->name) + 6];
		sprintf (cfgname, "%s.conf", ent->name);
		conf = lookup_entry_by_name (ent->parent, cfgname);
		if (! conf)
		{
			char *data;
			size_t sz;
			conf = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
			data = malloc (size);
			sz = getxattr(REAL_NAME(ent), "user.mpconf", data, size);
			tmplog("Data: '%s'\n", data);
			VIRTUAL_ENTRY(conf)->set_contents(conf, data, sz);
			attach_entry (ent->parent, conf, cfgname);
		}
	}

	char *dir = get_category (ent);
	move_to_named_subdir (ent, dir);
	if (conf)
		move_to_named_subdir (conf, dir);
	free (dir);
}
