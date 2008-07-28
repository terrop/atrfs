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

#ifndef LIST_SIZE
#define LIST_SIZE 10
#endif
#define XSTR(s) STR(s)
#define STR(s) #s
static char *top_name = "top-" XSTR(LIST_SIZE);
static char *last_name = "last-" XSTR(LIST_SIZE);
static char *recent_name = "recent";
#undef STR
#undef XSTR

struct atrfs_entry *statroot;

static struct atrfs_entry *recent_files[LIST_SIZE];
static void update_recent_file (struct atrfs_entry *ent)
{
	int i;
	struct atrfs_entry *recent = lookup_entry_by_name (statroot, recent_name);
	if (! recent)
		return;
	CHECK_TYPE (recent, ATRFS_VIRTUAL_FILE_ENTRY);

	if (! ent)
		abort ();
	if (recent_files[0] && ent == recent_files[0])
		return;
	for (i = LIST_SIZE - 1; i > 0; i--)
		recent_files[i] = recent_files[i - 1];
	recent_files[0] = ent;

	char *buf = NULL;
	size_t size;
	FILE *fp = open_memstream (&buf, &size);
	for (i = 0; i < LIST_SIZE && recent_files[i]; i++)
		fprintf (fp, "%s\n", recent_files[i]->name);
	fclose (fp);
	free (recent->virtual.data);
	recent->virtual.data = buf;
	recent->virtual.size = size;
}

void update_stats (void)
{
	struct atrfs_entry *st_ents[2];
	st_ents[0] = lookup_entry_by_name(statroot, top_name);
	st_ents[1] = lookup_entry_by_name(statroot, last_name);

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

		for (i = 0; i < LIST_SIZE && entries[i]; i++)
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

bool check_file_type (struct atrfs_entry *ent, char *ext)
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

void create_listed_entries (char *list)
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
			update_recent_file (ent);
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
	insert_entry (ent, top_name, statroot);
	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	insert_entry (ent, last_name, statroot);
	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	insert_entry (ent, recent_name, statroot);
	update_stats();
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
extern void atrfs_rename(fuse_req_t req, fuse_ino_t parent,
	const char *name, fuse_ino_t newparent, const char *newname);
extern void atrfs_create(fuse_req_t req, fuse_ino_t parent, const char *name,
	mode_t mode, struct fuse_file_info *fi);
extern void atrfs_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
extern void atrfs_read(fuse_req_t req, fuse_ino_t ino, size_t size,
	off_t off, struct fuse_file_info *fi);
extern void atrfs_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
	size_t size, off_t off, struct fuse_file_info *fi);
extern void atrfs_statfs(fuse_req_t req, fuse_ino_t ino);
extern void atrfs_access(fuse_req_t req, fuse_ino_t ino, int mask);
extern void atrfs_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
extern void atrfs_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
	struct fuse_file_info *fi);
extern void atrfs_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);

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
