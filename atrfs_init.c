#include <errno.h>
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

static void populate_root_dir (struct atrfs_entry *root, char *datafile)
{
	int categorize_helper (struct atrfs_entry *ent)
	{
		if (check_file_type (ent, ".flv"))
			categorize_flv_entry (ent, 0);
		return 0;
	}

	CHECK_TYPE (root, ATRFS_DIRECTORY_ENTRY);

	FILE *fp = fopen (datafile, "r");
	if (fp)
	{
		char buf[256];	/* XXX */
		char *obuf = NULL;
		size_t size = 0;
		FILE *out = open_memstream (&obuf, &size);

		while (fgets (buf, sizeof (buf), fp))
			fprintf (out, "%s", buf);
		fclose (out);
		create_listed_entries (obuf);
		free (obuf);
		fclose (fp);

		map_leaf_entries (root, categorize_helper);
	}
	free (datafile);
}

static void populate_stat_dir(struct atrfs_entry *statroot)
{
	struct atrfs_entry *ent;

	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	insert_entry (ent, top_name, statroot);
	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	insert_entry (ent, last_name, statroot);
	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	insert_entry (ent, recent_name, statroot);
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
	insert_entry (statroot, "stats", root);

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
