#include <fuse.h>
#include <fuse/fuse_lowlevel.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "entry.h"

struct directory_data
{
	GList *files;
	GList *cur;
};

static void set_data(struct fuse_file_info *fi, struct directory_data *data)
{
	fi->fh = (unsigned long)data;
}

static struct directory_data *get_data(struct fuse_file_info *fi)
{
	return (struct directory_data *)(unsigned long)fi->fh;
}

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
void atrfs_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	struct atrfs_entry *ent = ino_to_entry(ino);

	if (ent->flags & ENTRY_DELETED)
	{
		fuse_reply_err(req, ENOENT);
		return;
	}

	tmplog("opendir('%s')\n", ent->name);
	struct directory_data *data = malloc(sizeof(*data));
	if (data)
	{
		data->files = g_hash_table_get_keys(DIR_ENTRY(ino_to_entry(ino))->contents);
		data->cur = data->files;
		set_data(fi, data);
		fuse_reply_open(req, fi);
	} else {
		fuse_reply_err(req, ENOMEM);
	}
}

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
void atrfs_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
	struct fuse_file_info *fi)
{
	tmplog("readdir(ino=%lu, size=%lu, off=%lu)\n", ino, size, off);

	struct atrfs_entry *parent = ino_to_entry(ino);
	struct directory_data *data = get_data(fi);
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
void atrfs_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("releasedir('%s')\n", ent->name);
	struct directory_data *data = get_data(fi);
	g_list_free(data->files);
	free(data);
	fuse_reply_err(req, 0);
}

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
void atrfs_fsyncdir(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi)
{
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("fsyncdir('%s')\n", ent->name);
	fuse_reply_err(req, 0);
}

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
void atrfs_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode)
{
	struct atrfs_entry *pent = ino_to_entry(parent);
	tmplog("mkdir('%s', '%s')\n", pent->name, name);
	fuse_reply_err(req, ENOSYS);
}

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
void atrfs_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	struct atrfs_entry *pent = ino_to_entry(parent);
	tmplog("rmdir('%s', '%s')\n", pent->name, name);
	fuse_reply_err(req, ENOSYS);
}
