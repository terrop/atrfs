#include <stdlib.h>
#include <string.h>
#include "entry.h"
#include "subtitles.h"
#include "util.h"

extern char *get_real_srt(char *filename, double watchtime, double length);
extern char *get_virtual_srt(char *filename, double watchtime, double length);

char *language_list;

void attach_subtitles (struct atrfs_entry *ent)
{
	/* Add subtitles to flv-files. */
	char *srtname = get_related_name (ent->name, ".flv", ".srt");
	if (srtname)
	{
		if (! lookup_entry_by_name (ent->parent, srtname))
		{
			double watchtime = get_dvalue (ent, "user.watchtime", 0.0);
			double length = get_dvalue (ent, "user.length", 0.0);
			char *data = get_real_srt(FILE_ENTRY(ent)->real_path,
						  watchtime, length);
			if (!data)
				data = get_virtual_srt(FILE_ENTRY(ent)->real_path,
						       watchtime, length);

			struct atrfs_entry *srt = create_entry (ATRFS_VIRTUAL_FILE_ENTRY);
			VIRTUAL_ENTRY(srt)->set_contents(srt, data, strlen (data));
			attach_entry (ent->parent, srt, srtname);
		}

		free (srtname);
	}


}

void detach_subtitles (struct atrfs_entry *ent)
{
	/* Remove subtitle file. */
	char *srt_name = get_related_name (ent->name, ".flv", ".srt");

	if (srt_name)
	{
		struct atrfs_entry *srt = lookup_entry_by_name (ent->parent, srt_name);
		if (srt)
		{
			detach_entry (srt);
			free (VIRTUAL_ENTRY(srt)->m_data);
			destroy_entry (srt);
		}
		free (srt_name);
	}
}


