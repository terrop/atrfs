/* main.c - 20.7.2008 - 29.7.2008 Ari & Tero Roponen */
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
#include "atrfs_ops.h"
#include "entry.h"
#include "util.h"

#ifndef LIST_SIZE
#define LIST_SIZE 10
#endif
#define XSTR(s) STR(s)
#define STR(s) #s
char *top_name = "top-" XSTR(LIST_SIZE);
char *last_name = "last-" XSTR(LIST_SIZE);
char *recent_name = "recent";
#undef STR
#undef XSTR

struct atrfs_entry *statroot;

static struct atrfs_entry *recent_files[LIST_SIZE];
void update_recent_file (struct atrfs_entry *ent)
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
			fprintf (stfp, "%s\t%s%c%s\n", secs_to_time (val),
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

extern void unlink_subdir(fuse_req_t req, struct atrfs_entry *parent, const char *name);

static void move_to_named_subdir (struct atrfs_entry *ent, char *subdir)
{
	struct atrfs_entry *dir = lookup_entry_by_name (root, subdir);
	if (! dir)
	{
		dir = create_entry (ATRFS_DIRECTORY_ENTRY);
		dir->ops.unlink = unlink_subdir;
		attach_entry (root, dir, subdir);
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
		attach_entry (root, ent, name);
		free (name);
	}
}

void categorize_flv_entry (struct atrfs_entry *ent)
{
	CHECK_TYPE (ent, ATRFS_FILE_ENTRY);

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
			attach_entry (ent->parent, conf, cfgname);
		}
	}

	int count = get_value(ent, "user.count", 0);
	int total = get_value(ent, "user.watchtime", 0);
	int filelen = get_value(ent, "user.length", 0);

	if (count && total && filelen)
	{
		char *dir = get_pdir(filelen, count, total);
		move_to_named_subdir (ent, dir);
		if (conf)
			move_to_named_subdir (conf, dir);
	}
}

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
				canonicalize_file_name("atrfs.conf"));

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
