/* main.c - 20.7.2008 - 22.7.2008 Ari & Tero Roponen */
#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION 26
#define _GNU_SOURCE
#include <sys/stat.h>
#include <errno.h>
#include <fuse.h>
#include <glib.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *datafile;
static GHashTable *filemap;

struct file_info
{
	char *real_name;
	time_t start_time;
	char *dir;
	bool subdir;
};

static char *bare_name (const char *name)
{
	char *s = strrchr (name, '/');
	return s ? s + 1 : (char *) name;
}

static struct file_info *get_file_info (const char *file)
{
	return g_hash_table_lookup (filemap, file);
}

static int get_value (const char *file, char *attr, int def)
{
	struct file_info *fi = get_file_info (file);
	char *name = fi->real_name;

	int len = getxattr (name, attr, NULL, 0);
	if (len <= 0)
		return def;

	char *tail = NULL;
	char buf[len];
	if (getxattr (name, attr, buf, len) > 0)
	{
		int ret = strtol (buf, &tail, 10);
		if (tail && *tail)
			return def;
		return ret;
	}
	return def;
}

static void set_value (const char *file, char *attr, int value)
{
	struct file_info *fi = get_file_info (file);
	char buf[10];

	sprintf (buf, "%d", value);
	setxattr (fi->real_name, attr, buf, strlen (buf) + 1, 0);
}

static char *insert_as_unique(char *base, struct file_info *fi)
{
	if (!g_hash_table_lookup (filemap, base))
	{
		g_hash_table_replace (filemap, base, fi);
	} else {
		char buf[strlen (base) + 5];
		char *ext = strrchr (base, '.');
		int i;
		if (! ext)
			ext = base + strlen (base);

		for (i = 2;; i++)
		{
			sprintf (buf, "%.*s %d%s", ext - base, base, i, ext);

			if (! g_hash_table_lookup (filemap, buf))
			{
				base = strdup (buf);
				g_hash_table_replace (filemap, base, fi);
				break;
			}
		}
	}
}

static void add_file(char *real_name, bool is_subdir)
{
	if (real_name[0] != '/')
		abort ();

	char *name = bare_name (real_name);

	struct file_info *fi = malloc (sizeof (*fi));
	if (fi)
	{
		fi->real_name = real_name;
		fi->start_time = 0;
		fi->dir = "";	/* Root */
		fi->subdir = is_subdir;

		insert_as_unique (name, fi);
	}
}

static int atrfs_getattr(const char *file, struct stat *st)
{
	/*
	 * Get file attributes.
	 *
	 * Similar to stat(). The 'st_dev' and 'st_blksize' fields are
	 * ignored. The 'st_ino' field is ignored except if the 'use_ino'
	 * mount option is given.
	 */
	char *name = bare_name (file);
	struct file_info *fi = get_file_info (name);

	if (! fi && *name != '\0')
		return -ENOENT;

	bool root = !fi;
	bool subdir = fi && fi->subdir;

	if (root || subdir)
	{
		st->st_mode = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR;
		st->st_nlink = 1;
		st->st_uid = getuid();
		st->st_gid = getgid();
		st->st_size = 34;
		st->st_atime = time(NULL);
		st->st_mtime = time(NULL);
		st->st_ctime = time(NULL);
	} else {
		if (stat (fi->real_name, st) < 0)
			return -errno;

		st->st_nlink = get_value (name, "user.count", 0);
		st->st_mtime = get_value (name, "user.watchtime", 946677600);
	}

	return 0;
}

static int atrfs_readlink(const char *file, char *buf, size_t len)
{
	/*
	 * Read the target of a symbolic link
	 *
	 * The buffer should be filled with a null terminated string. The
	 * buffer size argument includes the space for the terminating
	 * null character. If the linkname is too long to fit in the
	 * buffer, it should be truncated. The return value should be 0
	 * for success.
	 */
	return -ENOSYS;
}

static int atrfs_mknod(const char *file, mode_t mode, dev_t dev)
{
	/*
	 * Create a file node
	 *
	 * This is called for creation of all non-directory, non-symlink
	 * nodes. If the filesystem defines a create() method, then for
	 * regular files that will be called instead.
	 */
	return -ENOSYS;
}

static int atrfs_mkdir(const char *name, mode_t mode)
{
	/* Create a directory */
	return -ENOSYS;
}

