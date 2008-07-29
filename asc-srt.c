/* asc-srt.c - 23.7.2008 - 29.7.2008 Ari & Tero Roponen */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

char *asc_read_subtitles (char *ascfile, char *lang)
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

#if 0
int main (int argc, char *argv[])
{
	if (argc != 3)
		return 1;

	char *srt = asc_read_subtitles (argv[1], argv[2]);
	if (srt)
	{
		printf ("%s", srt);
		free (srt);
	}
	return 0;
}
#endif
