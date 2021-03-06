#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "entry.h"
#include "subtitles.h"
#include "util.h"

extern char *get_real_srt(char *filename, double watchtime, double length, char *lang);
extern char *get_virtual_srt(char *title, double watchtime, double length, char *lang);

char *language_list;

void attach_subtitles (struct atrfs_entry *ent)
{
	char *lng = strdup (language_list);
	char *ext, *s, *saved;

	double watchtime = get_watchtime (ent);
	double length = get_length (ent);
	char *srtname, *data;
	int num = 1;
	char buf[40];

	ext = strrchr (ent->name, '.');
	if (! ext)
		return;

	/* Attach real subtitles, */
	for (s = strtok_r (lng, ", \n", &saved); s; s = strtok_r (NULL, ", \n", &saved))
	{
		snprintf(buf, sizeof(buf), "_%d.%s.srt", num, s);

		char *srtname = get_related_name (ent->name, ext, buf);
		if (srtname && ! lookup_entry_by_name (ent->parent, srtname))
		{
			data = get_real_srt(REAL_NAME(ent), watchtime, length, s);
			if (data)
			{
				struct atrfs_entry *srt = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
				VIRTUAL_ENTRY(srt)->set_contents (srt, data, strlen (data));

				VIRTUAL_ENTRY(srt)->next = FILE_ENTRY(ent)->subtitles;
				FILE_ENTRY(ent)->subtitles = srt;

				attach_entry (ent->parent, srt, srtname);
				num++;
			}
		}
		free (srtname);
	}

	/* Attach virtual subtitles */
	snprintf(buf, sizeof(buf), "_%d.virt.srt", num);
	srtname = get_related_name (ent->name, ext, buf);
	if (srtname)
	{
		char *base = basename (REAL_NAME(ent));
		char *ext = strrchr (base, '.');
		char *title = strndup (base, ext ? ext - base : strlen (base));
		data = get_virtual_srt(title, watchtime, length, NULL);
		free (title);

		if (data)
		{
			struct atrfs_entry *srt = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
			VIRTUAL_ENTRY(srt)->set_contents (srt, data, strlen (data));

			VIRTUAL_ENTRY(srt)->next = FILE_ENTRY(ent)->subtitles;
			FILE_ENTRY(ent)->subtitles = srt;

			attach_entry (ent->parent, srt, srtname);
		}
		free (srtname);
	}
}

void detach_subtitles (struct atrfs_entry *ent)
{
	struct atrfs_entry *tmp, *srt = FILE_ENTRY(ent)->subtitles;

	while (srt)
	{
		tmp = VIRTUAL_ENTRY(srt)->next;
		detach_entry (srt);
		free (VIRTUAL_ENTRY(srt)->m_data);
		destroy_entry (srt);
		srt = tmp;
	}

	FILE_ENTRY(ent)->subtitles = NULL;
}
