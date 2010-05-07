/* atrfs_ops.c - 28.7.2008 - 1.11.2008 Ari & Tero Roponen */
#include <errno.h>
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "atrfs_ops.h"
#include "entry.h"
#include "util.h"

extern char *get_real_srt(char *filename);
extern char *get_virtual_srt(char *filename);

/* in statistics.c */
extern struct atrfs_entry *statroot;
extern void update_stats (void);
extern void update_recent_file (struct atrfs_entry *ent);
extern void categorize_flv_entry (struct atrfs_entry *ent);
extern bool check_file_type (struct atrfs_entry *ent, char *ext);

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
void atrfs_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
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
void atrfs_forget(fuse_req_t req, fuse_ino_t ino, unsigned long nlookup)
{
	/*
	 * The entry was already deleted in atrfs_release(),
	 * so we must not try to reference it here.
	 */
	tmplog("forget()\n");
	fuse_reply_none(req);
}

/*
 * Create file node
 *
 * Create a regular file, character device, block device, fifo or
 * socket node.
 *
 * Valid replies:
 *   fuse_reply_entry
 *   fuse_reply_err
 *
 * @param req request handle
 * @param parent inode number of the parent directory
 * @param name to create
 * @param mode file type and mode with which to create the new file
 * @param rdev the device number (only valid if created file is a device)
 */
void atrfs_mknod(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, dev_t rdev)
{
	struct atrfs_entry *pent = ino_to_entry(parent);
	tmplog("mknod('%s', '%s')\n", pent->name, name);
	fuse_reply_err(req, ENOSYS);
}

/*
 * Rename a file
 *
 * Valid replies:
 *   fuse_reply_err
 *
 * @param req request handle
 * @param parent inode number of the old parent directory
 * @param name old name
 * @param newparent inode number of the new parent directory
 * @param newname new name
 */
void atrfs_rename(fuse_req_t req, fuse_ino_t parent,
	const char *name, fuse_ino_t newparent, const char *newname)
{
	struct atrfs_entry *pent = ino_to_entry(parent);
	struct atrfs_entry *npent = ino_to_entry(newparent);
	tmplog("rename('%s', '%s' -> '%s', '%s'\n",
		pent->name, name, npent->name, newname);
	fuse_reply_err(req, ENOSYS);
}

/*
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * Open flags (with the exception of O_NOCTTY) are available in
 * fi->flags.
 *
 * Filesystem may store an arbitrary file handle (pointer, index,
 * etc) in fi->fh, and use this in other all other file operations
 * (read, write, flush, release, fsync).
 *
 * There are also some flags (direct_io, keep_cache) which the
 * filesystem may set in fi, to change the way the file is opened.
 * See fuse_file_info structure in <fuse_common.h> for more details.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 *
 * Valid replies:
 *   fuse_reply_create
 *   fuse_reply_err
 *
 * @param req request handle
 * @param parent inode number of the parent directory
 * @param name to create
 * @param mode file type and mode with which to create the new file
 * @param fi file information
 */
void atrfs_create(fuse_req_t req, fuse_ino_t parent, const char *name,
	mode_t mode, struct fuse_file_info *fi)
{
	struct atrfs_entry *pent = ino_to_entry(parent);
	tmplog("create('%s', '%s')\n", pent->name, name);
	fuse_reply_err(req, ENOSYS);
}

static int open_file(char *cmd, struct atrfs_entry *ent, int flags)
{
	char *filename = FILE_ENTRY(ent)->real_path;

	/*
	 * Increase watch-count every time the 'mplayer' opens
	 * the file and its timer is not already running.
	 */
	if ((!strcmp(cmd, "mplayer") || !strcmp(cmd, "totem")) &&
	    isless(FILE_ENTRY(ent)->start_time, 0.0))
	{
		int count = get_ivalue (filename, "user.count", 0) + 1;
		set_ivalue (ent, "user.count", count);

		/* Add subtitles to flv-files. */
		char *srtname = get_related_name (ent->name, ".flv", ".srt");
		if (srtname)
		{
			if (! lookup_entry_by_name (ent->parent, srtname))
			{
				char *data = get_real_srt(FILE_ENTRY(ent)->real_path);
				if (!data)
					data = get_virtual_srt(FILE_ENTRY(ent)->real_path);

				struct atrfs_entry *srt = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
				VIRTUAL_ENTRY(srt)->set_contents(srt, data, strlen (data));
				attach_entry (ent->parent, srt, srtname);
			}

			free (srtname);
		}

		FILE_ENTRY(ent)->start_time = doubletime ();
	}

	int fd = FILE_ENTRY(ent)->fd;
	if (fd < 0)
	{
		fd = open(filename, flags);
		if (fd < 0)
			fd = -errno;
		else
			FILE_ENTRY(ent)->fd = fd;
	}
	return fd;
}

