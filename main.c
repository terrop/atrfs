/* main.c - 20.7.2008 - 25.7.2008 Ari & Tero Roponen */
#define FUSE_USE_VERSION 26
#define _GNU_SOURCE
#include <sys/stat.h>
#include <errno.h>
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "entry.h"
#include "util.h"

static char *datafile;
static void populate_root_dir (struct atrfs_entry *root);

static void move_to_named_subdir (struct atrfs_entry *ent, char *subdir)
{
	struct atrfs_entry *dir = lookup_entry_by_name (root, subdir);
	if (! dir)
	{
		dir = create_entry (ATRFS_DIRECTORY_ENTRY);
		insert_entry (dir, subdir, root);
	}
	move_entry (ent, dir);
}

static struct atrfs_entry *ino_to_entry(fuse_ino_t ino)
{
	if (ino == 1)
		return root;
	return (struct atrfs_entry *)ino;
}

static void atrfs_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
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
	tmplog("getattr(ino=%lu)\n", ino);

	struct stat st;
	struct atrfs_entry *ent = ino_to_entry(ino);

	int err = stat_entry (ent, &st);
	if (err)
		fuse_reply_err (req, err);
	else
		fuse_reply_attr (req, &st, 0.0);
}

static void atrfs_setattr(fuse_req_t req, fuse_ino_t ino,
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
	tmplog("setattr(%lu)\n", ino);
	fuse_reply_err(req, ENOSYS);
}

static void atrfs_readlink(fuse_req_t req, fuse_ino_t ino)
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
	tmplog("readlink(%lu)\n", ino);
	fuse_reply_err(req, ENOSYS);
}

static void atrfs_mknod(fuse_req_t req, fuse_ino_t parent,
	const char *name, mode_t mode, dev_t rdev)
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
	tmplog("mknod(%lu, '%s')\n", parent, name);
	fuse_reply_err(req, ENOSYS);
}

static void atrfs_mkdir(fuse_req_t req, fuse_ino_t parent,
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
	tmplog("mkdir(%lu, '%s')\n", parent, name);
	fuse_reply_err(req, ENOSYS);
}

static void atrfs_unlink(fuse_req_t req, fuse_ino_t parent, const char *name)
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
	tmplog("unlink(%lu, '%s'\n", parent, name);

	struct atrfs_entry *pent = ino_to_entry(parent);
	struct atrfs_entry *ent = lookup_entry_by_name(pent, name);

	if (!ent)
	{
		fuse_reply_err(req, ENOENT);
		return;
	}

	if (ent->parent != root)
	{
		move_entry (ent, root);
		set_value (ent, "user.category", 0);
	} else {
		set_value (ent, "user.count", 0);
		set_value (ent, "user.category", 0);
		set_value (ent, "user.watchtime", 0);
	}

	fuse_reply_err(req, 0);
}

static void atrfs_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name)
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
	tmplog("atrfs_rmdir\n");
	fuse_reply_err(req, ENOSYS);
}

static void atrfs_link(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent, const char *newname)
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
	tmplog("link(%lu, %lu, '%s'\n", ino, newparent, newname);
	fuse_reply_err(req, ENOSYS);
}

static void atrfs_symlink(fuse_req_t req, const char *link,
	fuse_ino_t parent, const char *name)
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
	tmplog("symlink('%s' '%s')\n", link, name);

	struct atrfs_entry *ent= create_entry (ATRFS_FILE_ENTRY);
	ent->file.e_real_file_name = strdup(link);
	name = uniquify_in_directory((char *)name, root);
	insert_entry (ent, (char *)name, root);
	free((char *)name);

	struct fuse_entry_param fep =
	{
		.ino = (fuse_ino_t)ent,
		.generation = 1,
		.attr_timeout = 1.0,
		.entry_timeout = 1.0,
	};

	stat_entry(ent, &fep.attr);
	fep.attr.st_mode |= S_IFLNK;

	fuse_reply_entry(req, &fep);
}

static void atrfs_rename(fuse_req_t req, fuse_ino_t parent,
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
	tmplog("atrfs_rename\n");
	fuse_reply_err(req, ENOSYS);
}

