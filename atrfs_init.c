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
extern char *top_name;
extern char *last_name;
extern char *recent_name;

static char *get_group_name(char *fname)
{
	int idx;
	static char gname[256];
	if (sscanf(fname, "%[^_]_%2d.flv", gname, &idx) == 2)
	{
		strcat(gname, ".flv");
		return gname;
	}
	return fname;
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

	add_real_file (ent, filename);
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

static void populate_root_dir (struct atrfs_entry *root, char *datafile)
{
	int categorize_helper (struct atrfs_entry *ent)
	{
		if (check_file_type (ent, ".flv"))
			categorize_flv_entry (ent);
		return 0;
	}

	CHECK_TYPE (root, ATRFS_DIRECTORY_ENTRY);

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

	map_leaf_entries (root, categorize_helper);
	free (datafile);
}

static void populate_stat_dir(struct atrfs_entry *statroot)
{
	struct atrfs_entry *ent;

	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	attach_entry (statroot, ent, top_name);
	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	attach_entry (statroot, ent, last_name);
	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	attach_entry (statroot, ent, recent_name);
	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	attach_entry (statroot, ent, "language");
	update_stats();
}

extern void unlink_root(fuse_req_t req, struct atrfs_entry *parent, const char *name);
extern void unlink_stat(fuse_req_t req, struct atrfs_entry *parent, const char *name);

static struct entry_ops rootops =
{
	.unlink = unlink_root,
};

static struct entry_ops statops =
{
	.unlink = unlink_stat,
};

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
	char *pwd = get_current_dir_name();
	tmplog("init(pwd='%s')\n", pwd);
	free(pwd);

	language_list = strdup("fi, it, en, la\n");
	root = create_entry (ATRFS_DIRECTORY_ENTRY);
	root->ops = &rootops;
	root->name = "/";

	statroot = create_entry (ATRFS_DIRECTORY_ENTRY);
	statroot->ops = &statops;
	attach_entry (root, statroot, "stats");

	populate_root_dir (root, (char *)userdata);
	populate_stat_dir (statroot);
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
