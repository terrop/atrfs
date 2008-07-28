/* atrfs_ops.c - 28.7.2008 - 28.7.2008 Ari & Tero Roponen */
#include <errno.h>
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "entry.h"
#include "util.h"

/* in main.c */
extern struct atrfs_entry *statroot;
extern void update_stats (void);
extern void create_listed_entries (char *list);
extern bool check_file_type (struct atrfs_entry *ent, char *ext);
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
//	struct atrfs_entry *ent = ino_to_entry(ino);
//	tmplog("forget('%s')\n", ent->name);
	fuse_reply_none(req);
}

void atrfs_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
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
	struct stat st;
	struct atrfs_entry *ent = ino_to_entry(ino);

	tmplog("getattr('%s')\n", ent->name);

	int err = stat_entry (ent, &st);
	if (err)
		fuse_reply_err (req, err);
	else
		fuse_reply_attr (req, &st, 0.0);
}

void atrfs_setattr(fuse_req_t req, fuse_ino_t ino,
	struct stat *attr, int to_set, struct fuse_file_info *fi)
{
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
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("setattr('%s')\n", ent->name);
	fuse_reply_err(req, ENOSYS);
}

void atrfs_readlink(fuse_req_t req, fuse_ino_t ino)
{
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
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("readlink('%s')\n", ent->name);
	fuse_reply_err(req, ENOSYS);
}

void atrfs_mknod(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, dev_t rdev)
{
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
	struct atrfs_entry *pent = ino_to_entry(parent);
	tmplog("mknod('%s', '%s')\n", pent->name, name);
	fuse_reply_err(req, ENOSYS);
}

void atrfs_mkdir(fuse_req_t req, fuse_ino_t parent,
	const char *name, mode_t mode)
{
	/*
	 * Create a directory
	 *
	 * Valid replies:
	 *   fuse_reply_entry
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param parent inode number of the parent directory
	 * @param name to create
	 * @param mode with which to create the new file
	 */
	struct atrfs_entry *pent = ino_to_entry(parent);
	tmplog("mkdir('%s', '%s')\n", pent->name, name);
	fuse_reply_err(req, ENOSYS);
}

void atrfs_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	/*
	 * Remove a directory
	 *
	 * Valid replies:
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param parent inode number of the parent directory
	 * @param name to remove
	 */
	struct atrfs_entry *pent = ino_to_entry(parent);
	tmplog("rmdir('%s', '%s')\n", pent->name, name);
	fuse_reply_err(req, ENOSYS);
}

void atrfs_link(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent, const char *newname)
{
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
	struct atrfs_entry *ent = ino_to_entry(ino);
	struct atrfs_entry *npent = ino_to_entry(newparent);
	tmplog("link('%s' -> '%s', '%s'\n", ent->name, npent->name, newname);
	fuse_reply_err(req, ENOSYS);
}

void atrfs_symlink(fuse_req_t req, const char *link, fuse_ino_t parent, const char *name)
{
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
	tmplog("symlink('%s' -> '%s')\n", link, name);

	struct atrfs_entry *ent= create_entry (ATRFS_FILE_ENTRY);
	ent->file.e_real_file_name = strdup(link);
	name = uniquify_name((char *)name, root);
	insert_entry (ent, (char *)name, root);
	free((char *)name);

	struct fuse_entry_param fep =
	{
		.ino = (fuse_ino_t)ent,
		.generation = 1,
		.attr_timeout = 0.0,
		.entry_timeout = 0.0,
		.attr.st_mode = S_IFLNK,
	};

	fuse_reply_entry(req, &fep);
}

void atrfs_unlink(fuse_req_t req, fuse_ino_t parent, const char *name)
{
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
	struct atrfs_entry *pent = ino_to_entry(parent);
	struct atrfs_entry *ent = lookup_entry_by_name(pent, name);

	tmplog("unlink('%s', '%s')\n", pent->name, name);

	if (!ent)
	{
		fuse_reply_err(req, ENOENT);
		return;
	}

	if (ent->parent == statroot)
	{
		fuse_reply_err(req, EROFS);
		return;
	}

	if (ent->parent != root)
	{
		move_entry (ent, root);
		if (ent->e_type == ATRFS_FILE_ENTRY)
			set_value (ent, "user.category", 0);
	} else if (ent->e_type == ATRFS_FILE_ENTRY) {
		set_value (ent, "user.count", 0);
		set_value (ent, "user.category", 0);
		set_value (ent, "user.watchtime", 0);
	}

	fuse_reply_err(req, 0);
}