static void atrfs_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
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
	tmplog("open(%p)\n", ino);

	switch (ent->e_type)
	{
	default:
		break;
	case ATRFS_FILE_ENTRY:
		if (ent->file.start_time == 0)
		{
			ent->file.start_time = time (NULL);
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

static void atrfs_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
	struct fuse_file_info *fi)
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
	tmplog("read(%lu, size=%lu, off=%lu)\n", ino, size, off);
	struct atrfs_entry *ent = ino_to_entry(ino);

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

static void atrfs_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
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
	tmplog("write(%lu, '%.*s')\n", ino, size, buf);

	/* TODO: refactor this & populate_root_dir.
	   uniquify_globally() */
	char *s, *str = strdup (buf);
	for (s = strtok (str, "\n"); s; s = strtok (NULL, "\n"))
	{
		struct atrfs_entry *ent;
		char *name;
		if (access (s, R_OK))
			continue;
		ent = create_entry (ATRFS_FILE_ENTRY);
		ent->file.e_real_file_name = strdup (s);
		name = uniquify_in_directory (basename (s), root);
		insert_entry (ent, name, root);
		free (name);
	}
	free (str);

	fuse_reply_write(req, size);
}

static void atrfs_statfs(fuse_req_t req, fuse_ino_t ino)
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
	tmplog("statfs(%lu)\n", ino);
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

static void atrfs_access(fuse_req_t req, fuse_ino_t ino, int mask)
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
	tmplog("access(%lu)\n", ino);
	fuse_reply_err(req, 0);
}

struct directory_data
{
	GList *files;
	GList *cur;
};

static void atrfs_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	/*
	 * Open a directory
	 *
	 * Filesystem may store an arbitrary file handle (pointer, index,
	 * etc) in fi->fh, and use this in other all other directory
	 * stream operations (readdir, releasedir, fsyncdir).
	 *
	 * Filesystem may also implement stateless directory I/O and not
	 * store anything in fi->fh, though that makes it impossible to
	 * implement standard conforming directory stream operations in
	 * case the contents of the directory can change between opendir
	 * and releasedir.
	 *
	 * Valid replies:
	 *   fuse_reply_open
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param fi file information
	 */
	tmplog("opendir(%lu)\n", ino);
	struct directory_data *data = malloc(sizeof(*data));
	if (data)
	{
		data->files = g_hash_table_get_keys(ino_to_entry(ino)->directory.e_contents);
		data->cur = data->files;
	}

	fi->fh = (uint32_t)data;
	fuse_reply_open(req, fi);
}

static void atrfs_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
	struct fuse_file_info *fi)
{
	/*
	 * Read directory
	 *
	 * Send a buffer filled using fuse_add_direntry(), with size not
	 * exceeding the requested size.  Send an empty buffer on end of
	 * stream.
	 *
	 * fi->fh will contain the value set by the opendir method, or
	 * will be undefined if the opendir method didn't set any value.
	 *
	 * Valid replies:
	 *   fuse_reply_buf
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param size maximum number of bytes to send
	 * @param off offset to continue reading the directory stream
	 * @param fi file information
	 */
	tmplog("readdir(ino=%lu, size=%lu, off=%lu)\n", ino, size, off);

	struct atrfs_entry *parent = ino_to_entry(ino);
	struct directory_data *data = (struct directory_data *)(uint32_t)fi->fh;
	int err = 0;

	char *buf = malloc(size);
	int pos = 0;

	while (data->cur)
	{
		struct stat st;
		struct atrfs_entry *ent = lookup_entry_by_name(parent, data->cur->data);

		err = stat_entry (ent, &st);
		if (err)
			goto out_err;

		int len = fuse_dirent_size(strlen(data->cur->data));
		if (pos + len > size)
			break;

		fuse_add_direntry(req, buf + pos, size - pos, data->cur->data, &st, ++off);
		pos += len;
		data->cur = data->cur->next;
	}

	if (pos)
	{
		fuse_reply_buf(req, buf, pos);
		free (buf);
	} else {
		fuse_reply_buf(req, NULL, 0);
	}

	return;
out_err:
	if (buf)
		free(buf);
	fuse_reply_err (req, err);
}

static void atrfs_flush(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
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
	tmplog("flush(%lu)\n", ino);
	fuse_reply_err(req, 0);
}

