#include <fuse.h>
#include <fuse/fuse_lowlevel.h>

#include "entry.h"

/*
 * Get file attributes
 *
 * Valid replies:
 *   fuse_reply_attr
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 * @param fi for future use, currently always NULL
 */
void atrfs_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	struct stat st;
	struct atrfs_entry *ent = ino_to_entry(ino);

	tmplog("getattr('%s')\n", ent->name);

	int err = stat_entry (ent, &st);
	if (err)
		fuse_reply_err (req, err);
	else
		fuse_reply_attr (req, &st, 0.0);
}

/*
 * Set file attributes
 *
 * In the 'attr' argument only members indicated by the 'to_set'
 * bitmask contain valid values.  Other members contain undefined
 * values.
 *
 * If the setattr was invoked from the ftruncate() system call
 * under Linux kernel versions 2.6.15 or later, the fi->fh will
 * contain the value set by the open method or will be undefined
 * if the open method didn't set any value.  Otherwise (not
 * ftruncate call, or kernel version earlier than 2.6.15) the fi
 * parameter will be NULL.
 *
 * Valid replies:
 *   fuse_reply_attr
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 * @param attr the attributes
 * @param to_set bit mask of attributes which should be set
 * @param fi file information, or NULL
 *
 * Changed in version 2.5:
 *     file information filled in for ftruncate
 */
void atrfs_setattr(fuse_req_t req, fuse_ino_t ino,
	struct stat *attr, int to_set, struct fuse_file_info *fi)
{
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("setattr('%s')\n", ent->name);
	struct stat st;
	stat_entry(ent, &st);
	fuse_reply_attr(req, &st, 0.0);
}
