/* entry.c - 24.7.2008 - 27.7.2008 Ari & Tero Roponen */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "entry.h"

struct atrfs_entry *root = NULL;

struct atrfs_entry *ino_to_entry(fuse_ino_t ino)
{
	if (ino == 1)
		return root;
	struct atrfs_entry *ent = (struct atrfs_entry *)ino;

	if (ent->flags & ENTRY_DELETED)
		tmplog("Warning: deleted entry accessed\n");

	return ent;
}

struct atrfs_entry *create_entry (enum atrfs_entry_type type)
{
	struct atrfs_entry *ent = malloc (sizeof (*ent));
	if (! ent)
		abort ();

	ent->e_type = type;
	ent->parent = NULL;
	ent->name = NULL;
	ent->flags = 0;

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

void destroy_entry (struct atrfs_entry *ent)
{
	if (! ent)
		abort ();
	switch (ent->e_type)
	{
	default:
		abort ();
	case ATRFS_DIRECTORY_ENTRY:
		if (g_hash_table_size (ent->directory.e_contents) > 0)
			abort ();
		g_hash_table_destroy (ent->directory.e_contents);
		break;
	case ATRFS_FILE_ENTRY:
	case ATRFS_VIRTUAL_FILE_ENTRY:
		break;
	}

	memset(ent, 0, sizeof(*ent));
	free (ent);
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

void detach_entry (struct atrfs_entry *ent)
{
	CHECK_TYPE (ent->parent, ATRFS_DIRECTORY_ENTRY);
	char *name = ent->name;
	if (name)
		g_hash_table_remove (ent->parent->directory.e_contents, name);
	ent->parent->directory.dir_len--;
	ent->parent = NULL;

	free (ent->name);
	ent->name = NULL;
}

void move_entry (struct atrfs_entry *ent, struct atrfs_entry *to)
{
	CHECK_TYPE (to, ATRFS_DIRECTORY_ENTRY);
	struct atrfs_entry *parent = ent->parent;
	char *name = strdup (ent->name);
	detach_entry (ent);
	insert_entry (ent, name, to);
	free (name);

	/* Remove empty directories. */
	while (parent != root && g_hash_table_size (parent->directory.e_contents) == 0)
	{
		struct atrfs_entry *tmp = parent->parent;
		detach_entry (parent);
//		destroy_entry (parent);
		parent->flags |= ENTRY_DELETED;
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
		st->st_gid = getgid();
		st->st_atime = time (NULL);
		st->st_mtime = time (NULL);
		st->st_ctime = time (NULL);
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

/* Call FN for every non-directory entry in tree rooted at ROOT.
   Return the first return value != 0 or 0 when all entries are handled. */
int map_leaf_entries (struct atrfs_entry *root, int (*fn) (struct atrfs_entry *ent))
{
	CHECK_TYPE (root, ATRFS_DIRECTORY_ENTRY);
	int ret = 0;
	GList *p, *entries = g_hash_table_get_values (root->directory.e_contents);
	struct atrfs_entry *ent;

	p = entries;
	while (p)
	{
		ent = p->data;
		switch (ent->e_type)
		{
		default:
			abort ();

		case ATRFS_DIRECTORY_ENTRY:
			/* Recursive call */
			ret = map_leaf_entries (ent, fn);
			if (ret)
				goto out;
			break;

		case ATRFS_VIRTUAL_FILE_ENTRY:
		case ATRFS_FILE_ENTRY:
			ret = fn (ent);
			if (ret)
				goto out;
			break;
		}
		p = p->next;
	}
out:
	g_list_free (entries);
	return ret;
}
