/* entrydb.c - 10.5.2010 - 10.5.2010 Ari & Tero Roponen */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "database.h"
#include "entrydb.h"

/* In sha1.c */
char *get_sha1 (char *filename);

/* In database.c */
void database_insert_file (struct database *db, char *filename, char *sha);

static struct database *entrydb;

bool open_entrydb (char *filename)
{
	entrydb = open_database (filename);
	if (entrydb)
		return true;
	return false;
}

void close_entrydb (void)
{
	if (entrydb)
		close_database (entrydb);
	entrydb = NULL;
}

char *entrydb_get (struct atrfs_entry *ent, char *attr)
{
	char *val = NULL;
	if (entrydb)
	{
		char *name = REAL_NAME(ent);
		int len = getxattr (name, "user.sha1", NULL, 0);
		char *sha1 = NULL;
		if (len <= 0)
		{
			sha1 = get_sha1 (name);
			if (setxattr (name, "user.sha1", sha1, strlen (sha1) + 1, 0))
				perror ("Can't set SHA1");
		}

		char buf[len];
		if (! sha1)
		{
			getxattr (name, "user.sha1", buf, len);
			sha1 = buf;
		}

		val = database_get (entrydb, sha1, "sha1");
		if (val)
			free (val);
		else
			database_insert_file (entrydb, name, sha1);
		val = database_get (entrydb, sha1, attr);

		/* Debugging... */
		tmplog ("%s, value: %s\n", attr, val);
	}

	return val;
}

void entrydb_put (struct atrfs_entry *ent, char *attr, char *val)
{
	if (entrydb)
	{
		char *sha = get_sha1 (REAL_NAME(ent));
		database_set (entrydb, sha, attr, val);
	}
}
