# asc.py - 2.5.2009 - 6.5.2009 Ari & Tero Roponen -*- coding: utf-8 -*-

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

def asc_read_subtitles(filename, lang, return_as_list = False):
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
    if return_as_list:
        return asc_text
    else:
        return "".join(asc_text)

def asc_fake_subtitle(time):
    bm, bs = (time / 60, time % 60)
    em, es = ((time+1) / 60, (time + 1) % 60)
    begin = "%02d:%02d" % (bm, bs)
    end = "%02d:%02d" % (em, es)
    return "00:%s,000 --> 00:%s,000\n%s\n" % (begin, end, begin)

def asc_fake_subtitles(name, length):
    text = ["0\n00:00:00,000 --> 00:00:05,000\n%s\n" % name]
    if length < 16: return text
    pos = 1
    for time in range(15, length, 15):
        text.append("%d\n%s" % (pos, asc_fake_subtitle(time)))
        pos = pos + 1
    return "\n".join(text)
