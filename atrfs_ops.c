/* atrfs_ops.c - 28.7.2008 - 28.7.2008 Ari & Tero Roponen */
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>
#include <unistd.h>
#include "entry.h"

/* in main.c */
extern struct atrfs_entry *statroot;
extern void populate_root_dir (struct atrfs_entry *root, char *datafile);
extern void populate_stat_dir (struct atrfs_entry *statroot);

void atrfs_init(void *userdata, struct fuse_conn_info *conn)
{
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