static int atrfs_unlink(const char *file)
{
	/* Remove a file */
	char *name = bare_name (file);
	struct file_info *fil = get_file_info (name);
	if (! fil)
		return -ENOENT;

	if (*fil->dir)
	{
		fil->dir = "";
	} else {
		set_value (name, "user.count", 0);
		set_value (name, "user.watchtime", 946677600);
	}
	return 0;
}

static int atrfs_rmdir(const char *name)
{
	/* Remove a directory */
	return -ENOSYS;
}

static int atrfs_link(const char *from, const char *to)
{
	/* Create a hard link to a file */
	return -ENOSYS;
}

static int atrfs_symlink(const char *from, const char *to)
{
	/* Create a symbolic link */
	return -ENOSYS;
}

static int atrfs_rename(const char *from, const char *to)
{
	/* Rename a file */
	return -ENOSYS;
}

static int atrfs_chmod(const char *file, mode_t mode)
{
	/* Change the permission bits of a file */
	return -ENOSYS;
}

static int atrfs_chown(const char *file, uid_t uid, gid_t gid)
{
	/* Change the owner and group of a file */
	return -ENOSYS;
}

static int atrfs_truncate(const char *file, off_t len)
{
	/* Change the size of a file */
	return -ENOSYS;
}

static int atrfs_open(const char *file, struct fuse_file_info *fi)
{
	/*
	 * File open operation
	 *
	 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
	 * will be passed to open().  Open should check if the operation
	 * is permitted for the given flags.  Optionally open may also
	 * return an arbitrary filehandle in the fuse_file_info structure,
	 * which will be passed to all file operations.
	 */
	char *name = bare_name (file);
	struct file_info *fil = get_file_info (name);
	if (! fil)
		return -ENOENT;

	if (fil->start_time == 0)
		fil->start_time = time (NULL);

	return 0;
}

static int atrfs_read(const char *file, char *buf, size_t len,
	off_t off, struct fuse_file_info *fi)
{
	/*
	 * Read data from an open file
	 *
	 * Read should return exactly the number of bytes requested except
	 * on EOF or error, otherwise the rest of the data will be
	 * substituted with zeroes.	 An exception to this is when the
	 * 'direct_io' mount option is specified, in which case the return
	 * value of the read system call will reflect the return value of
	 * this operation.
	 */
	char *name = bare_name (file);
	struct file_info *fil = get_file_info (name);
	if (! fil)
		return 0;

	int fd = open (fil->real_name, O_RDONLY);
	if (fd < 0)
		return -errno;
	int ret = pread (fd, buf, len, off);
	if (ret < 0)
		ret = -errno;
	close (fd);

	return ret;
}

static int atrfs_write(const char *file, const char *buf, size_t len,
	off_t off, struct fuse_file_info *fi)
{
	/*
	 * Write data to an open file
	 *
	 * Write should return exactly the number of bytes requested
	 * except on error.	 An exception to this is when the 'direct_io'
	 * mount option is specified (see read operation).
	 *
	 */
	return -ENOSYS;
}

static int atrfs_statfs(const char *file, struct statvfs *stat)
{
	/*
	 * Get file system statistics
	 *
	 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
	 *
	 */
	return -ENOSYS;
}

static int atrfs_access(const char *file, int acc)
{
	/*
	 * Check file access permissions
	 *
	 * This will be called for the access() system call.  If the
	 * 'default_permissions' mount option is given, this method is not
	 * called.
	 */
	return 0;
}

static int atrfs_opendir(const char *file, struct fuse_file_info *fi)
{
	/*
	 * Open directory
	 *
	 * This method should check if the open operation is permitted for
	 * this directory
	 *
	 */
	return 0;
}

static int atrfs_readdir(const char *file, void *buf,
	fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	/* Read directory */
	static GList *files, *cur;

	if (offset == 0)
	{
		if (files)
			g_list_free (files);
		files = g_hash_table_get_keys (filemap);
		cur = files;
	}

	while (cur)
	{
		char *dir = bare_name(file);
		struct file_info *fil = get_file_info (cur->data);

		/* Fill only items that are in current directory */
		if (! strcmp (fil->dir, dir))
		{
			if (filler (buf, cur->data, NULL, offset + 1) == 1)
				break;
		}

		cur = cur->next;
	}

	return 0;
}

