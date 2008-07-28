#include <sys/types.h>
#include <attr/xattr.h>
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>
#include <errno.h>
#include <string.h>
#include "entry.h"

/* Entries of type ATRFS_FILE_ENTRY have this xattr */
static const char real_name_attr[] = "user.realname";

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
	tmplog("setxattr('%s': '%s' = '%.*s'\n", ent->name, name, size, value);
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
	tmplog("getxattr('%s', '%s', size=%lu)\n", ent->name, name, size);

	if (ent->e_type == ATRFS_FILE_ENTRY)
	{
		if (strcmp(name, real_name_attr))
		{
			fuse_reply_err(req, ENOATTR);
		} else {
			if (size == 0)
			{
				fuse_reply_xattr(req, strlen(ent->file.e_real_file_name) + 1);
			} else if (size < strlen(ent->file.e_real_file_name) + 1) {
				fuse_reply_err(req, ERANGE);
			} else {
				fuse_reply_buf(req, ent->file.e_real_file_name,
					strlen(ent->file.e_real_file_name) + 1);
			}
		}
	} else {
		fuse_reply_err(req, ENOTSUP);
	}
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
	struct atrfs_entry *ent = ino_to_entry(ino);
	tmplog("listxattr('%s')\n", ent->name);
	if (ent->e_type == ATRFS_FILE_ENTRY)
	{
		if (size == 0)
		{
			fuse_reply_xattr(req, sizeof(real_name_attr));
		} else if (size < sizeof(real_name_attr)) {
			fuse_reply_err(req, ERANGE);
		} else {
			fuse_reply_buf(req, real_name_attr, sizeof(real_name_attr));
		}
	} else {
		fuse_reply_err(req, ENOSYS);
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
	tmplog("removexattr('%s', '%s')\n", ent->name, name);
	fuse_reply_err(req, ENOSYS);
}
