/* util.c - 24.7.2008 - 1.11.2008 Ari & Tero Roponen */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include "entry.h"
#include "entrydb.h"
#include "util.h"

char *get_sha1 (char *filename);

double doubletime(void)
{
	struct timeval tv;
	double ret;
	do
	{
		gettimeofday(&tv, NULL);
		ret = tv.tv_sec + (tv.tv_usec / 10000) / 100.0;
	} while (isnan(ret));
	return ret;
}

static bool get_value_internal (struct atrfs_entry *ent, char *attr, int count, char *fmt, ...)
{
	char *val = entrydb_get (ent, attr);
	if (val)
	{
		va_list list;
		va_start (list, fmt);
		int ret = vsscanf (val, fmt, list);
		va_end (list);
		if (ret != count)
			return false;
		return true;
	}
	return false;
}

static bool set_value_internal (struct atrfs_entry *ent, char *attr, char *fmt, ...)
{
	char *buf = NULL;
	va_list list;
	va_start (list, fmt);
	int ret = vasprintf (&buf, fmt, list);
	va_end (list);
	if (ret < 0)
		return false;

	entrydb_put (ent, attr, buf);
	ret = 0;

	free (buf);
	if (ret == 0)
		return true;
	return false;
}

int get_ivalue (struct atrfs_entry *ent, char *attr, int def)
{
	int value;
	if (get_value_internal (ent, attr, 1, "%d", &value))
		return value;
	return def;
}

double get_dvalue (struct atrfs_entry *ent, char *attr, double def)
{
	double value;
	if (get_value_internal (ent, attr, 1, "%lf", &value))
		return value;
	return def;
}

void set_ivalue (struct atrfs_entry *ent, char *attr, int value)
{
	ASSERT_TYPE (ent, ATRFS_FILE_ENTRY);
	set_value_internal(ent, attr, "%d", value);
}

void set_dvalue (struct atrfs_entry *ent, char *attr, double value)
{
	ASSERT_TYPE (ent, ATRFS_FILE_ENTRY);
	set_value_internal(ent, attr, "%lf", value);
}

char *uniquify_name (char *name, struct atrfs_entry *root)
{
	char *uniq = strdup (name);

	int globally_unique (struct atrfs_entry *ent)
	{
		if (strcmp (uniq, ent->name) == 0)
			return 1;
		return 0;
	}

	int len = strlen (name);
	char *ext = strrchr (name, '.');
	if (! ext)
		ext = name + strlen (name);

	int i;
	for (i = 2;; i++)
	{
		if (map_leaf_entries (root, globally_unique) == 0)
		{
			return uniq;
		} else {
			char buf[len + 10];
			sprintf (buf, "%.*s %d%s", (int)(ext - name), name, i, ext);
			free (uniq);
			uniq = strdup (buf);
		}
	}
}

char *get_related_name (char *filename, char *old_ext, char *new_ext)
{
	char *srtname = NULL;
	char *ext = strrchr (filename, '.');
	if (ext && !strcmp (ext, old_ext))
		asprintf (&srtname, "%.*s%s", (int)(ext - filename), filename, new_ext);
	return srtname;
}

void tmplog(char *fmt, ...)
{
	static FILE *fp;
	va_list list;
	va_start(list, fmt);

	if (!fp)
		fp = fopen("/tmp/loki.txt", "w+");
	if (fp)
	{
		vfprintf(fp, fmt, list);
		fflush(fp);
//		fclose(fp);
	}

	va_end(list);
}

extern bool entrydb_exec (int (*callback)(void *data, int ncols, char **values, char **names)
			  , char *cmdfmt, ...);

extern GHashTable *sha1_to_entry_map;

void get_all_file_entries (struct atrfs_entry ***entries, size_t *count)
{
	if (! entries || ! count)
		abort ();

	struct atrfs_entry **ptr;
	size_t nitems = g_hash_table_size (sha1_to_entry_map);

	int inserter (void *unused, int ncols, char **values, char **names)
	{
		struct atrfs_entry *ent = g_hash_table_lookup (sha1_to_entry_map, values[0]);
		if (ent)
			*ptr++ = ent;
		return 0;
	}

	struct atrfs_entry **ents = malloc ((nitems + 1) * sizeof (struct atrfs_entry *));
	ptr = ents;
	entrydb_exec (inserter, "SELECT sha1 FROM Files ORDER BY watchtime DESC");
	*ptr = NULL;

	*entries = ents;
	*count = nitems;
}

char *secs_to_timestr (double secs)
{
	static int count;
	static char buf[2][10];	/* XXX */
	int min = (int)secs / 60;
	int sec = (int)secs % 60;
	count = (count + 1) % 2;
	sprintf (buf[count], "%02d:%02d", min, sec);
	return buf[count];
}

char *pid_to_cmdline(pid_t pid)
{
	char *ret = NULL;
	static char buf[128]; //XXX
	sprintf(buf, "/proc/%d/cmdline", pid);

	FILE *fp = fopen(buf, "r");
	if (fp)
	{
		ret = fgets(buf, sizeof(buf), fp);
		fclose(fp);
	}

	return ret;
}

