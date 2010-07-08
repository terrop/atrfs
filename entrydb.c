/* entrydb.c - 10.5.2010 - 10.5.2010 Ari & Tero Roponen */
#include <sqlite3.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "entrydb.h"

/* In sha1.c */
char *get_sha1 (char *filename);

static sqlite3 *entrydb;

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
		sqlite3_exec (handle,
			      "CREATE TABLE Files (sha1 TEXT, count INT DEFAULT 0);"
			      "ALTER TABLE Files ADD watchtime REAL DEFAULT 0.0;"
			      "ALTER TABLE Files ADD length REAL DEFAULT 0.0;"
//			      "INSERT INTO Files (File) VALUES (\"oma.flv\");"
			      , NULL, NULL, &err);
		if (err)
		{
			tmplog ("Error: %s\n", err);
			sqlite3_free (err);
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
	char *err, *val = NULL;
	char *cmd;

	int get_callback (void *data, int ncols, char **values, char **names)
	{
		if (ncols >= 1 && !val)	/* Exactly 1! */
			val = strdup (values[0]);
		return 0;
	}

	asprintf (&cmd, "SELECT %s FROM Files WHERE sha1=\"%s\" limit 1;", key, sha);

	sqlite3_exec (db, cmd, get_callback, NULL, &err);
	free (cmd);

	if (err)
	{
		tmplog ("database_get: %s\n", err);
		sqlite3_free (err);
	}

	return val;
}

void database_set (sqlite3 *db, char *sha, char *key, char *val)
{
	char *err;
	char *cmd;

	asprintf (&cmd, "UPDATE Files SET %s = '%s' WHERE sha1=\"%s\";", key, val, sha);

	sqlite3_exec (db, cmd, NULL, NULL, &err);
	free (cmd);
	if (err)
	{
		tmplog ("database_set: %s\n", err);
		sqlite3_free (err);
	}
}

void database_insert_file (sqlite3 *db, char *filename, char *sha)
{
	char *err;
	char *cmd;

	asprintf (&cmd, "INSERT INTO Files (sha1) VALUES (\"%s\");", sha);

	sqlite3_exec (db, cmd, NULL, NULL, &err);
	free (cmd);
	if (err)
	{
		tmplog ("database_insert_file: %s\n", err);
		sqlite3_free (err);
	}
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