/*
 * Open a file
 *
 * Open flags (with the exception of O_CREAT, O_EXCL, O_NOCTTY and
 * O_TRUNC) are available in fi->flags.
 *
 * Filesystem may store an arbitrary file handle (pointer, index,
 * etc) in fi->fh, and use this in other all other file operations
 * (read, write, flush, release, fsync).
 *
 * Filesystem may also implement stateless file I/O and not store
 * anything in fi->fh.
 *
 * There are also some flags (direct_io, keep_cache) which the
 * filesystem may set in fi, to change the way the file is opened.
 * See fuse_file_info structure in <fuse_common.h> for more details.
 *
 * Valid replies:
 *   fuse_reply_open
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 * @param fi file information
 *
 * ATR: this will not be called when ino points to a directory.
 */
void atrfs_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	const struct fuse_ctx *ctx = fuse_req_ctx(req);
	struct atrfs_entry *ent = ino_to_entry(ino);
	char *cmd = pid_to_cmdline(ctx->pid);

	tmplog("'%s': open('%s')\n", cmd, ent->name);

	/* Update statistics when needed */
	if (ent->parent == statroot)
		update_stats();

	/*
	 * Don't allow multiple opens of a same file
	 * because it would make accounting inaccurate.
	 */
	if (ent->flags & ENTRY_BUSY)
	{
		fuse_reply_err(req, EBUSY);
		return;
	}

	switch (ent->e_type)
	{
	default:
		fi->fh = -1;
		ent->flags |= ENTRY_BUSY;
		fuse_reply_open(req, fi);
		break;
	case ATRFS_FILE_ENTRY:
	{
		int fd = open_file (cmd, ent, fi->flags);
		if (fd < 0)
		{
			fuse_reply_err (req, -fd);
		} else {
			ent->flags |= ENTRY_BUSY;
			fuse_reply_open (req, fi);
		}
		break;
	}
	case ATRFS_VIRTUAL_FILE_ENTRY:
		fi->fh = -1;
		ent->flags |= ENTRY_BUSY;
		fuse_reply_open(req, fi);
		break;
	}
}

/*
 * Read data
 *
 * Read should send exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the file
 * has been opened in 'direct_io' mode, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * fi->fh will contain the value set by the open method, or will
 * be undefined if the open method didn't set any value.
 *
 * Valid replies:
 *   fuse_reply_buf
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 * @param size number of bytes to read
 * @param off offset to read from
 * @param fi file information
 *
 * ATR: this will not be called when ino points to a directory.
 */
void atrfs_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi)
{
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("read('%s', size=%lu, off=%lu)\n", ent->name, size, off);

	switch (ent->e_type)
	{
	default:
		fuse_reply_err(req, ENOSYS);
		break;

	case ATRFS_FILE_ENTRY:
	case ATRFS_VIRTUAL_FILE_ENTRY:
	{
		char buf[size];
		int ret = ent->ops->read(ent, buf, size, off);
		if (ret < 0)
			fuse_reply_err (req, errno);
		else
			fuse_reply_buf (req, buf, ret);
		break;
	}
	}
}

/*
 * Write data
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the file has
 * been opened in 'direct_io' mode, in which case the return value
 * of the write system call will reflect the return value of this
 * operation.
 *
 * fi->fh will contain the value set by the open method, or will
 * be undefined if the open method didn't set any value.
 *
 * Valid replies:
 *   fuse_reply_write
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 * @param buf data to write
 * @param size number of bytes to write
 * @param off offset to write to
 * @param fi file information
 */

