/* main.c - 20.7.2008 - 28.7.2008 Ari & Tero Roponen */
#include <sys/stat.h>
#include <errno.h>
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "entry.h"
#include "util.h"

struct atrfs_entry *statroot;

static void update_stats (void)
{
	struct atrfs_entry *st_ents[2];
	st_ents[0] = lookup_entry_by_name(statroot, "top-10");
	st_ents[1] = lookup_entry_by_name(statroot, "last-10");

	struct atrfs_entry **entries;
	size_t count;
	int i, j;

	int compare_times (void *a, void *b)
	{
		struct atrfs_entry *e1, *e2;
		e1 = *(struct atrfs_entry **)a;
		e2 = *(struct atrfs_entry **)b;
		if (get_value (e1, "user.watchtime", 0) > get_value (e2, "user.watchtime", 0))
			return -1;
		return 1;
	}

	get_all_file_entries (&entries, &count);

	qsort (entries, count, sizeof (struct atrfs_entry *),
	       (comparison_fn_t) compare_times);

	for (j = 0; j < 2; j++)
	{
		char *stbuf = NULL;
		size_t stsize;
		FILE *stfp = open_memstream(&stbuf, &stsize);

		for (i = 0; i < 10 && entries[i]; i++)
		{
			struct atrfs_entry *ent;
			if (j == 0)
				ent = entries[i];
			else
				ent = entries[count - 1 - i];

			int val = get_value (ent, "user.watchtime", 0);
			fprintf (stfp, "%4d\t%s%c%s\n", val,
				ent->parent == root ? "" : ent->parent->name,
				ent->parent == root ? '\0' : '/', ent->name);
		}

		fclose (stfp);
		free (st_ents[j]->virtual.data);
		st_ents[j]->virtual.data = stbuf;
		st_ents[j]->virtual.size = stsize;
	}
		
	free (entries);
}

static bool check_file_type (struct atrfs_entry *ent, char *ext)
{
	CHECK_TYPE (ent, ATRFS_FILE_ENTRY);
	if (! ext)
		abort ();
	char *s = strrchr (ent->file.e_real_file_name, '.');
	return (s && !strcmp (s, ext));
}

void populate_root_dir (struct atrfs_entry *root, char *datafile);
void populate_stat_dir (struct atrfs_entry *statroot);

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
	struct atrfs_entry *pent = ino_to_entry(parent);
	struct atrfs_entry *npent = ino_to_entry(newparent);
	tmplog("rename('%s', '%s' -> '%s', '%s'\n",
		pent->name, name, npent->name, newname);
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

