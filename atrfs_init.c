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

static char *get_group_name(char *fname)
{
	static char gname[256];
#if 0
	int idx;
	if (sscanf(fname, "%[^_]_%2d.flv", gname, &idx) == 2)
	{
		strcat(gname, ".flv");
		return gname;
	}
	return fname;
#else
	char *uniq_name = uniquify_name(fname, root);
	strcpy(gname, uniq_name);
	free(uniq_name);
	return gname;
#endif
}

static void add_file_when_flv(const char *filename)
{
	char *groupname;
	char *ext = strrchr (filename, '.');
	if (!ext || strcmp (ext, ".flv")) /* Add only flv-files. */
		return;

	tmplog ("%s\n", filename);
	groupname = get_group_name (basename (filename));
	tmplog ("group: '%s'\n", groupname);

	struct atrfs_entry *ent = lookup_entry_by_name (root, groupname);

	if (!ent)
	{
		ent = create_entry (ATRFS_FILE_ENTRY);
		attach_entry (root, ent, groupname);
	}

	ent->file.real_path = strdup(filename);
}

extern char *language_list;

static void for_each_file (char *dir_or_file, void (*file_handler)(const char *filename))
{
	int handler (const char *fpath, const struct stat *sb, int type)
	{
		if (type == FTW_F)
			file_handler (fpath);
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

static void populate_stat_dir(struct atrfs_entry *statroot)
{
	struct atrfs_entry *ent;

	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	attach_entry (statroot, ent, "top-list");
	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	attach_entry (statroot, ent, "last-list");
	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	attach_entry (statroot, ent, "stat-count");
	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	attach_entry (statroot, ent, "recent");
	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
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