void atrfs_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
	size_t size, off_t off, struct fuse_file_info *fi)
{
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("write('%s', '%.*s')\n", ent->name, size, buf);

	if (ent->ops && ent->ops->write)
	{
		ent->ops->write(buf, size);
		fuse_reply_write(req, size);
	} else {
		fuse_reply_err(req, EROFS);
	}
}

/*
 * Get file system statistics
 *
 * Valid replies:
 *   fuse_reply_statfs
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number, zero means "undefined"
 */
void atrfs_statfs(fuse_req_t req, fuse_ino_t ino)
{
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("statfs('%s')\n", ent->name);
	struct statvfs st;
	st.f_bsize = 1024;	/* file system block size */
	st.f_frsize = 1024;	/* fragment size */
	st.f_blocks = 1000;	/* size of fs in f_frsize units */
	st.f_bfree = 800;	/* # free blocks */
	st.f_bavail = 800;	/* # free blocks for non-root */
	st.f_files = 34;	/* # inodes */
	st.f_ffree = 1000;	/* # free inodes */
	st.f_favail = 1000;	/* # free inodes for non-root */
	st.f_fsid = 342;	/* file system ID */
	st.f_flag = 0;		/* mount flags */
	st.f_namemax = 128;	/* maximum filename length */
	fuse_reply_statfs(req, &st);
}

/*
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 *
 * Valid replies:
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 * @param mask requested access mode
 */
void atrfs_access(fuse_req_t req, fuse_ino_t ino, int mask)
{
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("access('%s')\n", ent->name);

	if (ent->flags & ENTRY_BUSY)
		fuse_reply_err(req, EBUSY);
	else
		fuse_reply_err(req, 0);
}

/*
 * Flush method
 *
 * This is called on each close() of the opened file.
 *
 * Since file descriptors can be duplicated (dup, dup2, fork), for
 * one open call there may be many flush calls.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * fi->fh will contain the value set by the open method, or will
 * be undefined if the open method didn't set any value.
 *
 * NOTE: the name of the method is misleading, since (unlike
 * fsync) the filesystem is not forced to flush pending writes.
 * One reason to flush data, is if the filesystem wants to return
 * write errors.
 *
 * If the filesystem supports file locking operations (setlk,
 * getlk) it should remove all locks belonging to 'fi->owner'.
 *
 * Valid replies:
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 * @param fi file information
 */
void atrfs_flush(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("flush('%s')\n", ent->name);
	fuse_reply_err(req, 0);
}

static void release_file(struct atrfs_entry *ent, double playtime)
{
	/* Remove subtitle file. */
	char *srt_name = get_related_name (ent->name, ".flv", ".srt");

	if (srt_name)
	{
		struct atrfs_entry *srt = lookup_entry_by_name (ent->parent, srt_name);
		if (srt)
		{
			detach_entry (srt);
			free (VIRTUAL_ENTRY(srt)->m_data);
			destroy_entry (srt);
		}
	}

	/* If this was a flv-file, then srt_name != NULL */
	if (srt_name && isgreater (playtime, 0.0))
	{
		char *filename = FILE_ENTRY(ent)->real_path;

		/* * Update the total watch-time. */
		double watchtime = get_dvalue (filename, "user.watchtime", 0.0);
		set_dvalue (ent, "user.watchtime", watchtime + playtime);

		/* * Categorize the file by moving it to a proper subdirectory. */
		categorize_flv_entry (ent);
	}

	free (srt_name);
	update_recent_file (ent);
}

/*
 * Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open call there will be exactly one release call.
 *
 * The filesystem may reply with an error, but error values are
 * not returned to close() or munmap() which triggered the
 * release.
 *
 * fi->fh will contain the value set by the open method, or will
 * be undefined if the open method didn't set any value.
 * fi->flags will contain the same flags as for open.
 *
 * Valid replies:
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 * @param fi file information
 */
