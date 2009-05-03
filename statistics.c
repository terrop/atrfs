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
	CHECK_TYPE (recent, ATRFS_VIRTUAL_FILE_ENTRY);

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
	free (recent->virtual.data);
	recent->virtual.data = buf;
	recent->virtual.size = size;
}

int get_total_watchcount(struct atrfs_entry *ent);
double get_total_watchtime(struct atrfs_entry *ent);
double get_total_length(struct atrfs_entry *ent);

void update_stats (void)
{
	struct atrfs_entry *statcount_ent;
	struct atrfs_entry *language_ent;
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
		if (get_total_watchtime(e1) > get_total_watchtime(e2))
			return -1;
		return 1;
	}

	get_all_file_entries (&entries, &count);

	qsort (entries, count, sizeof (struct atrfs_entry *),
	       (comparison_fn_t) compare_times);

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

			double val = get_total_watchtime(ent);
			fprintf (stfp, "%s\t%s%c%s\n", secs_to_time (val),
				 ent->parent == root ? "" : ent->parent->name,
				 ent->parent == root ? '\0' : '/', ent->name);
		}

		fclose (stfp);
		free (st_ents[j]->virtual.data);
		st_ents[j]->virtual.data = stbuf;
		st_ents[j]->virtual.size = stsize;
	}
		
	free (entries);

	language_ent = lookup_entry_by_name(statroot, "language");
	if (language_ent)
	{
		language_ent->virtual.data = language_list;
		language_ent->virtual.size = strlen(language_list);
	}

	statcount_ent = lookup_entry_by_name(statroot, "stat-count");
	if (statcount_ent)
	{
		static char buf[10];
		sprintf(buf, "%d\n", stat_count);
		statcount_ent->virtual.data = buf;
		statcount_ent->virtual.size = strlen(buf);
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

void categorize_flv_entry (struct atrfs_entry *ent)
{
	CHECK_TYPE (ent, ATRFS_FILE_ENTRY);

	/* Handle file-specific configuration. */
	struct atrfs_entry *conf = NULL;
	int size = getxattr(ent->file.real_path, "user.mpconf", NULL, 0);
	if (size > 0)
	{
//		tmplog ("File-specific config for %s:%d\n", ent->file.real_path, size);
		char cfgname[strlen (ent->name) + 6];
		sprintf (cfgname, "%s.conf", ent->name);
		conf = lookup_entry_by_name (ent->parent, cfgname);
		if (! conf)
		{
			conf = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
			conf->virtual.data = malloc (size);
			conf->virtual.size = getxattr(ent->file.real_path,
				"user.mpconf", conf->virtual.data, size);
			tmplog("Data: '%s'\n", conf->virtual.data);
			attach_entry (ent->parent, conf, cfgname);
		}
	}

	int count = get_total_watchcount(ent);
	double total = get_total_watchtime(ent);
	double filelen = get_total_length(ent);

	if (count && total > 0.0 && filelen > 0.0)
	{
		char *dir = get_pdir(filelen, count, total);
		move_to_named_subdir (ent, dir);
		if (conf)
			move_to_named_subdir (conf, dir);
	}
}