static int atrfs_flush(const char *file, struct fuse_file_info *fi)
{
	/*
	 * Possibly flush cached data
	 *
	 * BIG NOTE: This is not equivalent to fsync().  It's not a
	 * request to sync dirty data.
	 *
	 * Flush is called on each close() of a file descriptor.  So if a
	 * filesystem wants to return write errors in close() and the file
	 * has cached dirty data, this is a good place to write back data
	 * and return any errors.  Since many applications ignore close()
	 * errors this is not always useful.
	 *
	 * NOTE: The flush() method may be called more than once for each
	 * open().	This happens if more than one file descriptor refers
	 * to an opened file due to dup(), dup2() or fork() calls.	It is
	 * not possible to determine if a flush is final, so each flush
	 * should be treated equally.  Multiple write-flush sequences are
	 * relatively rare, so this shouldn't be a problem.
	 *
	 * Filesystems shouldn't assume that flush will always be called
	 * after some writes, or that if will be called at all.
	 */
	return -ENOSYS;
}

static int atrfs_release(const char *file, struct fuse_file_info *fi)
{
	/*
	 * Release an open file
	 *
	 * Release is called when there are no more references to an open
	 * file: all file descriptors are closed and all memory mappings
	 * are unmapped.
	 *
	 * For every open() call there will be exactly one release() call
	 * with the same flags and file descriptor.	 It is possible to
	 * have a file opened more than once, in which case only the last
	 * release will mean, that no more reads/writes will happen on the
	 * file.  The return value of release is ignored.
	 */
	char *name = bare_name (file);
	struct file_info *fil = get_file_info (name);
	if (! fil)
		return 0;

	if (fil->start_time)
	{
		int delta = time (NULL) - fil->start_time;
		int watchtime = get_value (name, "user.watchtime", 946677600);
		set_value (name, "user.watchtime", watchtime + delta);

		if (delta >= 45)
		{
			int val = get_value (name, "user.count", 0) + 1;
			set_value (name, "user.count", val);
		}

		delta /= 15;
		delta++;
		delta *= 15;
		char buf[20];
		sprintf (buf, "/time_%d", delta);
		char *n = strdup (buf);

		if (! get_file_info (n + 1))
			add_file (n, true);
		fil->dir = n+1;

		fil->start_time = 0;
	}

	return 0;
}

static int atrfs_fsync(const char *file, int datasync, struct fuse_file_info *fi)
{
	/*
	 * Synchronize file contents
	 *
	 * If the datasync parameter is non-zero, then only the user data
	 * should be flushed, not the meta data.
	 */
	return -ENOSYS;
}

static int atrfs_setxattr(const char *file, const char *name,
	const char *value, size_t size, int flags)
{
	/* Set extended attributes */
	return -ENOSYS;
}

static int atrfs_getxattr(const char *file, const char *name, char *buf, size_t len)
{
	/* Get extended attributes */
	return -ENOSYS;
}

static int atrfs_listxattr(const char *file, char *buf, size_t len)
{
	/* List extended attributes */
	return -ENOSYS;
}

static int atrfs_removexattr(const char *file, const char *name)
{
	/* Remove extended attributes */
	return -ENOSYS;
}

static int atrfs_releasedir(const char *file, struct fuse_file_info *fi)
{
	/* Release directory */
	return -ENOSYS;
}

static int atrfs_fsyncdir(const char *file, int datasync, struct fuse_file_info *fi)
{
	/*
	 * Synchronize directory contents
	 *
	 * If the datasync parameter is non-zero, then only the user data
	 * should be flushed, not the meta data
	 */
	return -ENOSYS;
}

static void *atrfs_init(struct fuse_conn_info *conn)
{
	/*
	 * Initialize filesystem
	 *
	 * The return value will passed in the private_data field of
	 * fuse_context to all file operations and as a parameter to the
	 * destroy() method.
	 *
	 */

	filemap = g_hash_table_new (g_str_hash, g_str_equal);

	add_file ("/.", true);
	add_file ("/..", true);

	FILE *fp = fopen (datafile, "r");
	if (fp)
	{
		char buf[256];

		while (fgets (buf, sizeof (buf), fp))
		{
			*strchr(buf, '\n') = '\0';
			add_file (canonicalize_file_name (buf), false);
		}

		fclose (fp);
	}
	free (datafile);
	datafile = NULL;

	return filemap;
}

