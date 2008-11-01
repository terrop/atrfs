/* util.c - 24.7.2008 - 1.11.2008 Ari & Tero Roponen */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "util.h"

extern char *get_srt(struct atrfs_entry *ent);

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

bool get_value_internal (char *name, char *attr, int count, char *fmt, ...)
{
	int len = getxattr (name, attr, NULL, 0);
	if (len <= 0)
		return false;

	char buf[len];
	if (getxattr (name, attr, buf, len) <= 0)
		return false;

	va_list list;
	va_start (list, fmt);
	int ret = vsscanf (buf, fmt, list);
	va_end (list);

	if (ret != count)
		return false;
	return true;
}

static bool set_value_internal (char *name, char *attr, char *fmt, ...)
{
	char *buf = NULL;
	va_list list;
	va_start (list, fmt);
	int ret = vasprintf (&buf, fmt, list);
	va_end (list);
	if (ret < 0)
		return false;
	ret = setxattr (name, attr, buf, strlen (buf) + 1, 0);
	free (buf);
	if (ret == 0)
		return true;
	return false;
}

int get_ivalue (char *filename, char *attr, int def)
{
	int value;
	if (get_value_internal (filename, attr, 1, "%d", &value))
		return value;
	return def;
}

double get_dvalue (char *filename, char *attr, double def)
{
	double value;
	if (get_value_internal (filename, attr, 1, "%lf", &value))
		return value;
	return def;
}

void set_ivalue (struct atrfs_entry *ent, char *attr, int value)
{
	CHECK_TYPE (ent, ATRFS_FILE_ENTRY);
	set_value_internal (get_real_file_name (ent), attr, "%d", value);
}

void set_dvalue (struct atrfs_entry *ent, char *attr, double value)
{
	CHECK_TYPE (ent, ATRFS_FILE_ENTRY);
	set_value_internal (get_real_file_name (ent), attr, "%lf", value);
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

void handle_srt_for_file (struct atrfs_entry *ent, bool insert)
{
	CHECK_TYPE (ent, ATRFS_FILE_ENTRY);

	char *name = ent->name;

	/* Add/remove subtitles for flv-files. */
	char *ext = strrchr (name, '.');
	if (ext && !strcmp (ext, ".flv"))
	{
		struct atrfs_entry *srt_ent;
		char srtname[strlen (name) + 1];
		sprintf (srtname, "%.*s.srt", ext - name, name);

		if (insert)
		{
			int i;
			char *data = get_srt(ent);

			srt_ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
			srt_ent->virtual.data = data;
			srt_ent->virtual.size = strlen (data);
			attach_entry (ent->parent, srt_ent, srtname);
		} else {
			srt_ent = lookup_entry_by_name (ent->parent, srtname);
			if (srt_ent && srt_ent->e_type == ATRFS_VIRTUAL_FILE_ENTRY)
			{
				detach_entry (srt_ent);
				free (srt_ent->virtual.data);
				destroy_entry (srt_ent);
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

char *get_pdir(double filelen, int watchcount, double watchtime)
{
	static char buf[10]; /* XXX */
	double ret = (watchtime * 100 / watchcount) / filelen;
	sprintf (buf, "%d", (unsigned int)round(ret));
	return buf;
}

char *secs_to_time (double secs)
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

bool check_file_type (struct atrfs_entry *ent, char *ext)
{
	CHECK_TYPE (ent, ATRFS_FILE_ENTRY);
	if (! ext)
		abort ();
	char *s = strrchr (get_real_file_name(ent), '.');
	return (s && !strcmp (s, ext));
}

