/* asc-srt.c - 23.7.2008 - 25.8.2008 Ari & Tero Roponen */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "entry.h"
#include "util.h"

char *wanted_language[10] =
{
	[0] = "fi",
	[1] = "it",
	[2] = "en",
	[3] = "la",
};

static bool has_many_languages = false;
static bool found_language = false;

static void print_correct_text (const char *line, char *lang, FILE *out)
{
	char *end = strchr (line+1, ']');
	if (! end)
	{
		fprintf (out, "%s", line);
		return;
	}
	has_many_languages = true;

	char *tags = strdup (line + 1);
	tags[end - line - 1] = '\0';
	char *s;
	for (s = strtok (tags, ","); s; s = strtok (NULL, ","))
	{
		if (! strcmp (s, lang))
		{
			found_language = true;
			fprintf (out, "%s", end + 1 + 1); /* space */
			break;
		}
	}
	free (tags);
}

static char *asc_read_subtitles (char *ascfile, char *lang)
{
	char *text = NULL;
	size_t size;
	FILE *out = (FILE *)open_memstream (&text, &size);

	FILE *fp = fopen (ascfile, "r");
	if (! fp)
		return NULL;

	has_many_languages = false;
	found_language = false;

	char *line = NULL;
	size_t len = 0;
	bool after_empty = true;
	int pos = 0;

	while (getline (&line, &len, fp) > 0)
	{
		switch (line[0])
		{
		default:
			fprintf (out, "%s", line);
			after_empty = false;
			break;
		case '\n':
			after_empty = true;
			fprintf (out, "%s", line);
		case '#':	/* comment */
			break;
		case '0' ... '9': /* possible timestamp */
			if (after_empty)
				fprintf (out, "%d\n", pos++);
			fprintf (out, "%s", line);
			break;
		case '[':
			print_correct_text (line, lang, out);
			break;
		}
		free (line);
		line = NULL;
	}
	fclose (fp);
	fclose (out);

	if (has_many_languages && ! found_language)
	{
		free (text);
		return NULL;
	}
	return text;
}

static char *get_virtual_srt(struct atrfs_entry *ent)
{
	char *ret = NULL;
	int i, linenum;

	char *ext = strrchr(ent->name, '.');
	if (!ext || strcmp(ext, ".flv"))
		goto out;

	asprintf (&ret,
		  "1\n00:00:00,00 --> 00:00:05,00\n"
		  "%.*s\n%s / %s\n\n",
		  ext - ent->name,
		  ent->name,
		  secs_to_time (get_dvalue (ent, "user.watchtime", 0.0)),
		  secs_to_time (get_dvalue (ent, "user.length", 0.0)));

	linenum = 2;
	for (i = 15; i < (int)get_dvalue (ent, "user.length", 0.0); i += 15)
	{
		char *tmp = NULL;
		char *line = NULL;
		asprintf (&line,
			"%d\n00:%s.00 --> 00:%s,00\n%s\n\n",
			linenum++,
			secs_to_time(i),
			secs_to_time(i+1),
			secs_to_time(i));

		asprintf (&tmp, "%s%s", ret, line);
		free (ret);
		ret = tmp;
	}
out:
	return ret;
}

static char *get_real_srt(struct atrfs_entry *ent)
{
	char *ret = NULL;

	char *ascname = strdup (ent->file.e_real_file_name);
	strcpy (strrchr (ascname, '.'), ".asc");

	/* Try different languages in requested order */
	int i;
	for (i = 0; wanted_language[i]; i++)
	{
		ret = asc_read_subtitles(ascname, wanted_language[i]);
		if (ret)
			break;
	}

	free (ascname);
	return ret;
}

char *get_srt(struct atrfs_entry *ent)
{
	/*
	 * Try to load real subtitles from asc-file. If this
	 * fails, use virtual subtitles instead.
	 * */
	char *ret = get_real_srt(ent);
	if (!ret)
		ret = get_virtual_srt(ent);
	return ret;
}
