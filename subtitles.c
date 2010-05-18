#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "entry.h"
#include "subtitles.h"
#include "util.h"

extern char *get_real_srt(char *filename, double watchtime, double length, char *lang);
extern char *get_virtual_srt(char *filename, double watchtime, double length, char *lang);

char *language_list;

void attach_subtitles (struct atrfs_entry *ent)
{
	char *lng = strdup (language_list);
	char *s, *saved;

	double watchtime = get_dvalue (ent, "user.watchtime", 0.0);
	double length = get_dvalue (ent, "user.length", 0.0);
	char *srtname, *data;

	/* Attach real subtitles, */
	for (s = strtok_r (lng, ", \n", &saved); s; s = strtok_r (NULL, ", \n", &saved))
	{
		char buf[10];
		sprintf (buf, "_%s.srt", s);

		char *srtname = get_related_name (ent->name, ".flv", buf);
		if (srtname && ! lookup_entry_by_name (ent->parent, srtname))
		{
			data = get_real_srt(FILE_ENTRY(ent)->real_path, watchtime, length, s);
			if (data)
			{
				struct atrfs_entry *srt = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
				VIRTUAL_ENTRY(srt)->set_contents (srt, data, strlen (data));

				VIRTUAL_ENTRY(srt)->next = FILE_ENTRY(ent)->subtitles;
				FILE_ENTRY(ent)->subtitles = srt;

				attach_entry (ent->parent, srt, srtname);
			}
		}
		free (srtname);
	}

	/* Attach virtual subtitles */
	srtname = get_related_name (ent->name, ".flv", "_xx.srt");
	if (srtname)
	{
		data = get_virtual_srt(FILE_ENTRY(ent)->real_path, watchtime, length, NULL);
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
}
