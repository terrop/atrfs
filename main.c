/* main.c - 20.7.2008 - 29.7.2008 Ari & Tero Roponen */
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>
#include <stdlib.h>
#include "atrfs_ops.h"

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
