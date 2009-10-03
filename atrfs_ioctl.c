#include <errno.h>
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>

#include "entry.h"

/*
 * ioctl
 *
 * Note: For unrestricted ioctls (not allowed for FUSE
 * servers), data in and out areas can be discovered by giving
 * iovs and setting FUSE_IOCTL_RETRY in @flags.  For
 * restricted ioctls, kernel prepares in/out data area
 * according to the information encoded in cmd.
 *
 * Introduced in version 2.8
 *
 * Valid replies:
 *   fuse_reply_ioctl_retry
 *   fuse_reply_ioctl
 *   fuse_reply_ioctl_iov
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 * @param cmd ioctl command
 * @param arg ioctl argument
 * @param fi file information
 * @param flags for FUSE_IOCTL_* flags
 * @param in_buf data fetched from the caller
 * @param in_bufsz number of fetched bytes
 * @param out_bufsz maximum size of output data
 */
void atrfs_ioctl(fuse_req_t req, fuse_ino_t ino, int cmd, void *arg,
	struct fuse_file_info *fi, unsigned flags,
	const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
	struct atrfs_entry *ent = ino_to_entry(ino);
	fuse_reply_err(req, ENOSYS);
}
