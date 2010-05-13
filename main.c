/* main.c - 20.7.2008 - 29.7.2008 Ari & Tero Roponen */
#include <sys/inotify.h>
#include <errno.h>
#include <ftw.h>
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <unistd.h>
#include "atrfs_ops.h"
#include "entry.h"
#include "entrydb.h"
#include "util.h"

/* in main.c */
extern struct atrfs_entry *statroot;

extern char *language_list;

struct pollfd pfd[2];
static sigset_t sigs;

static int atrfs_session_loop(struct fuse_session *se)
{
	int res = 0;
	struct fuse_chan *ch = fuse_session_next_chan(se, NULL);
	char buf[fuse_chan_bufsize(ch)];

	sigfillset(&sigs);
	pfd[0].fd = fuse_chan_fd(ch);
	pfd[0].events = POLLIN;

	while (!fuse_session_exited(se))
	{
		int ret = ppoll(pfd, 2, NULL, &sigs);

		if (ret == -1)
		{
		} else if (ret == 0) /* timeout */ {
		} else {
			/* inotify events */
			if (pfd[1].revents)
				handle_notify();

			/* FUSE events */
			if (pfd[0].revents)
			{
				res = fuse_chan_receive(ch, buf, sizeof(buf));
				if (!res)
					continue;

				if (res == -1)
					break;

				fuse_session_process(se, buf, res, ch);
				res = 0;
			}
		}
	}

	fuse_session_reset(se);
	return res;
}

static void add_file_when_flv(const char *filename)
{
	char *uniq_name;
	char *ext = strrchr (filename, '.');
	if (!ext || strcmp (ext, ".flv")) /* Add only flv-files. */
		return;

	uniq_name = uniquify_name(basename(filename), root);

	struct atrfs_entry *ent = lookup_entry_by_name (root, uniq_name);

	if (!ent)
	{
		ent = create_entry (ATRFS_FILE_ENTRY);
		attach_entry (root, ent, uniq_name);
	}

	FILE_ENTRY(ent)->real_path = strdup(filename);
	free(uniq_name);
}

static void for_each_file (char *dir_or_file, void (*file_handler)(const char *filename))
{
	int handler (const char *fpath, const struct stat *sb, int type)
	{
		if (type == FTW_F)
			file_handler (fpath);
		else if (type == FTW_D)
			add_notify(fpath,
				IN_CREATE |
				IN_DELETE |
				IN_MOVED_FROM |
				IN_MOVED_TO);
		return 0;
	}
	ftw (dir_or_file, handler, 10);
}

static void parse_config_file (char *datafile, struct atrfs_entry *root)
{
	/* Root-entry must be initialized. */
	CHECK_TYPE (root, ATRFS_DIRECTORY_ENTRY);

	/* Parse config file. */
	FILE *fp = fopen(datafile, "r");
	if (fp)
	{
		char buf[256]; /* XXX */
		while (fgets (buf, sizeof (buf), fp))
		{
			*strchr(buf, '\n') = '\0';

			if (!*buf || buf[0] == '#')
				continue;

			switch (buf[0])
			{
				case '#': /* Comment */
					continue;
				case '/': /* Path to search files */
					for_each_file (buf, add_file_when_flv);
					continue;
			}

			if (strncmp(buf, "language=", 9) == 0)
			{
				free(language_list);
				language_list = strdup(buf + 9);
			} else if (strncmp (buf, "database=", 9) == 0) {
				close_entrydb ();
				if (! open_entrydb (buf + 9))
					printf ("Can't open %s\n", buf + 9);
			}
		}
	}

	free (datafile);
}

extern char *language_list;

extern int stat_count;
static void write_statcount(struct atrfs_entry *ent, const char *buf, size_t size)
{
	stat_count = atoi(buf);
}

static void write_language(struct atrfs_entry *ent, const char *buf, size_t size)
{
	free(language_list);
	language_list = strndup(buf, size);
}

static void populate_stat_dir(struct atrfs_entry *statroot)
{
	static struct atrfs_entry_ops statcount_ops;
	static struct atrfs_entry_ops language_ops;
	struct atrfs_entry *ent;

	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	attach_entry (statroot, ent, "top-list");
	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	attach_entry (statroot, ent, "last-list");

	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	statcount_ops = *ent->ops;
	statcount_ops.write = write_statcount;
	ent->ops = &statcount_ops;
	attach_entry (statroot, ent, "stat-count");

	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	attach_entry (statroot, ent, "recent");

	ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
	language_ops = *ent->ops;
	language_ops.write = write_language;
	ent->ops = &language_ops;
	attach_entry (statroot, ent, "language");

	update_stats();
}

int main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	char *mountpoint;
	int foreground;
	int err = -1;

	int categorize_flv_helper (struct atrfs_entry *ent)
	{
		if (ent->e_type == ATRFS_FILE_ENTRY)
			categorize_flv_entry (ent);
		return 0;
	}

	char *pwd = get_current_dir_name();
	tmplog("init(pwd='%s')\n", pwd);
	free(pwd);

	language_list = strdup("fi, it, en, la\n");
	root = create_entry (ATRFS_DIRECTORY_ENTRY);
	root->name = "/";

	statroot = create_entry (ATRFS_DIRECTORY_ENTRY);
	attach_entry (root, statroot, "stats");

	parse_config_file (canonicalize_file_name("atrfs.conf"), root);
	populate_stat_dir (statroot);

	/* Put file into right category directory. We assume that
	 * there are only flv-files. */
	map_leaf_entries (root, categorize_flv_helper);

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
					err = atrfs_session_loop(fs);
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
