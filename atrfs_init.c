#include <sys/inotify.h>
#include <errno.h>
#include <ftw.h>
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "entry.h"
#include "util.h"

/* in main.c */
extern struct atrfs_entry *statroot;

static void add_file_when_flv(const char *filename)
{
	char *uniq_name;
	char *ext = strrchr (filename, '.');
	if (!ext || strcmp (ext, ".flv")) /* Add only flv-files. */
		return;

	uniq_name = uniquify_name(basename(filename), root);

	struct atrfs_entry *ent = lookup_entry_by_name (root, uniq_name);

	if (!ent)
	{
		ent = create_entry (ATRFS_FILE_ENTRY);
		attach_entry (root, ent, uniq_name);
	}

	FILE_ENTRY(ent)->real_path = strdup(filename);
	free(uniq_name);
}

extern char *language_list;

static void for_each_file (char *dir_or_file, void (*file_handler)(const char *filename))
{
	int handler (const char *fpath, const struct stat *sb, int type)
	{
		if (type == FTW_F)
			file_handler (fpath);
		else if (type == FTW_D)
			add_notify(fpath,
				IN_CREATE |
				IN_DELETE |
				IN_MOVED_FROM |
				IN_MOVED_TO);
		return 0;
	}
	ftw (dir_or_file, handler, 10);
}

static void parse_config_file (char *datafile, struct atrfs_entry *root)
{
	/* Root-entry must be initialized. */
	CHECK_TYPE (root, ATRFS_DIRECTORY_ENTRY);

	/* Parse config file. */
	FILE *fp = fopen(datafile, "r");
	if (fp)
	{
		char buf[256]; /* XXX */
		while (fgets (buf, sizeof (buf), fp))
		{
			*strchr(buf, '\n') = '\0';

			if (!*buf || buf[0] == '#')
				continue;

			switch (buf[0])
			{
				case '#': /* Comment */
					continue;
				case '/': /* Path to search files */
					for_each_file (buf, add_file_when_flv);
					continue;
			}

			if (strncmp(buf, "language=", 9) == 0)
			{
				free(language_list);
				language_list = strdup(buf + 9);
			}
		}
	}

	free (datafile);
}

extern int stat_count;
static void write_statcount(struct atrfs_entry *ent, const char *buf, size_t size)
{
	stat_count = atoi(buf);
}

static void write_language(struct atrfs_entry *ent, const char *buf, size_t size)
{
	free(language_list);
	language_list = strndup(buf, size);
}

static struct atrfs_entry_ops statcount_ops;
static struct atrfs_entry_ops language_ops;

static void populate_stat_dir(struct atrfs_entry *statroot)
{
	struct atrfs_entry *ent;

	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	attach_entry (statroot, ent, "top-list");
	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	attach_entry (statroot, ent, "last-list");

	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	statcount_ops = *ent->ops;
	statcount_ops.write = write_statcount;
	ent->ops = &statcount_ops;
	attach_entry (statroot, ent, "stat-count");

	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	attach_entry (statroot, ent, "recent");

	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	language_ops = *ent->ops;
	language_ops.write = write_language;
	ent->ops = &language_ops;
	attach_entry (statroot, ent, "language");

	update_stats();
}

/*
 * Initialize filesystem
 *
 * Called before any other filesystem method
 *
 * There's no reply to this function
 *
 * @param userdata the user data passed to fuse_lowlevel_new()
 *
 */
void atrfs_init(void *userdata, struct fuse_conn_info *conn)
{
	int categorize_flv_helper (struct atrfs_entry *ent)
	{
		if (ent->e_type == ATRFS_FILE_ENTRY)
			categorize_flv_entry (ent);
		return 0;
	}

	char *pwd = get_current_dir_name();
	tmplog("init(pwd='%s')\n", pwd);
	free(pwd);

	language_list = strdup("fi, it, en, la\n");
	root = create_entry (ATRFS_DIRECTORY_ENTRY);
	root->name = "/";

	statroot = create_entry (ATRFS_DIRECTORY_ENTRY);
	attach_entry (root, statroot, "stats");

	parse_config_file ((char *)userdata, root);
	populate_stat_dir (statroot);

	/* Put file into right category directory. We assume that
	 * there are only flv-files. */
	map_leaf_entries (root, categorize_flv_helper);
}

/*
 * Clean up filesystem
 *
 * Called on filesystem exit
 *
 * There's no reply to this function
 *
 * @param userdata the user data passed to fuse_lowlevel_new()
 */
void atrfs_destroy(void *userdata)
{
	tmplog("destroy()\n");
}
