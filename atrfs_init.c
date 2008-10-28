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

static int handle_file(const char *fpath, const struct stat *sb, int typeflag)
{
	char *gname;
	char *ext;

	if (typeflag != FTW_F)
		return 0;

	ext = strrchr(fpath, '.');
	if (!ext || strcmp(ext, ".flv"))
		return 0;

	tmplog("%s\n", fpath);
	gname = get_group_name(basename(fpath));
	tmplog("group: '%s'\n", gname);

	struct atrfs_entry *ent = lookup_entry_by_name(root, gname);

	if (!ent)
	{
		ent = create_entry(ATRFS_FILE_ENTRY);
		attach_entry(root, ent, gname);
	}

	add_real_file(ent, fpath);

	return 0;
}

extern char *language_list;

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
					ftw(buf, handle_file, 10);
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
