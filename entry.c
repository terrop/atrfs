/* entry.c - 24.7.2008 - 1.11.2008 Ari & Tero Roponen */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "entry.h"
#include "util.h"

struct atrfs_entry *root = NULL;

static struct atrfs_entry_ops virtual_ops, file_ops, directory_ops;

struct atrfs_entry *ino_to_entry(fuse_ino_t ino)
{
	struct atrfs_entry *ent = (struct atrfs_entry *)ino;
	if (ino == 1)
		ent = root;

	if (ent->flags & ENTRY_DELETED)
		tmplog("Warning: deleted entry accessed\n");

	return ent;
}

int get_watchcount(struct atrfs_entry *ent)
{
	return get_ivalue (ent, "user.count", 0);
}

double get_watchtime(struct atrfs_entry *ent)
{
	return get_dvalue (ent, "user.watchtime", 0.0);
}

double get_length(struct atrfs_entry *ent)
{
	double value = get_dvalue (ent, "user.length", -1.0);
	if (value < 0.0)
	{
		char buf[256];
		FILE *in;

		snprintf(buf, sizeof(buf),
			"mplayer -identify -frames 0 -ao null -vo null "
			"2>/dev/null -- \"%s\"", REAL_NAME(ent));

		in = popen(buf, "r");
		while (fgets(buf, sizeof(buf), in))
		{
			if (strncmp(buf, "ID_LENGTH=", 10) == 0)
			{
				value = atof(buf + 10);
				break;
			}
		}
		pclose(in);

		set_dvalue (ent, "user.length", value);
	}

	return value;
}

static void set_virtual_contents(struct atrfs_entry *ent, char *str, size_t sz)
{
	VIRTUAL_ENTRY(ent)->m_data = str;
	VIRTUAL_ENTRY(ent)->m_size = sz;
}

static ssize_t virtual_read (struct atrfs_entry *ent, char *buf, size_t size, off_t offset)
{
	ssize_t count = VIRTUAL_ENTRY(ent)->m_size;
	if (offset + size > count)
		size = count - offset;
	memcpy (buf, VIRTUAL_ENTRY(ent)->m_data + offset, size);
	return size;
}

static ssize_t file_read (struct atrfs_entry *ent, char *buf, size_t size, off_t offset)
{
	int ret = pread (FILE_ENTRY(ent)->fd, buf, size, offset);
	return ret;
}

struct atrfs_entry *create_entry (enum atrfs_entry_type type)
{
	struct atrfs_entry *ent;

	switch (type)
	{
	default:
		abort ();
		break;
	case ATRFS_FILE_ENTRY:
	{
		struct atrfs_file_entry *fent = malloc (sizeof (*fent));
		if (! fent)
			abort ();
		fent->fd = -1;
		fent->real_path = NULL;
		fent->start_time = -1.0;
		fent->subtitles = NULL;
		ent = &fent->entry;
		break;
	}
	case ATRFS_VIRTUAL_FILE_ENTRY:
	{
		struct atrfs_virtual_entry *vent = malloc (sizeof (*vent));
		if (! vent)
			abort ();
		vent->set_contents = set_virtual_contents;
		vent->next = NULL;
		ent = &vent->entry;
		VIRTUAL_ENTRY(ent)->set_contents(ent, NULL, 0);
		break;
	}
	case ATRFS_DIRECTORY_ENTRY:
	{
		struct atrfs_directory_entry *dent = malloc (sizeof (*dent));
		if (! dent)
			abort ();
		ent = &dent->entry;
		DIR_ENTRY(ent)->contents = g_hash_table_new (g_str_hash, g_str_equal);
		break;
	}
	}

	ent->e_type = type;
	ent->parent = NULL;
	ent->name = NULL;
	ent->flags = 0;
	ent->ops = NULL;