static void atrfs_destroy(void *data)
{
	/*
	 * Clean up filesystem
	 *
	 * Called on filesystem exit.
	 */
	GHashTable *ht = data;
	if (ht)
		g_hash_table_destroy (ht);
}

static int atrfs_create(const char *file, mode_t mode, struct fuse_file_info *fi)
{
	/*
	 * Create and open a file
	 *
	 * If the file does not exist, first create it with the specified
	 * mode, and then open it.
	 *
	 * If this method is not implemented or under Linux kernel
	 * versions earlier than 2.6.15, the mknod() and open() methods
	 * will be called instead.
	 */
	return -ENOSYS;
}

static int atrfs_ftruncate(const char *file, off_t len, struct fuse_file_info *fi)
{
	/*
	 * Change the size of an open file
	 *
	 * This method is called instead of the truncate() method if the
	 * truncation was invoked from an ftruncate() system call.
	 *
	 * If this method is not implemented or under Linux kernel
	 * versions earlier than 2.6.15, the truncate() method will be
	 * called instead.
	 *
	 * Introduced in version 2.5
	 */
	return -ENOSYS;
}

static int atrfs_fgetattr(const char *file, struct stat *st, struct fuse_file_info *fi)
{
	/*
	 * Get attributes from an open file
	 *
	 * This method is called instead of the getattr() method if the
	 * file information is available.
	 *
	 * Currently this is only called after the create() method if that
	 * is implemented (see above).  Later it may be called for
	 * invocations of fstat() too.
	 */
	return -ENOSYS;
}

static int atrfs_lock(const char *file, struct fuse_file_info *fi, int cmd, struct flock *fl)
{
	/*
	 * Perform POSIX file locking operation
	 *
	 * The cmd argument will be either F_GETLK, F_SETLK or F_SETLKW.
	 *
	 * For the meaning of fields in 'struct flock' see the man page
	 * for fcntl(2).  The l_whence field will always be set to
	 * SEEK_SET.
	 *
	 * For checking lock ownership, the 'fuse_file_info->owner'
	 * argument must be used.
	 *
	 * For F_GETLK operation, the library will first check currently
	 * held locks, and if a conflicting lock is found it will return
	 * information without calling this method.	 This ensures, that
	 * for local locks the l_pid field is correctly filled in.	The
	 * results may not be accurate in case of race conditions and in
	 * the presence of hard links, but it's unlikly that an
	 * application would rely on accurate GETLK results in these
	 * cases.  If a conflicting lock is not found, this method will be
	 * called, and the filesystem may fill out l_pid by a meaningful
	 * value, or it may leave this field zero.
	 *
	 * For F_SETLK and F_SETLKW the l_pid field will be set to the pid
	 * of the process performing the locking operation.
	 *
	 * Note: if this method is not implemented, the kernel will still
	 * allow file locking to work locally.  Hence it is only
	 * interesting for network filesystems and similar.
	 */
	return -ENOSYS;
}

static int atrfs_utimens(const char *file, const struct timespec tv[2])
{
	/*
	 * Change the access and modification times of a file with
	 * nanosecond resolution
	 *
	 * Introduced in version 2.6
	 */
	return -ENOSYS;
}

static int atrfs_bmap(const char *file, size_t blocksize, uint64_t *idx)
{
	/*
	 * Map block index within file to block index within device
	 *
	 * Note: This makes sense only for block device backed filesystems
	 * mounted with the 'blkdev' option
	 */
	return -ENOSYS;
}

int main(int argc, char *argv[])
{
	struct fuse_operations atrfs_operations =
	{
		.init = atrfs_init,
		.destroy = atrfs_destroy,
		.getattr = atrfs_getattr,
		.fgetattr = atrfs_fgetattr,
		.readlink = atrfs_readlink,
		.mknod = atrfs_mknod,
		.mkdir = atrfs_mkdir,
		.rmdir = atrfs_rmdir,
		.link = atrfs_link,
		.symlink = atrfs_symlink,
		.unlink = atrfs_unlink,
		.rename = atrfs_rename,
		.chmod = atrfs_chmod,
		.chown = atrfs_chown,
		.truncate = atrfs_truncate,
		.ftruncate = atrfs_ftruncate,
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
		.lock = atrfs_lock,
		.utimens = atrfs_utimens,
		.bmap = atrfs_bmap,
	};

	datafile = canonicalize_file_name ("piccolocoro.txt");
	return fuse_main (argc, argv, &atrfs_operations, NULL);
}