void atrfs_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("release('%s')\n", ent->name);

	/* Temporary entries don't have a parent */
	if (ent->parent == NULL)
	{
		if (ent->name)
			tmplog("Warning, temporary node with a name '%s' found\n", ent->name);
		free(ent);
		fuse_reply_err(req, 0);
		return;
	}

	if (ent->e_type == ATRFS_FILE_ENTRY)
	{
		double playtime = 0.0;
		if (isgreaterequal(FILE_ENTRY(ent)->start_time, 0.0))
			playtime = doubletime () - FILE_ENTRY(ent)->start_time;
		FILE_ENTRY(ent)->start_time = -1.0;

		release_file (ent, playtime);
		if (FILE_ENTRY(ent)->fd >= 0)
		{
			close (FILE_ENTRY(ent)->fd);
			FILE_ENTRY(ent)->fd = -1;
		}
	}

	ent->flags &= ~ENTRY_BUSY;
	fuse_reply_err(req, 0);
}

/*
 * Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Valid replies:
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 * @param datasync flag indicating if only data should be flushed
 * @param fi file information
 */
void atrfs_fsync(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi)
{
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("fsync('%s', %d)\n", ent->name, datasync);
	fuse_reply_err(req, ENOSYS);
}

/*
 * Map block index within file to block index within device
 *
 * Note: This makes sense only for block device backed filesystems
 * mounted with the 'blkdev' option
 *
 * Introduced in version 2.6
 *
 * Valid replies:
 *   fuse_reply_bmap
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 * @param blocksize unit of block index
 * @param idx block index within file
 */
void atrfs_bmap(fuse_req_t req, fuse_ino_t ino, size_t blocksize, uint64_t idx)
{
	struct atrfs_entry *ent = ino_to_entry(ino);
	fuse_reply_err(req, ENOSYS);
}

/*
 * Poll for IO readiness
 *
 * Introduced in version 2.8
 *
 * Note: If ph is non-NULL, the client should notify
 * when IO readiness events occur by calling
 * fuse_lowelevel_notify_poll() with the specified ph.
 *
 * Regardless of the number of times poll with a non-NULL ph
 * is received, single notification is enough to clear all.
 * Notifying more times incurs overhead but doesn't harm
 * correctness.
 *
 * The callee is responsible for destroying ph with
 * fuse_pollhandle_destroy() when no longer in use.
 *
 * Valid replies:
 *   fuse_reply_poll
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 * @param fi file information
 * @param ph poll handle to be used for notification
 */
void atrfs_poll(fuse_req_t req, fuse_ino_t ino,
	struct fuse_file_info *fi, struct fuse_pollhandle *ph)
{
	struct atrfs_entry *ent = ino_to_entry(ino);
	fuse_reply_err(req, ENOSYS);
}

struct fuse_lowlevel_ops atrfs_operations =
{
	.init = atrfs_init,
	.destroy = atrfs_destroy,
	.lookup = atrfs_lookup,
	.forget = atrfs_forget,
	.getattr = atrfs_getattr,
	.setattr = atrfs_setattr,
	.readlink = atrfs_readlink,
	.mknod = atrfs_mknod,
	.mkdir = atrfs_mkdir,
	.rmdir = atrfs_rmdir,
	.link = atrfs_link,
	.symlink = atrfs_symlink,
	.unlink = atrfs_unlink,
	.rename = atrfs_rename,
	.create = atrfs_create,
	.open = atrfs_open,
	.read = atrfs_read,
	.write = atrfs_write,
	.statfs = atrfs_statfs,
	.access = atrfs_access,
	.opendir = atrfs_opendir,
	.readdir = atrfs_readdir,
	.releasedir = atrfs_releasedir,
	.fsyncdir = atrfs_fsyncdir,
	.flush = atrfs_flush,
	.release = atrfs_release,
	.fsync = atrfs_fsync,
	.setxattr = atrfs_setxattr,
	.getxattr = atrfs_getxattr,
	.listxattr = atrfs_listxattr,
	.removexattr = atrfs_removexattr,
	.getlk = atrfs_getlk,
	.setlk = atrfs_setlk,
	.bmap = atrfs_bmap,
	.ioctl = atrfs_ioctl,
	.poll = atrfs_poll,
};
