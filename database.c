/* database.c - 7.5.2010 - 8.7.2010 Ari & Tero Roponen */
#include <stdbool.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "database.h"
#include "util.h"

struct database *open_database (char *filename)
{
	struct database *db = NULL;

	sqlite3 *handle;
	char *err;
	bool must_create = false;

	if (access (filename, F_OK) != 0)
		must_create = true;

	sqlite3_open (filename, &handle);
	if (must_create)
	{
		sqlite3_exec (handle,
			      "CREATE TABLE Files (file FILE, sha1 TEXT);"
			      "ALTER TABLE Files ADD count INT DEFAULT 0;"
			      "ALTER TABLE Files ADD watchtime REAL DEFAULT 0.0;"
			      "ALTER TABLE Files ADD length REAL DEFAULT 0.0;"
//			      "INSERT INTO Files (File) VALUES (\"oma.flv\");"
			      , NULL, NULL, &err);
		if (err)
		{
			tmplog ("Error: %s\n", err);
			sqlite3_free (err);
			return NULL;
		}
		tmplog ("Created database: %s\n", filename);
	}

	db = malloc (sizeof (*db));
	if (!db)
	{
		sqlite3_close (handle);
		return NULL;
	}

	db->handle = handle;
	return db;
}

void close_database (struct database *db)
{
	sqlite3 *handle = db->handle;
	sqlite3_close (handle);
	db->handle = NULL;
	free (db);
}

char *database_get (struct database *db, char *sha, char *key)
{
	char *err, *val = NULL;
	char *cmd;

	int get_callback (void *data, int ncols, char **values, char **names)
	{
		int i;
		for (i = 0; i < ncols; i++)
			tmplog ("Name: %s, value: %s\n", names[i], values[i]);

		if (ncols >= 1)	/* Exactly 1! */
			val = strdup (values[0]);
		return 0;
	}

	asprintf (&cmd, "SELECT %s FROM Files WHERE sha1=\"%s\";", key, sha);

	sqlite3_exec (db->handle, cmd, get_callback, NULL, &err);
	if (err)
	{
		tmplog ("database_get: %s\n", err);
		sqlite3_free (err);
	}

	return val;
}

void database_set (struct database *db, char *sha, char *key, char *val)
{
	char *err;
	char *cmd;

	asprintf (&cmd, "UPDATE Files SET %s = '%s' WHERE sha1=\"%s\";", key, val, sha);

	sqlite3_exec (db->handle, cmd, NULL, NULL, &err);
	if (err)
	{
		tmplog ("database_set: %s\n", err);
		sqlite3_free (err);
	}
}

void database_insert_file (struct database *db, char *filename, char *sha)
{
	char *err;
	char *cmd;

	asprintf (&cmd, "INSERT INTO Files (file, sha1) VALUES (\"%s\", \"%s\");", filename, sha);

	sqlite3_exec (db->handle, cmd, NULL, NULL, &err);
	if (err)
	{
		tmplog ("database_insert_file: %s\n", err);
		sqlite3_free (err);
	}
}

#ifdef DATABASE_TEST
int main (int argc, char *argv[])
{
	if (argc < 2)
		return 1;
	struct database *db = open_database (argv[1]);
	if (db)
	{
		if (argc == 4)
		{
			char *data = database_get (db, argv[2], argv[3]);
			if (data)
			{
				printf ("%s\n", data);
				free (data);
			}
		}
		close_database (db);
	}
	return 0;
}
#endif
