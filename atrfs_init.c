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

static int handle_file(const char *fpath, const struct stat *sb, int typeflag)
{
	char *ext;

	if (typeflag != FTW_F)
		return 0;

	ext = strrchr(fpath, '.');
	if (!ext || strcmp(ext, ".flv"))
		return 0;

	tmplog("%s\n", fpath);

	struct atrfs_entry *ent = create_entry(ATRFS_FILE_ENTRY);
	if (ent)
	{
		char *name = uniquify_name (basename(fpath), root);
		ent->file.e_real_file_name = strdup(fpath);

		attach_entry (root, ent, name);
		free (name);
	}

	return 0;
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

			ftw(buf, handle_file, 10);
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
	char *pwd = get_current_dir_name();
	tmplog("init(pwd='%s')\n", pwd);
	free(pwd);
	root = create_entry (ATRFS_DIRECTORY_ENTRY);
	root->name = "/";

	statroot = create_entry (ATRFS_DIRECTORY_ENTRY);
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
