/* util.c - 24.7.2008 - 29.7.2008 Ari & Tero Roponen */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

extern char *asc_read_subtitles (char *ascfile, char *lang);

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
			char *data;

			// Use asc-file contents when possible.
			char *ascname = strdup (file->file.e_real_file_name);
			strcpy (strrchr (ascname, '.'), ".asc");

			data = asc_read_subtitles (ascname, "fi");
			if (! data)
				data = asc_read_subtitles (ascname, "it");
			if (! data)
				data = asc_read_subtitles (ascname, "en");
			if (! data)
				data = asc_read_subtitles (ascname, "la");
			free (ascname);

			if (! data)
			{
				asprintf (&data,
					  "1\n00:00:00,00 --> 00:00:05,00\n"
					  "%.*s\n%s\n\n"
					  "2\n00:00:15.00 --> 00:00:16,00\n15\n\n"
					  "3\n00:00:30.00 --> 00:00:31,00\n30\n\n"
					  "4\n00:00:45.00 --> 00:00:46,00\n45\n\n"
					  "5\n00:01:00.00 --> 00:01:01,00\n1:00\n\n"
					  "6\n00:01:15.00 --> 00:01:16,00\n1:15\n\n"
					  "7\n00:01:30.00 --> 00:01:31,00\n1:30\n\n"
					  "8\n00:01:45.00 --> 00:01:46,00\n1:45\n\n"
					  "9\n00:02:00.00 --> 00:02:01,00\n2:00\n\n"
					  "10\n00:02:15.00 --> 00:02:16,00\n2:15\n\n"
					  "11\n00:02:30.00 --> 00:02:31,00\n2:30\n\n"
					  "12\n00:02:45.00 --> 00:02:46,00\n2:45\n\n"
					  "13\n00:03:00.00 --> 00:03:01,00\n3:00\n\n"
					  "14\n00:03:15.00 --> 00:03:16,00\n3:15\n\n"
					  "15\n00:03:30.00 --> 00:03:31,00\n3:30\n\n"
					  "16\n00:03:45.00 --> 00:03:46,00\n3:45\n\n"
					  "17\n00:04:00.00 --> 00:04:01,00\n4:00\n\n"
					  "18\n00:04:15.00 --> 00:04:16,00\n4:15\n\n"
					  "19\n00:04:30.00 --> 00:04:31,00\n4:30\n\n"
					  "20\n00:04:45.00 --> 00:04:46,00\n4:45\n\n"
					  "21\n00:05:00.00 --> 01:00:00,00\n5:00+\n\n",
					  ext - name, name,
					  secs_to_time (get_value (file, "user.watchtime", 0)));
			}

			ent = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
			ent->virtual.data = data;
			ent->virtual.size = strlen (data);
			insert_entry (ent, srtname, file->parent);
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
	sprintf (buf, "%d%%", ret);
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
