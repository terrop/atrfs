#include <sys/types.h>
#include <attr/xattr.h>
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>
#include <errno.h>
#include <string.h>
#include "entry.h"
#include "util.h"

static char *get_realname(struct atrfs_entry *ent)
{
	return ent->file.e_real_file_name;
};

static char *get_length(struct atrfs_entry *ent)
{
	return secs_to_time(get_dvalue(ent, "user.length", 0.0));
}

static struct virtual_xattr
{
	char *name;
	char *(*fn)(struct atrfs_entry *ent);
} atrfs_attributes[] = {
	{"user.realname", get_realname},
	{"user.length", get_length},
	{NULL, NULL}
};

/*
 * Set an extended attribute
 *
 * Valid replies:
 *   fuse_reply_err
 */
void atrfs_setxattr(fuse_req_t req, fuse_ino_t ino, const char *name,
	const char *value, size_t size, int flags)
{
	struct atrfs_entry *ent = ino_to_entry(ino);
//	tmplog("setxattr('%s': '%s' = '%.*s'\n", ent->name, name, size, value);
	fuse_reply_err(req, ENOSYS);
}

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
void atrfs_getxattr(fuse_req_t req, fuse_ino_t ino, const char *name, size_t size)
{
	struct atrfs_entry *ent = ino_to_entry(ino);
//	tmplog("getxattr('%s', '%s', size=%lu)\n", ent->name, name, size);

	if (ent->e_type != ATRFS_FILE_ENTRY)
	{
		fuse_reply_err(req, ENOATTR);
		return;
	}

	int i;
	for (i = 0; atrfs_attributes[i].name; i++)
	{
		if (strcmp(name, atrfs_attributes[i].name))
			continue;

		char *ret = atrfs_attributes[i].fn(ent);

		if (size == 0)
		{
			fuse_reply_xattr(req, strlen(ret) + 1);
		} else if (size < strlen(ret) + 1) {
			fuse_reply_err(req, ERANGE);
		} else {
			fuse_reply_buf(req, ret, strlen(ret) + 1);
		}

		return;
	}

	fuse_reply_err(req, ENOATTR);
}

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
void atrfs_listxattr(fuse_req_t req, fuse_ino_t ino, size_t size)
{
	static size_t attr_len;
	if (attr_len == 0)
	{
		int i;
		for (i = 0; atrfs_attributes[i].name; i++)
			attr_len += strlen(atrfs_attributes[i].name) + 1;
	}

	struct atrfs_entry *ent = ino_to_entry(ino);
//	tmplog("listxattr('%s')\n", ent->name);
	if (ent->e_type == ATRFS_FILE_ENTRY)
	{
		if (size == 0)
		{
			fuse_reply_xattr(req, attr_len);
		} else if (size < attr_len) {
			fuse_reply_err(req, ERANGE);
		} else {
			char buf[size];
			int pos = 0;
			int i;
			for (i = 0; atrfs_attributes[i].name; i++)
			{
				int l = strlen(atrfs_attributes[i].name) + 1;
				memcpy(buf + pos, atrfs_attributes[i].name, l);
				pos += l;
			}

			fuse_reply_buf(req, buf, pos);
		}
	} else {
		fuse_reply_err(req, ENOATTR);
	}
}

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
void atrfs_removexattr(fuse_req_t req, fuse_ino_t ino, const char *name)
{
	struct atrfs_entry *ent = ino_to_entry(ino);
//	tmplog("removexattr('%s', '%s')\n", ent->name, name);
	fuse_reply_err(req, ENOTSUP);
}
