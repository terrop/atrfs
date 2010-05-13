/* entrydb.c - 10.5.2010 - 10.5.2010 Ari & Tero Roponen */
#include <stdbool.h>
#include <string.h>
#include "database.h"
#include "entrydb.h"

/* In sha1.c */
char *get_sha1 (char *filename);

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
		char *name = FILE_ENTRY(ent)->real_path;
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
		val = database_get (entrydb, sha1, attr);
	}
	return val;
}

void entrydb_put (struct atrfs_entry *ent, char *attr, char *val)
{
	if (entrydb)
	{
		char *sha = get_sha1 (FILE_ENTRY(ent)->real_path);
		database_set (entrydb, sha, attr, val);
	}
}
