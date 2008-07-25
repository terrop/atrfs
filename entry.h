/* entry.h - 24.7.2008 - 25.7.2008 Ari & Tero Roponen */
#ifndef ENTRY_H
#define ENTRY_H
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

struct atrfs_entry
{
	enum atrfs_entry_type e_type;
	struct atrfs_entry *parent;
	char *name;

	union
	{
		struct
		{
			char *e_real_file_name;
			time_t start_time;
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

extern struct atrfs_entry *root;

struct atrfs_entry *create_entry (enum atrfs_entry_type type);

struct atrfs_entry *lookup_entry_by_name (struct atrfs_entry *dir, const char *name);

void insert_entry (struct atrfs_entry *ent, char *name, struct atrfs_entry *dir);
void remove_entry (struct atrfs_entry *ent);
void move_entry (struct atrfs_entry *ent, struct atrfs_entry *to);

int stat_entry (struct atrfs_entry *ent, struct stat *st);

#endif
