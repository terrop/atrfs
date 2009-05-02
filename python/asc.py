# asc.py - 2.5.2009 - 2.5.2009 Ari & Tero Roponen -*- coding: utf-8 -*-

"""
char *language_list;
static bool has_many_languages = false;
static bool found_language = false;

static void print_correct_text (const char *line, char *lang, FILE *out)
{
	char *saved;
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
	for (s = strtok_r (tags, ",", &saved); s; s = strtok_r (NULL, ",", &saved))
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

static char *get_virtual_srt(char *filename)
{
	char *base = basename (filename);
	char *ret = NULL;
	int i, linenum;
	double watchtime, length;

	char *ext = strrchr(base, '.');
	if (!ext || strcmp(ext, ".flv"))
		goto out;

	watchtime = get_dvalue (filename, "user.watchtime", 0.0);
	length = get_dvalue (filename, "user.length", 0.0);

	asprintf (&ret,
		  "1\n00:00:00,00 --> 00:00:05,00\n"
		  "%.*s\n%.2lf × %s\n\n",
		  ext - base, base,
		  length >= 1.0 ? watchtime / length : 0,
		  secs_to_time (length));

	linenum = 2;
	for (i = 15; i < (int)length; i += 15)
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

static char *get_real_srt(char *filename)
{
	char *ret = NULL;
	char *lng = strdup(language_list);
	char *s, *saved;
	char *ascname = get_related_name (filename, ".flv", ".asc");

	/* Try different languages in requested order */
	for (s = strtok_r(lng, ", \n", &saved); s; s = strtok_r(NULL, ", \n", &saved))
	{
		ret = asc_read_subtitles(ascname, s);
		if (ret)
			break;
	}

	free (ascname);
	free(lng);
	return ret;
}

char *get_srt(char *filename)
{
	/*
	 * Try to load real subtitles from asc-file. If this
	 * fails, use virtual subtitles instead.
	 * */
	char *ret = get_real_srt(filename);
	if (!ret)
		ret = get_virtual_srt(filename);
	return ret;
}
"""

asc_text = None
has_many_languages = False
found_language = False

def add_correct_text(line, lang):
    global asc_text, has_many_languages, found_language
    end = line.index("]")
    if not "]" in line:
        return
    has_many_languages = True
    tags = line[1:line.index("]")].split(",")
    if lang in tags:
        found_language = True
        asc_text.append(line[line.index("]")+1+1:]) # skip space

def asc_read_subtitles(filename, lang):
    global asc_text, has_many_languages, found_language
    asc_text = []
    has_many_languages = False
    found_language = False

    f = file(filename)
    lines = f.readlines()
    f.close()
    after_empty = True
    pos = 0
    for line in lines:
        if line[0] == "\n":
            after_empty = True
            asc_text.append(line)
        elif line[0] == "#":
            pass
        elif line[0] in ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9"]:
            if after_empty:
                asc_text.append("%d\n" % pos)
                pos = pos + 1
            asc_text.append(line)
        elif line[0] == "[":
            add_correct_text(line, lang)
        else:
            asc_text.append(line)
    if has_many_languages and not found_language:
        return None
    return "".join(asc_text)
