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