static void categorize_flv_entry (struct atrfs_entry *ent, int new)
{
	CHECK_TYPE (ent, ATRFS_FILE_ENTRY);
	char *ext = strrchr (ent->file.e_real_file_name, '.');
	if (!ext || strcmp (ext, ".flv"))
		return;
	int current = get_value (ent, "user.category", 0);

	if (new > current)
	{
		current = new;
		set_value (ent, "user.category", new);
	}

	if (current > 0)
	{
		char dir[20];
		sprintf (dir, "time_%d", current);
		move_to_named_subdir (ent, dir);
	}
}

static void atrfs_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
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
	tmplog("release(%lu)\n", ino);
	struct atrfs_entry *ent = ino_to_entry(ino);

	switch (ent->e_type)
	{
	default:
		abort ();

	case ATRFS_FILE_ENTRY:
		if (ent->file.start_time)
		{
			handle_srt_for_file (ent, false);
			int delta = time (NULL) - ent->file.start_time;
			int watchtime = get_value (ent, "user.watchtime", 0);
			set_value (ent, "user.watchtime", watchtime + delta);

			if (delta >= 45)
			{
				int val = get_value (ent, "user.count", 0) + 1;
				set_value (ent, "user.count", val);
			}

#if 1
			delta /= 15;
			delta++;
			delta *= 15;
			categorize_flv_entry (ent, delta);
#endif
			ent->file.start_time = 0;
		}
		break;

	case ATRFS_DIRECTORY_ENTRY:
		break;

	case ATRFS_VIRTUAL_FILE_ENTRY:
		break;
	}

	fuse_reply_err(req, 0);
}

static void atrfs_fsync(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi)
{
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
	tmplog("fsync(%lu, %d)\n", ino, datasync);
	fuse_reply_err(req, ENOSYS);
}

static void atrfs_setxattr(fuse_req_t req, fuse_ino_t ino, const char *name,
	const char *value, size_t size, int flags)
{
	/*
	 * Set an extended attribute
	 *
	 * Valid replies:
	 *   fuse_reply_err
	 */
	tmplog("setxattr(%lu, '%s' = '%.*s'\n", ino, name, size, value);
	fuse_reply_err(req, ENOSYS);
}

static void atrfs_getxattr(fuse_req_t req, fuse_ino_t ino, const char *name, size_t size)
{
	/*
	 * Get an extended attribute
	 *
	 * If size is zero, the size of the value should be sent with
	 * fuse_reply_xattr.
	 *
	 * If the size is non-zero, and the value fits in the buffer, the
	 * value should be sent with fuse_reply_buf.
	 *
	 * If the size is too small for the value, the ERANGE error should
	 * be sent.
	 *
	 * Valid replies:
	 *   fuse_reply_buf
	 *   fuse_reply_xattr
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param name of the extended attribute
	 * @param size maximum size of the value to send
	 */
	tmplog("getxattr(%lu, '%s', size=%lu\n", ino, name, size);
	fuse_reply_err(req, ENOTSUP);
}

static void atrfs_listxattr(fuse_req_t req, fuse_ino_t ino, size_t size)
{
	/*
	 * List extended attribute names
	 *
	 * If size is zero, the total size of the attribute list should be
	 * sent with fuse_reply_xattr.
	 *
	 * If the size is non-zero, and the null character separated
	 * attribute list fits in the buffer, the list should be sent with
	 * fuse_reply_buf.
	 *
	 * If the size is too small for the list, the ERANGE error should
	 * be sent.
	 *
	 * Valid replies:
	 *   fuse_reply_buf
	 *   fuse_reply_xattr
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param size maximum size of the list to send
	 */
	tmplog("listxattr(%lu)\n", ino);
	fuse_reply_err(req, ENOSYS);
}

static void atrfs_removexattr(fuse_req_t req, fuse_ino_t ino, const char *name)
{
	/*
	 * Remove an extended attribute
	 *
	 * Valid replies:
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param name of the extended attribute
	 */
	tmplog("removexattr(%lu, '%s')\n", ino, name);
	fuse_reply_err(req, ENOSYS);
}

