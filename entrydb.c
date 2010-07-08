/* entrydb.c - 10.5.2010 - 10.5.2010 Ari & Tero Roponen */
#include <sqlite3.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "entrydb.h"

/* In sha1.c */
char *get_sha1 (char *filename);

static sqlite3 *entrydb;

char *get_sha1_fast (char *filename)
{
	int len = getxattr (filename, "user.sha1", NULL, 0);
	static char *sha1;
	static char buf[32];

	sha1 = NULL;
	if (len <= 0)
	{
		sha1 = get_sha1 (filename);
		if (setxattr (filename, "user.sha1", sha1, strlen (sha1) + 1, 0))
			perror ("Can't set SHA1");
	}

	if (! sha1)
	{
		getxattr (filename, "user.sha1", buf, len);
		sha1 = buf;
	}
	return sha1;
}

bool entrydb_exec (int (*callback)(void *data, int ncols, char **values, char **names), char *cmdfmt, ...)
{
	char *cmd = NULL, *err = NULL;
	bool ret = true;
	va_list list;
	va_start (list, cmdfmt);

	vasprintf (&cmd, cmdfmt, list);
	sqlite3_exec (entrydb, cmd, callback, NULL, &err);
	va_end (list);

	if (err)
	{
		tmplog ("%s while executing: \"%s\"\n", err, cmd);
		sqlite3_free (err);
		ret = false;
	}
	free (cmd);
	return ret;
}

bool open_entrydb (char *filename)
{
	sqlite3 *handle = NULL;
	char *err;
	bool must_create = false;

	if (access (filename, F_OK) != 0)
		must_create = true;

	sqlite3_open (filename, &handle);
	if (must_create)
	{
		entrydb = handle; /* Hack! */
		if (! entrydb_exec (NULL,
				    "CREATE TABLE Files (sha1 TEXT, count INT DEFAULT 0);"
				    "ALTER TABLE Files ADD watchtime REAL DEFAULT 0.0;"
				    "ALTER TABLE Files ADD length REAL DEFAULT 0.0;"
				    //"INSERT INTO Files (File) VALUES (\"oma.flv\");"
			    ))
		{
			sqlite3_close (handle);
			entrydb = NULL;
			return false;
		}

		tmplog ("Created database: %s\n", filename);
	}

	entrydb = handle;
	return true;
}

void close_entrydb (void)
{
	sqlite3_close (entrydb);
	entrydb = NULL;
}

static char *database_get (sqlite3 *db, char *sha, char *key)
{
	char *val = NULL;

	int get_callback (void *data, int ncols, char **values, char **names)
	{
		if (ncols >= 1 && !val)	/* Exactly 1! */
			val = strdup (values[0]);
		return 0;
	}

	if (! entrydb_exec (get_callback, "SELECT %s FROM Files WHERE sha1=\"%s\" limit 1;", key, sha))
		return NULL;
	return val;
}

static void entrydb_ensure_exists (char *sha1)
{
	char *val = database_get (entrydb, sha1, "sha1");
	if (! val)
	{
		entrydb_exec (NULL, "INSERT INTO Files (sha1) VALUES (\"%s\");", sha1);
	}
	free (val);
}

char *entrydb_get (struct atrfs_entry *ent, char *attr)
{
	char *val = NULL;
	if (entrydb)
	{
		char *sha1 = get_sha1_fast (REAL_NAME (ent));
		if (! sha1)
			abort ();

		entrydb_ensure_exists (sha1);
		val = database_get (entrydb, sha1, attr);
	}

	return val;
}

void entrydb_put (struct atrfs_entry *ent, char *attr, char *val)
{
	if (entrydb)
	{
		char *sha1 = get_sha1 (REAL_NAME(ent));

		entrydb_ensure_exists (sha1);
		entrydb_exec (NULL, "UPDATE Files SET %s = \"%s\" WHERE sha1=\"%s\";", attr, val, sha1);
	}
}
