/* util.c - 24.7.2008 - 29.7.2008 Ari & Tero Roponen */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

int get_value (struct atrfs_entry *ent, char *attr, int def)
{
	CHECK_TYPE (ent, ATRFS_FILE_ENTRY);
	char *name = ent->file.e_real_file_name;

	int len = getxattr (name, attr, NULL, 0);
	if (len <= 0)
		return def;

	char *tail = NULL;
	char buf[len];
	if (getxattr (name, attr, buf, len) > 0)
	{
		int ret = strtol (buf, &tail, 10);
		if (tail && *tail)
		{
			if (*tail != '.')
				return def;

			/* HACK: Fix length to be an integer: 105.65 => 106 */
			switch (*(tail+1))
			{
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
					set_value(ent, attr, ret);
					break;
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				{
					ret++;
					set_value(ent, attr, ret);
					break;
				}
			}
		}

		return ret;
	}
	return def;
}

void set_value (struct atrfs_entry *ent, char *attr, int value)
{
	CHECK_TYPE (ent, ATRFS_FILE_ENTRY);
	char buf[30];	     /* XXX */
	sprintf (buf, "%d", value);
	setxattr (ent->file.e_real_file_name, attr, buf, strlen (buf) + 1, 0);
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
			sprintf (buf, "%.*s %d%s", ext - name, name, i, ext);
			free (uniq);
			uniq = strdup (buf);
		}
	}
}

void handle_srt_for_file (struct atrfs_entry *file, bool insert)
{
	CHECK_TYPE (file, ATRFS_FILE_ENTRY);

	char *name = file->name;

	/* Add/remove subtitles for flv-files. */
	char *ext = strrchr (name, '.');
	if (ext && !strcmp (ext, ".flv"))
	{
		struct atrfs_entry *ent;
		char srtname[strlen (name) + 1];
		sprintf (srtname, "%.*s.srt", ext - name, name);

		if (insert)
		{
			int i;
			char *data = get_srt(file);

			ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
			ent->virtual.data = data;
			ent->virtual.size = strlen (data);
			attach_entry (file->parent, ent, srtname);
		} else {
			ent = lookup_entry_by_name (file->parent, srtname);
			if (ent && ent->e_type == ATRFS_VIRTUAL_FILE_ENTRY)
			{
				detach_entry (ent);
				free (ent->virtual.data);
				destroy_entry (ent);
			}
		}
	}
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

void get_all_file_entries (struct atrfs_entry ***entries, size_t *count)
{
	if (! entries || ! count)
		abort ();

	size_t nitems;
	struct atrfs_entry **ptr;

	int counter (struct atrfs_entry *ent)
	{
		if (ent->e_type == ATRFS_FILE_ENTRY)
			nitems++;
		return 0;
	}
	int inserter (struct atrfs_entry *ent)
	{
		if (ent->e_type == ATRFS_FILE_ENTRY)
			*ptr++ = ent;
		return 0;
	}

	nitems = 0;
	map_leaf_entries (root, counter);

//	tmplog("get_all_file_entries: %d entries\n", count);

	struct atrfs_entry **ents = malloc ((nitems + 1) * sizeof (struct atrfs_entry *));
	ptr = ents;
	map_leaf_entries (root, inserter);
	*ptr = NULL;

	*entries = ents;
	*count = nitems;
}

char *get_pdir(int filelen, int watchcount, int watchtime)
{
	static char buf[10]; /* XXX */
	int ret = (watchtime * 100 / watchcount) / filelen;
	sprintf (buf, "%d", ret);
	return buf;
}

char *secs_to_time (int secs)
{
	static char buf[10];	/* XXX */
	int min = secs / 60;
	int sec = secs % 60;
	sprintf (buf, "%02d:%02d", min, sec);
	return buf;
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

bool check_file_type (struct atrfs_entry *ent, char *ext)
{
	CHECK_TYPE (ent, ATRFS_FILE_ENTRY);
	if (! ext)
		abort ();
	char *s = strrchr (ent->file.e_real_file_name, '.');
	return (s && !strcmp (s, ext));
}