void atrfs_rename(fuse_req_t req, fuse_ino_t parent,
	const char *name, fuse_ino_t newparent, const char *newname)
{
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
	struct atrfs_entry *pent = ino_to_entry(parent);
	struct atrfs_entry *npent = ino_to_entry(newparent);
	tmplog("rename('%s', '%s' -> '%s', '%s'\n",
		pent->name, name, npent->name, newname);
	fuse_reply_err(req, ENOSYS);
}

void atrfs_create(fuse_req_t req, fuse_ino_t parent, const char *name,
	mode_t mode, struct fuse_file_info *fi)
{
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
	struct atrfs_entry *pent = ino_to_entry(parent);
	tmplog("create('%s', '%s')\n", pent->name, name);
	char *ext = strrchr(name, '.');
	if (!ext || strcmp(ext, ".txt"))
	{
		fuse_reply_err(req, ENOMSG); /* "No message of desired type" */
		return;
	}

	struct atrfs_entry *ent = create_entry(ATRFS_FILE_ENTRY);
	struct fuse_entry_param fep =
	{
		.ino = (fuse_ino_t)ent,
		.generation = 1,
		.attr.st_mode = mode,
		.attr_timeout = 1.0,
		.entry_timeout = 1.0,
	};

	fuse_reply_create(req, &fep, fi);
}

void atrfs_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
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
	 */
	struct atrfs_entry *ent = (struct atrfs_entry *)ino;
	tmplog("open('%s')\n", ent->name);

	/* Update statistics when needed */
	if (ent->parent == statroot)
		update_stats();

	switch (ent->e_type)
	{
	default:
		break;
	case ATRFS_FILE_ENTRY:
		if (ent->file.start_time == 0)
		{
			ent->file.start_time = time (NULL);
			if (check_file_type (ent, ".flv"))
				handle_srt_for_file (ent, true);
		}
		break;
	case ATRFS_VIRTUAL_FILE_ENTRY:
		break;
	case ATRFS_DIRECTORY_ENTRY:
		abort ();
	}

	fuse_reply_open(req, fi);
}

void atrfs_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi)
{
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
	 */
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("read('%s', size=%lu, off=%lu)\n", ent->name, size, off);

	switch (ent->e_type)
	{
	default:
		abort ();

	case ATRFS_DIRECTORY_ENTRY:
		abort ();

	case ATRFS_VIRTUAL_FILE_ENTRY:
	{
		size_t count = ent->virtual.size;
		if (size < count)
			count = size;
		char buf[count];
		memcpy (buf, ent->virtual.data + off, count);
		fuse_reply_buf(req, buf, sizeof(buf));
		return;
	}

	case ATRFS_FILE_ENTRY:
	{
		char buf[size];
		int ret, fd = open (ent->file.e_real_file_name, O_RDONLY);
		if (fd < 0)
		{
			fuse_reply_err(req, errno);
			return;
		}
		ret = pread (fd, buf, size, off);
		if (ret < 0)
			ret = errno;
		close (fd);

		if (ret < 0)
			fuse_reply_err(req, ret);
		else
			fuse_reply_buf(req, buf, ret);
	}
	}
}

void atrfs_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
	size_t size, off_t off, struct fuse_file_info *fi)
{
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
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("write('%s', '%.*s')\n", ent->name, size, buf);

	char *list = strdup (buf);
	create_listed_entries (list);
	free (list);

	fuse_reply_write(req, size);
}

void atrfs_statfs(fuse_req_t req, fuse_ino_t ino)
{
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

void atrfs_access(fuse_req_t req, fuse_ino_t ino, int mask)
{
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
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("access('%s')\n", ent->name);
	fuse_reply_err(req, 0);
}

void atrfs_flush(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
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
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("flush('%s')\n", ent->name);
	fuse_reply_err(req, 0);
}