static void create_listed_entries (char *list)
{
	/* LIST must be modifiable. Caller frees the LIST. */

	char *s;
	for (s = strtok (list, "\n"); s; s = strtok (NULL, "\n"))
	{
		struct atrfs_entry *ent;
		char *name;
		if (access (s, R_OK))
			continue;
		ent = create_entry (ATRFS_FILE_ENTRY);
		ent->file.e_real_file_name = strdup (s);
		name = uniquify_name (basename (s), root);
		insert_entry (ent, name, root);
		free (name);
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
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("write('%s', '%.*s')\n", ent->name, size, buf);

	char *list = strdup (buf);
	create_listed_entries (list);
	free (list);

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
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("access('%s')\n", ent->name);
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
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("opendir('%s')\n", ent->name);
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
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("flush('%s')\n", ent->name);
	fuse_reply_err(req, 0);
}

static void categorize_flv_entry (struct atrfs_entry *ent, int new)
{
	CHECK_TYPE (ent, ATRFS_FILE_ENTRY);
	int current = get_value (ent, "user.category", 0);

	if (new > current)
	{
		current = new;
		set_value (ent, "user.category", new);
	}

	/* Handle file-specific configuration. */
	struct atrfs_entry *conf = NULL;
	int size = getxattr (ent->file.e_real_file_name, "user.mpconf", NULL, 0);
	if (size > 0)
	{
//		tmplog ("File-specific config for %s:%d\n", ent->file.e_real_file_name, size);
		char cfgname[strlen (ent->name) + 6];
		sprintf (cfgname, "%s.conf", ent->name);
		conf = lookup_entry_by_name (ent->parent, cfgname);
		if (! conf)
		{
			conf = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
			conf->virtual.data = malloc (size);
			conf->virtual.size = getxattr (ent->file.e_real_file_name,
				"user.mpconf", conf->virtual.data, size);
			tmplog("Data: '%s'\n", conf->virtual.data);
			insert_entry (conf, cfgname, ent->parent);
		}
	}

	if (current > 0)
	{
		char dir[20];
		sprintf (dir, "time_%d", current);
		move_to_named_subdir (ent, dir);
		if (conf)
			move_to_named_subdir (conf, dir);
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

	switch (ent->e_type)
	{
	default:
		abort ();

	case ATRFS_FILE_ENTRY:
		if (ent->file.start_time)
		{
			if (check_file_type (ent, ".flv"))
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
			delta *= 15;
			if (delta > 0 && check_file_type (ent, ".flv"))
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
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("fsync('%s', %d)\n", ent->name, datasync);
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
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("setxattr('%s': '%s' = '%.*s'\n", ent->name, name, size, value);
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
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("getxattr('%s', '%s', size=%lu)\n", ent->name, name, size);
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
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("listxattr('%s')\n", ent->name);
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
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("removexattr('%s', '%s')\n", ent->name, name);
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
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("releasedir('%s')\n", ent->name);
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
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("fsyncdir('%s')\n", ent->name);
	fuse_reply_err(req, 0);
}

void populate_root_dir (struct atrfs_entry *root, char *datafile)
{
	int categorize_helper (struct atrfs_entry *ent)
	{
		if (check_file_type (ent, ".flv"))
			categorize_flv_entry (ent, 0);
		return 0;
	}

	CHECK_TYPE (root, ATRFS_DIRECTORY_ENTRY);

	FILE *fp = fopen (datafile, "r");
	if (fp)
	{
		char buf[256];	/* XXX */
		char *obuf = NULL;
		size_t size = 0;
		FILE *out = open_memstream (&obuf, &size);

		while (fgets (buf, sizeof (buf), fp))
			fprintf (out, "%s", buf);
		fclose (out);
		create_listed_entries (obuf);
		free (obuf);
		fclose (fp);

		map_leaf_entries (root, categorize_helper);
	}
	free (datafile);
}

void populate_stat_dir(struct atrfs_entry *statroot)
{
	struct atrfs_entry *ent;

	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	insert_entry (ent, "top-10", statroot);
	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	insert_entry (ent, "last-10", statroot);
	update_stats();
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
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("getlk('%s')\n", ent->name);
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
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("setlk('%s')\n", ent->name);
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
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("bmap('%s')\n", ent->name);
	fuse_reply_err(req, ENOSYS);
}

extern void atrfs_init(void *userdata, struct fuse_conn_info *conn);
extern void atrfs_destroy(void *userdata);
extern void atrfs_lookup(fuse_req_t req, fuse_ino_t parent, const char *name);
extern void atrfs_forget(fuse_req_t req, fuse_ino_t ino, unsigned long nlookup);
extern void atrfs_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
extern void atrfs_setattr(fuse_req_t req, fuse_ino_t ino,
	struct stat *attr, int to_set, struct fuse_file_info *fi);
extern void atrfs_readlink(fuse_req_t req, fuse_ino_t ino);
extern void atrfs_mknod(fuse_req_t req, fuse_ino_t parent,
	const char *name, mode_t mode, dev_t rdev);
extern void atrfs_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode);
extern void atrfs_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name);
extern void atrfs_link(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent, const char *newname);
extern void atrfs_symlink(fuse_req_t req, const char *link, fuse_ino_t parent, const char *name);
extern void atrfs_unlink(fuse_req_t req, fuse_ino_t parent, const char *name);

int main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
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

	char *mountpoint;
	int foreground;
	int err = -1;

	if (fuse_parse_cmdline(&args, &mountpoint, NULL, &foreground) != -1)
	{
		fuse_opt_add_arg(&args, "-ofsname=atrfs");
		int fd = open(mountpoint, O_RDONLY);
		struct fuse_chan *fc = fuse_mount(mountpoint, &args);
		if (fc)
		{
			struct fuse_session *fs = fuse_lowlevel_new(&args,
				&atrfs_operations,
				sizeof(atrfs_operations),
				canonicalize_file_name("piccolocoro.txt"));

			if (fs)
			{
				if (fuse_set_signal_handlers(fs) != -1)
				{
					fuse_session_add_chan(fs, fc);
					fuse_daemonize(foreground);
					fchdir(fd);
					close(fd);
					err = fuse_session_loop(fs);
					fuse_remove_signal_handlers(fs);
					fuse_session_remove_chan(fc);
				}

				fuse_session_destroy(fs);
			}

			fuse_unmount(mountpoint, fc);
		}
	}

	fuse_opt_free_args(&args);
	return err ? 1 : 0;
}