	switch (type)
	{
	case ATRFS_FILE_ENTRY:
		ent->ops = &file_ops;
		break;
	case ATRFS_VIRTUAL_FILE_ENTRY:
		ent->ops = &virtual_ops;
		break;
	case ATRFS_DIRECTORY_ENTRY:
		ent->ops = &directory_ops;
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
		if (g_hash_table_size (DIR_ENTRY(ent)->contents) > 0)
			abort ();
		g_hash_table_destroy (DIR_ENTRY(ent)->contents);
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
	ASSERT_TYPE (dir, ATRFS_DIRECTORY_ENTRY);
	return g_hash_table_lookup (DIR_ENTRY(dir)->contents, name);
}

/*
 * Attach the given entry to a given directory
 * and give it the specified name.
 */
void attach_entry (struct atrfs_entry *dir, struct atrfs_entry *ent, char *name)
{
	ASSERT_TYPE (dir, ATRFS_DIRECTORY_ENTRY);
	ent->name = strdup (name);
	g_hash_table_replace (DIR_ENTRY(dir)->contents, ent->name, ent);
	ent->parent = dir;
}

/*
 * Detach the given entry from its parent and make
 * it anonymous.
 */
void detach_entry (struct atrfs_entry *ent)
{
	ASSERT_TYPE (ent->parent, ATRFS_DIRECTORY_ENTRY);
	char *name = ent->name;
	if (name)
		g_hash_table_remove (DIR_ENTRY(ent->parent)->contents, name);
	ent->parent = NULL;

	free (ent->name);
	ent->name = NULL;
}

void move_entry (struct atrfs_entry *ent, struct atrfs_entry *to)
{
	ASSERT_TYPE (to, ATRFS_DIRECTORY_ENTRY);
	struct atrfs_entry *parent = ent->parent;
	char *name = strdup (ent->name);
	detach_entry (ent);
	attach_entry (to, ent, name);
	free (name);

	/* Remove empty directories. */
	while (parent != root && g_hash_table_size (DIR_ENTRY(parent)->contents) == 0)
	{
		struct atrfs_entry *tmp = parent->parent;
		detach_entry (parent);
//		destroy_entry (parent);
		parent->flags |= ENTRY_DELETED;
		parent = tmp;
	}
}

static int directory_stat (struct atrfs_entry *ent, struct stat *st)
{
	st->st_ino = (ino_t)(unsigned long)ent;
	st->st_mode = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR;
	st->st_nlink = 1;
	st->st_uid = getuid();
	st->st_gid = getgid();
	st->st_size = g_hash_table_size(DIR_ENTRY(ent)->contents);
	st->st_atime =
	st->st_mtime =
	st->st_ctime = time (NULL);
	return 0;
}

static int virtual_stat (struct atrfs_entry *ent, struct stat *st)
{
	st->st_ino = (ino_t)(unsigned long)ent;
	st->st_nlink = 1;
	st->st_size = VIRTUAL_ENTRY(ent)->m_size;
	st->st_mode = S_IFREG | S_IRUSR;
	st->st_uid = getuid();
	st->st_gid = getgid();
	st->st_atime =
	st->st_mtime =
	st->st_ctime = time (NULL);
	return 0;
}

static int file_stat (struct atrfs_entry *ent, struct stat *st)
{
	char *filename = REAL_NAME(ent);
	if (stat (filename, st) < 0)
		return errno;

	st->st_nlink = get_watchcount (ent);
	/* start at 1.1.2000 */
	st->st_mtime = (time_t)(get_watchtime (ent) + 946677600.0);
	st->st_ino = (ino_t)(unsigned long)ent;
	return 0;
}

/* Call FN for every non-directory entry in tree rooted at ROOT.
   Return the first return value != 0 or 0 when all entries are handled. */
int map_leaf_entries (struct atrfs_entry *root, int (*fn) (struct atrfs_entry *ent))
{
	ASSERT_TYPE (root, ATRFS_DIRECTORY_ENTRY);
	int ret = 0;
	GList *p, *entries = g_hash_table_get_values (DIR_ENTRY(root)->contents);
	struct atrfs_entry *ent;

	for (p = entries; p; p = p->next)
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
	}
out:
	g_list_free (entries);
	return ret;
}


static struct atrfs_entry_ops virtual_ops =
{
	.read = virtual_read,
	.stat = virtual_stat,
};

static struct atrfs_entry_ops file_ops =
{
	.read = file_read,
	.stat = file_stat,
};

static struct atrfs_entry_ops directory_ops =
{
	.stat = directory_stat,
};
