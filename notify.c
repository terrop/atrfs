#include <sys/inotify.h>
#include <poll.h>

extern struct pollfd pfd[2];
static int notify_fd = -1;

void add_notify(const char *dirname, uint32_t mask)
{
	if (notify_fd < 0)
	{
		notify_fd = inotify_init1(IN_NONBLOCK);
		pfd[1].fd = notify_fd;
		pfd[1].events = POLLIN;
	}

	inotify_add_watch(notify_fd, dirname, mask);
	tmplog("Watching '%s'\n", dirname);
}

void handle_notify(void)
{
	char ibuf[256]; /* XXX */
	int i = 0;
	int len = read(notify_fd, ibuf, sizeof(ibuf));

	while (i < len)
	{
		struct inotify_event *ie = (struct inotify_event *)&ibuf[i];
		i += sizeof(struct inotify_event) + ie->len;

		if (!ie->len)
			continue;

		tmplog("'%s': %s", ie->name, ie->mask & IN_ISDIR ? "dir" : "file");

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
		if (ie->mask & IN_CREATE)
			tmplog(", CREATE");
		if (ie->mask & IN_DELETE)
			tmplog(", DELETE");
		if (ie->mask & IN_DELETE_SELF)
			tmplog(", DELETE_SELF");
		if (ie->mask & IN_MODIFY)
			tmplog(", MODIFY");
		if (ie->mask & IN_MOVE_SELF)
			tmplog(", MOVE_SELF");
		if (ie->mask & IN_MOVED_FROM)
			tmplog(", MOVED_FROM (cookie=%d)", ie->cookie);
		if (ie->mask & IN_MOVED_TO)
			tmplog(", MOVED_TO (cookie=%d)", ie->cookie);
		if (ie->mask & IN_OPEN)
			tmplog(", OPEN");
		tmplog("\n");
	}
}

