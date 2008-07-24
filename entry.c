/* entry.c - 24.7.2008 - 24.7.2008 Ari & Tero Roponen */
#include <stdlib.h>
#include <string.h>
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

struct atrfs_entry *lookup_entry_by_path (const char *path)
{
	struct atrfs_entry *ent = root;
	char *buf = strdup (path);
	char *s;

	for (s = strtok (buf, "/"); ent && s; s = strtok (NULL, "/"))
		ent = lookup_entry_by_name (ent, s);
	return ent;
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
	char *name = ent->name;
	remove_entry (ent);
	insert_entry (ent, name, to);
	free (name);
}

