/* entry_filter.h - 19.5.2010 - 19.5.2010 Ari & Tero Roponen */

#ifndef ENTRY_FILTER_H
#define ENTRY_FILTER_H
#include "entry.h"

void add_filter (char *str);
char *get_category (struct atrfs_entry *ent);

#endif /* ! ENTRY_FILTER_H */
