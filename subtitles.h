#ifndef SUBTITLES_H
#define SUBTITLES_H
#include "entry.h"

/* In subtitles.c */
extern char *language_list;

void attach_subtitles (struct atrfs_entry *ent);
void detach_subtitles (struct atrfs_entry *ent);

#endif /* SUBTITLES_H */
