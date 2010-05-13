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
#if 0
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
#endif
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
	close_entrydb ();
}
