/* entry.h - 24.7.2008 - 1.11.2008 Ari & Tero Roponen */
#ifndef ENTRY_H
#define ENTRY_H
#include <fuse/fuse_lowlevel.h>
#include <glib.h>
#include <sys/stat.h>
#include <time.h>

#define CHECK_TYPE(ent,type) do { if (!(ent) || (ent)->e_type != (type)) abort ();} while(0)
#define VIRTUAL_ENTRY(ent) ((struct atrfs_virtual_entry*)(ent))
#define DIR_ENTRY(ent) ((struct atrfs_directory_entry *)(ent))
#define FILE_ENTRY(ent) ((struct atrfs_file_entry *)(ent))

enum atrfs_entry_type
{
	ATRFS_FILE_ENTRY,
	ATRFS_DIRECTORY_ENTRY,
	ATRFS_VIRTUAL_FILE_ENTRY,
};

struct atrfs_entry;
struct atrfs_entry_ops
{
	ssize_t (*read)(struct atrfs_entry *ent, char *buf, size_t size, off_t offset);
	void (*write)(const char *buf, size_t size);
};

struct atrfs_entry
{
	enum atrfs_entry_type e_type;
	struct atrfs_entry *parent;
	char *name;
	unsigned char flags;

	struct atrfs_entry_ops *ops;
};

struct atrfs_file_entry
{
	struct atrfs_entry entry;
	int fd;
	char *real_path;
	double start_time;
};

struct atrfs_virtual_entry
{
	struct atrfs_entry entry;
	char * m_data;
	size_t m_size;
	void (*set_contents)(struct atrfs_entry *vent, char *str, size_t sz);
};

struct atrfs_directory_entry
{
	struct atrfs_entry entry;
	GHashTable *contents;
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

char *get_real_file_name(struct atrfs_entry *ent);

#endif
