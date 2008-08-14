/* entry.h - 24.7.2008 - 14.8.2008 Ari & Tero Roponen */
#ifndef ENTRY_H
#define ENTRY_H
#include <fuse/fuse_lowlevel.h>
#include <glib.h>
#include <sys/stat.h>
#include <time.h>

#define CHECK_TYPE(ent,type) do { if (!(ent) || (ent)->e_type != (type)) abort ();} while(0)

enum atrfs_entry_type
{
	ATRFS_FILE_ENTRY,
	ATRFS_DIRECTORY_ENTRY,
	ATRFS_VIRTUAL_FILE_ENTRY,
};

struct atrfs_entry;

struct entry_ops
{
	void (*read)(fuse_req_t req, struct atrfs_entry *ent, size_t size, off_t off);
	void (*release)(fuse_req_t req, struct atrfs_entry *ent, struct fuse_file_info *fi);
	void (*unlink)(fuse_req_t req, struct atrfs_entry *parent, const char *name);
	void (*bmap)(fuse_req_t req, struct atrfs_entry *ent, size_t blocksize, uint64_t idx);
};

struct atrfs_entry
{
	enum atrfs_entry_type e_type;
	struct atrfs_entry *parent;
	char *name;
	unsigned char flags;

	struct entry_ops *ops;

	union
	{
		struct
		{
			char *e_real_file_name;
			double start_time;
		} file;

		struct
		{
			GHashTable *e_contents;
			int dir_len;
		} directory;

		struct
		{
			char *data;
			size_t size;
		} virtual;
	};
};

enum
{
	ENTRY_HIDDEN	= (1<<0),
	ENTRY_DELETED	= (1<<1),
};

extern struct atrfs_entry *root;

struct atrfs_entry *ino_to_entry(fuse_ino_t ino);

struct atrfs_entry *create_entry (enum atrfs_entry_type type);
void destroy_entry (struct atrfs_entry *ent);

struct atrfs_entry *lookup_entry_by_name (struct atrfs_entry *dir, const char *name);
int map_leaf_entries (struct atrfs_entry *root, int (*fn) (struct atrfs_entry *ent));

void attach_entry (struct atrfs_entry *dir, struct atrfs_entry *ent, char *name);
void detach_entry (struct atrfs_entry *ent);
void move_entry (struct atrfs_entry *ent, struct atrfs_entry *to);

int stat_entry (struct atrfs_entry *ent, struct stat *st);

#endif
