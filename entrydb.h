#ifndef ENTRYDB_H
#define ENTRYDB_H
#include <stdbool.h>
#include "entry.h"

bool open_entrydb (char *filename);
void close_entrydb (void);

char *entrydb_get (struct atrfs_entry *ent, char *attr);
void entrydb_put (struct atrfs_entry *ent, char *attr, char *val);

void entrydb_ensure_exists (char *sha1);

#endif /* ! ENTRYDB_H */
