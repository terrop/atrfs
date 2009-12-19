/* main.c - 20.7.2008 - 29.7.2008 Ari & Tero Roponen */
#include <sys/inotify.h>
#include <errno.h>
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include "atrfs_ops.h"

struct pollfd pfd[2];
static sigset_t sigs;

static int atrfs_session_loop(struct fuse_session *se)
{
	int res = 0;
	struct fuse_chan *ch = fuse_session_next_chan(se, NULL);
	size_t bufsize = fuse_chan_bufsize(ch);
	char *buf = malloc(bufsize);
	if (!buf)
		return -1;

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
			/* FUSE events */
			if (pfd[0].revents)
			{
				res = fuse_chan_receive(ch, buf, bufsize);
				if (!res)
					goto inotify;

				if (res == -1)
					break;

				fuse_session_process(se, buf, res, ch);
				res = 0;
			}

inotify:
			/* inotify events */
			if (pfd[1].revents)
			{
				char ibuf[256];
				int i = 0;
				int len = read(pfd[1].fd, ibuf, sizeof(ibuf));

				while (i < len)
				{
					struct inotify_event *ie = (struct inotify_event *)&ibuf[i];
					i += sizeof(struct inotify_event) + ie->len;

					if (!ie->len)
						continue;
					tmplog("'%s': %s", ie->name, ie->mask & IN_ISDIR ? "dir" : "file");

#if 0
					if (ie->mask & IN_Q_OVERFLOW)
						tmplog(", Q_OVERFLOW");
					if (ie->mask & IN_UNMOUNT)
						tmplog(", UNMOUNT");
					if (ie->mask & IN_IGNORED)
						tmplog(", IGNORED");
					if (ie->mask & IN_ACCESS)
						tmplog(", ACCESS");
					if (ie->mask & IN_ATTRIB)
						tmplog(", ATTRIB");
					if (ie->mask & IN_CLOSE_WRITE)
						tmplog(", CLOSE_WRITE");
					if (ie->mask & IN_CLOSE_NOWRITE)
						tmplog(", CLOSE_NOWRITE");
#endif
					if (ie->mask & IN_CREATE)
						tmplog(", CREATE");
					if (ie->mask & IN_DELETE)
						tmplog(", DELETE");
#if 0
					if (ie->mask & IN_DELETE_SELF)
						tmplog(", DELETE_SELF");
					if (ie->mask & IN_MODIFY)
						tmplog(", MODIFY");
					if (ie->mask & IN_MOVE_SELF)
						tmplog(", MOVE_SELF");
#endif
					if (ie->mask & IN_MOVED_FROM)
						tmplog(", MOVED_FROM (cookie=%d)", ie->cookie);
					if (ie->mask & IN_MOVED_TO)
						tmplog(", MOVED_TO (cookie=%d)", ie->cookie);
#if 0
					if (ie->mask & IN_OPEN)
						tmplog(", OPEN");
#endif
					tmplog("\n");
				}
			}
		}
	}

	free(buf);
	fuse_session_reset(se);
	return res;
}

int main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

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
					int watch;
					pfd[1].fd = inotify_init1(IN_NONBLOCK);
					pfd[1].events = POLLIN;
					inotify_add_watch(pfd[1].fd, "/home/terrop/Videos/", IN_ALL_EVENTS);

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
