#include <errno.h>
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>

#include "entry.h"

/*
 * Test for a POSIX file lock
 *
 * Introduced in version 2.6
 *
 * Valid replies:
 *   fuse_reply_lock
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 * @param fi file information
 * @param lock the region/type to test
 */
void atrfs_getlk(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct flock *lock)
{
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("getlk('%s')\n", ent->name);
	fuse_reply_err(req, ENOSYS);
}

/*
 * Acquire, modify or release a POSIX file lock
 *
 * For POSIX threads (NPTL) there's a 1-1 relation between pid and
 * owner, but otherwise this is not always the case.  For checking
 * lock ownership, 'fi->owner' must be used.  The l_pid field in
 * 'struct flock' should only be used to fill in this field in
 * getlk().
 *
 * Note: if the locking methods are not implemented, the kernel
 * will still allow file locking to work locally.  Hence these are
 * only interesting for network filesystems and similar.
 *
 * Introduced in version 2.6
 *
 * Valid replies:
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 * @param fi file information
 * @param lock the region/type to test
 * @param sleep locking operation may sleep
 */
void atrfs_setlk(fuse_req_t req, fuse_ino_t ino,
	struct fuse_file_info *fi, struct flock *lock, int sleep)
{
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("setlk('%s')\n", ent->name);
	fuse_reply_err(req, ENOSYS);
}
