/* entry.c - 24.7.2008 - 25.7.2008 Ari & Tero Roponen */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "entry.h"

struct atrfs_entry *root = NULL;

struct atrfs_entry *create_entry (enum atrfs_entry_type type)
{
	struct atrfs_entry *ent = malloc (sizeof (*ent));
	if (! ent)
		abort ();

	ent->e_type = type;
	ent->parent = NULL;
	ent->name = NULL;

	switch (type)
	{
	default:
		abort ();
	case ATRFS_DIRECTORY_ENTRY:
		ent->directory.e_contents = g_hash_table_new (g_str_hash, g_str_equal);
		ent->directory.dir_len = 0;
		break;
	case ATRFS_FILE_ENTRY:
		ent->file.e_real_file_name = NULL;
		ent->file.start_time = 0;
		break;
	case ATRFS_VIRTUAL_FILE_ENTRY:
		ent->virtual.data = NULL;
		ent->virtual.size = 0;
		break;
	}
	return ent;
}

struct atrfs_entry *lookup_entry_by_name (struct atrfs_entry *dir, const char *name)
{
	CHECK_TYPE (dir, ATRFS_DIRECTORY_ENTRY);
	return g_hash_table_lookup (dir->directory.e_contents, name);
}

void insert_entry (struct atrfs_entry *ent, char *name, struct atrfs_entry *dir)
{
	CHECK_TYPE (dir, ATRFS_DIRECTORY_ENTRY);
	ent->name = strdup (name);
	g_hash_table_replace (dir->directory.e_contents, ent->name, ent);
	ent->parent = dir;
	dir->directory.dir_len++;
}

void remove_entry (struct atrfs_entry *ent)
{
	CHECK_TYPE (ent->parent, ATRFS_DIRECTORY_ENTRY);
	char *name = ent->name;
	if (name)
		g_hash_table_remove (ent->parent->directory.e_contents, name);
	ent->parent->directory.dir_len--;
	ent->parent = NULL;
}

void move_entry (struct atrfs_entry *ent, struct atrfs_entry *to)
{
	CHECK_TYPE (to, ATRFS_DIRECTORY_ENTRY);
	struct atrfs_entry *parent = ent->parent;
	char *name = ent->name;
	remove_entry (ent);
	insert_entry (ent, name, to);
	free (name);

	/* Remove empty directories. */
	while (parent != root && g_hash_table_size (parent->directory.e_contents) == 0)
	{
		struct atrfs_entry *tmp = parent->parent;
		remove_entry (parent);
		free (parent->name);
		g_hash_table_destroy (parent->directory.e_contents);
		parent = tmp;
	}
}

int stat_entry (struct atrfs_entry *ent, struct stat *st)
{
	if (! ent)
		abort ();

	switch (ent->e_type)
	{
	default:
		abort ();

	case ATRFS_DIRECTORY_ENTRY:
		st->st_mode = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR;
		st->st_nlink = 1;
		st->st_uid = getuid();
		st->st_gid = getgid();
		st->st_size = ent->directory.dir_len;
		st->st_atime = time (NULL);
		st->st_mtime = time (NULL);
		st->st_ctime = time (NULL);
		break;

	case ATRFS_VIRTUAL_FILE_ENTRY:
		st->st_nlink = 1;
		st->st_size = ent->virtual.size;
		st->st_mode = S_IFREG | S_IRUSR;
		st->st_uid = getuid();
		st->st_mtime = time (NULL);
		break;

	case ATRFS_FILE_ENTRY:
		if (stat (ent->file.e_real_file_name, st) < 0)
			return errno;

		st->st_nlink = get_value (ent, "user.count", 0);
		/* start at 1.1.2000 */
		st->st_mtime = get_value (ent, "user.watchtime", 0) + 946677600;
		st->st_ino = (ino_t)(unsigned int) ent;
		break;
	}

	return 0;
}
