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

#ifndef LIST_SIZE
#define LIST_SIZE 10
#endif
#define XSTR(s) STR(s)
#define STR(s) #s
char *top_name = "top-" XSTR(LIST_SIZE);
char *last_name = "last-" XSTR(LIST_SIZE);
char *recent_name = "recent";
#undef STR
#undef XSTR

struct atrfs_entry *statroot;

static struct atrfs_entry *recent_files[LIST_SIZE];
void update_recent_file (struct atrfs_entry *ent)
{
	int i;
	struct atrfs_entry *recent = lookup_entry_by_name (statroot, recent_name);
	if (! recent)
		return;
	CHECK_TYPE (recent, ATRFS_VIRTUAL_FILE_ENTRY);

	if (! ent)
		abort ();
	if (recent_files[0] && ent == recent_files[0])
		return;
	for (i = LIST_SIZE - 1; i > 0; i--)
		recent_files[i] = recent_files[i - 1];
	recent_files[0] = ent;

	char *buf = NULL;
	size_t size;
	FILE *fp = open_memstream (&buf, &size);
	for (i = 0; i < LIST_SIZE && recent_files[i]; i++)
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

void update_stats (void)
{
	struct atrfs_entry *st_ents[2];
	st_ents[0] = lookup_entry_by_name(statroot, top_name);
	st_ents[1] = lookup_entry_by_name(statroot, last_name);

	struct atrfs_entry **entries;
	size_t count;
	int i, j;

	int compare_times (void *a, void *b)
	{
		struct atrfs_entry *e1, *e2;
		e1 = *(struct atrfs_entry **)a;
		e2 = *(struct atrfs_entry **)b;
		if (get_dvalue (e1, "user.watchtime",0.0) > get_dvalue (e2, "user.watchtime",0.0))
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

		for (i = 0; i < LIST_SIZE && entries[i]; i++)
		{
			struct atrfs_entry *ent;
			if (j == 0)
				ent = entries[i];
			else
				ent = entries[count - 1 - i];

			double val = get_dvalue (ent, "user.watchtime", 0.0);
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
}

extern void unlink_subdir(fuse_req_t req, struct atrfs_entry *parent, const char *name);

static struct entry_ops subdirops =
{
	.unlink = unlink_subdir,
};

static void move_to_named_subdir (struct atrfs_entry *ent, char *subdir)
{
	struct atrfs_entry *dir = lookup_entry_by_name (root, subdir);
	if (! dir)
	{
		dir = create_entry (ATRFS_DIRECTORY_ENTRY);
		dir->ops = &subdirops;
		attach_entry (root, dir, subdir);
	}
	move_entry (ent, dir);
}

void create_listed_entries (char *list)
{
	/* LIST must be modifiable. Caller frees the LIST. */

	char *s;
	for (s = strtok (list, "\n"); s; s = strtok (NULL, "\n"))
	{
		struct atrfs_entry *ent;
		char *name;
		if (access (s, R_OK))
			continue;
		ent = create_entry (ATRFS_FILE_ENTRY);
		ent->file.e_real_file_name = strdup (s);
		name = uniquify_name (basename (s), root);
		attach_entry (root, ent, name);
		free (name);
	}
}

void categorize_flv_entry (struct atrfs_entry *ent)
{
	CHECK_TYPE (ent, ATRFS_FILE_ENTRY);

	/* Handle file-specific configuration. */
	struct atrfs_entry *conf = NULL;
	int size = getxattr (ent->file.e_real_file_name, "user.mpconf", NULL, 0);
	if (size > 0)
	{
//		tmplog ("File-specific config for %s:%d\n", ent->file.e_real_file_name, size);
		char cfgname[strlen (ent->name) + 6];
		sprintf (cfgname, "%s.conf", ent->name);
		conf = lookup_entry_by_name (ent->parent, cfgname);
		if (! conf)
		{
			conf = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
			conf->virtual.data = malloc (size);
			conf->virtual.size = getxattr (ent->file.e_real_file_name,
				"user.mpconf", conf->virtual.data, size);
			tmplog("Data: '%s'\n", conf->virtual.data);
			attach_entry (ent->parent, conf, cfgname);
		}
	}

	int count = get_ivalue(ent, "user.count", 0);
	double total = get_dvalue(ent, "user.watchtime", 0.0);
	double filelen = get_dvalue(ent, "user.length", 0.0);

	if (count && total > 0.0 && filelen > 0.0)
	{
		char *dir = get_pdir(filelen, count, total);
		move_to_named_subdir (ent, dir);
		if (conf)
			move_to_named_subdir (conf, dir);
	}
}
