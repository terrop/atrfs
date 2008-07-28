/* atrfs_ops.c - 28.7.2008 - 28.7.2008 Ari & Tero Roponen */
#include <errno.h>
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>
#include <stdlib.h>
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

void atrfs_destroy(void *userdata)
{
	/*
	 * Clean up filesystem
	 *
	 * Called on filesystem exit
	 *
	 * There's no reply to this function
	 *
	 * @param userdata the user data passed to fuse_lowlevel_new()
	 */
	tmplog("destroy()\n");
}

void atrfs_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	/*
	 * Look up a directory entry by name and get its attributes.
	 *
	 * Valid replies:
	 *   fuse_reply_entry
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param parent inode number of the parent directory
	 * @param name the name to look up
	 */
	struct atrfs_entry *pent = ino_to_entry(parent);
	struct atrfs_entry *ent = lookup_entry_by_name(pent, name);
	struct fuse_entry_param ep;
	struct stat st;

	tmplog("lookup('%s', '%s')\n", pent->name, name);

	if (!ent)
	{
		fuse_reply_err(req, ENOENT);
		return;
	}

	stat_entry (ent, &st);

	ep.ino = (fuse_ino_t)ent;
	ep.generation = 1;
	ep.attr = st;
	ep.attr_timeout = 1.0;
	ep.entry_timeout = 1.0;

	fuse_reply_entry(req, &ep);
}

void atrfs_forget(fuse_req_t req, fuse_ino_t ino, unsigned long nlookup)
{
	/*
	 * Forget about an inode
	 *
	 * The nlookup parameter indicates the number of lookups
	 * previously performed on this inode.
	 *
	 * If the filesystem implements inode lifetimes, it is recommended
	 * that inodes acquire a single reference on each lookup, and lose
	 * nlookup references on each forget.
	 *
	 * The filesystem may ignore forget calls, if the inodes don't
	 * need to have a limited lifetime.
	 *
	 * On unmount it is not guaranteed, that all referenced inodes
	 * will receive a forget message.
	 *
	 * Valid replies:
	 *   fuse_reply_none
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param nlookup the number of lookups to forget
	 */
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("forget('%s')\n", ent->name);
	fuse_reply_none(req);
}