static void atrfs_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	/*
	 * Release an open directory
	 *
	 * For every opendir call there will be exactly one releasedir
	 * call.
	 *
	 * fi->fh will contain the value set by the opendir method, or
	 * will be undefined if the opendir method didn't set any value.
	 *
	 * Valid replies:
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param fi file information
	 */
	tmplog("atrfs_releasedir\n");
	struct directory_data *data = (struct directory_data *)(uint32_t)fi->fh;
	g_list_free(data->files);
	free(data);
	fuse_reply_err(req, 0);
}

static void atrfs_fsyncdir(fuse_req_t req, fuse_ino_t ino, int datasync,
	struct fuse_file_info *fi)
{
	/*
	 * Synchronize directory contents
	 *
	 * If the datasync parameter is non-zero, then only the directory
	 * contents should be flushed, not the meta data.
	 *
	 * fi->fh will contain the value set by the opendir method, or
	 * will be undefined if the opendir method didn't set any value.
	 *
	 * Valid replies:
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param datasync flag indicating if only data should be flushed
	 * @param fi file information
	 */
	tmplog("atrfs_fsyncdir\n");
	fuse_reply_err(req, 0);
}

static void populate_root_dir (struct atrfs_entry *root)
{
	CHECK_TYPE (root, ATRFS_DIRECTORY_ENTRY);

	FILE *fp = fopen (datafile, "r");
	if (fp)
	{
		char buf[256];
		char *name;

		while (fgets (buf, sizeof (buf), fp))
		{
			struct atrfs_entry *ent;
			*strchr(buf, '\n') = '\0';

			/* We need the read access to the real file */
			if (access(buf, R_OK))
				continue;

			ent = create_entry (ATRFS_FILE_ENTRY);
			ent->file.e_real_file_name = strdup (buf);

			name = uniquify_in_directory (basename (buf), root);
			insert_entry (ent, name, root);
			free (name);

			categorize_flv_entry (ent, 0);
		}

		fclose (fp);
	}
	free (datafile);
	datafile = NULL;
}

static void atrfs_init(void *userdata, struct fuse_conn_info *conn)
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
	tmplog("atrfs_init\n");
	root = create_entry (ATRFS_DIRECTORY_ENTRY);

	populate_root_dir (root);

}

static void atrfs_destroy(void *userdata)
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
}

static void atrfs_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
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
	tmplog("lookup(%lu, '%s')\n", parent, name);
	struct atrfs_entry *pent = ino_to_entry(parent);
	struct atrfs_entry *ent = lookup_entry_by_name(pent, name);
	struct fuse_entry_param ep;
	struct stat st;

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

static void atrfs_forget(fuse_req_t req, fuse_ino_t ino, unsigned long nlookup)
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
	tmplog("atrfs_forget\n");
	fuse_reply_none(req);
}

static void atrfs_create(fuse_req_t req, fuse_ino_t parent, const char *name,
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
	tmplog("create(%lu, '%s')\n", parent, name);
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

static void atrfs_getlk(fuse_req_t req, fuse_ino_t ino,
	struct fuse_file_info *fi, struct flock *lock)
{
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
	tmplog("atrfs_getlk\n");
	fuse_reply_err(req, ENOSYS);
}

static void atrfs_setlk(fuse_req_t req, fuse_ino_t ino,
	struct fuse_file_info *fi, struct flock *lock, int sleep)
{
	/**
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
	tmplog("atrfs_setlk\n");
	fuse_reply_err(req, ENOSYS);
}

static void atrfs_bmap(fuse_req_t req, fuse_ino_t ino, size_t blocksize, uint64_t idx)
{
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
	tmplog("atrfs_bmap\n");
	fuse_reply_err(req, ENOSYS);
}

int main(int argc, char *argv[])
{
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
		.bmap = atrfs_bmap,
	};

	struct fuse_chan *fc;
	struct fuse_session *fs;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	char *mountpoint;
	int multithreaded;
	int foreground;

	datafile = canonicalize_file_name ("piccolocoro.txt");

	fuse_parse_cmdline(&args, &mountpoint, &multithreaded, &foreground);
	fc = fuse_mount(mountpoint, &args);
	if (fc)
	{
		fs = fuse_lowlevel_new(&args,
			&atrfs_operations,
			sizeof(struct fuse_lowlevel_ops),
			NULL);

		if (fs)
		{
			fuse_session_add_chan(fs, fc);
			fuse_daemonize(foreground);
			return fuse_session_loop(fs);
		}
	}

	return 0;
}
