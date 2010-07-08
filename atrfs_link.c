#include <errno.h>
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>

#include "entry.h"

extern struct atrfs_entry *statroot;

/*
 * Read symbolic link
 *
 * Valid replies:
 *   fuse_reply_readlink
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 */
void atrfs_readlink(fuse_req_t req, fuse_ino_t ino)
{
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("readlink('%s')\n", ent->name);
	fuse_reply_err(req, ENOSYS);
}

/*
 * Create a hard link
 *
 * Valid replies:
 *   fuse_reply_entry
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the old inode number
 * @param newparent inode number of the new parent directory
 * @param newname new name to create
 */
void atrfs_link(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent, const char *newname)
{
	struct atrfs_entry *ent = ino_to_entry(ino);
	struct atrfs_entry *npent = ino_to_entry(newparent);
	tmplog("link('%s' -> '%s', '%s'\n", ent->name, npent->name, newname);
	fuse_reply_err(req, ENOSYS);
}

/*
 * Create a symbolic link
 *
 * Valid replies:
 *   fuse_reply_entry
 *   fuse_reply_err
 *
 * @param req request handle
 * @param link the contents of the symbolic link
 * @param parent inode number of the parent directory
 * @param name to create
 */
void atrfs_symlink(fuse_req_t req, const char *link, fuse_ino_t parent, const char *name)
{
	tmplog("symlink('%s' -> '%s')\n", link, name);
	fuse_reply_err(req, ENOSYS);
}

/*
 * Remove a file
 *
 * Valid replies:
 *   fuse_reply_err
 *
 * @param req request handle
 * @param parent inode number of the parent directory
 * @param name to remove
 */
void atrfs_unlink(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	int err = ENOENT;
	struct atrfs_entry *pent = ino_to_entry(parent);
	struct atrfs_entry *entry = lookup_entry_by_name (pent, name);
	if (entry)
	{
		if (entry->parent == statroot)
		{
			/* Files under 'stat' cannot be removed. */
			err = EROFS;
		} else if (entry->parent == root) {
			if (entry->e_type == ATRFS_FILE_ENTRY)
			{
				set_ivalue (entry, "count", 0);
				set_dvalue (entry, "watchtime", 0.0);
			}
			err = 0;
		} else {	/* subdir */
			move_entry (entry, root);
			err = 0;
		}
	}
	fuse_reply_err(req, err);
}
