/* util.h - 24.7.2008 - 29.7.2008 Ari & Tero Roponen */
#ifndef UTIL_H
#define UTIL_H
#include <stdbool.h>
#include "entry.h"

int get_value (struct atrfs_entry *ent, char *attr, int def);
void set_value (struct atrfs_entry *ent, char *attr, int value);
char *uniquify_name (char *name, struct atrfs_entry *root);
void handle_srt_for_file (struct atrfs_entry *file, bool insert);

void tmplog(char *fmt, ...);
void get_all_file_entries (struct atrfs_entry ***entries, size_t *count);
char *get_pdir(int filelen, int watchcount, int watchtime);
char *secs_to_time (int secs);
char *pid_to_cmdline(pid_t pid);

#endif
