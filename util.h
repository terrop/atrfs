/* util.h - 24.7.2008 - 25.7.2008 Ari & Tero Roponen */
#ifndef UTIL_H
#define UTIL_H
#include <stdbool.h>
#include "entry.h"

int get_value (struct atrfs_entry *ent, char *attr, int def);
void set_value (struct atrfs_entry *ent, char *attr, int value);
char *uniquify_in_directory (char *name, struct atrfs_entry *dir);
char *uniquify_globally (char *name, struct atrfs_entry *root);
void handle_srt_for_file (struct atrfs_entry *file, bool insert);

void tmplog(char *fmt, ...);


#endif
