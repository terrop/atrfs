/* database.c - 7.5.2010 - 7.5.2010 Ari & Tero Roponen */

#include <db.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "database.h"

struct database *open_database (char *filename)
{
	struct database *db = NULL;
	DB *handle;
	int ret;

	if (db_create (&handle, NULL, 0))
		return NULL;

	/* Create file if it doesn't exist. */
	if (access (filename, F_OK) != 0)
		ret = handle->open(handle, NULL, filename, NULL, DB_HASH, DB_CREATE, 0);
	else
		ret = handle->open(handle, NULL, filename, NULL, DB_UNKNOWN, 0, 0);

	if (ret)
	{
		printf ("Error: %d\n", ret);
		handle->close(handle, 0);
		return NULL;
	}

	db = malloc (sizeof (*db));
	if (!db)
	{
		handle->close(handle, 0);
		return NULL;
	}

	db->handle = handle;
	return db;
}

void close_database (struct database *db)
{
	DB *handle = db->handle;
	handle->close(handle, 0);
	db->handle = NULL;
	free (db);
}

static char *database_get_raw (struct database *db, char *keystr)
{
	DBT key = {
		.data = keystr,
		.size = strlen (keystr),
	};
	DBT data = {0};

	if (((DB *)db->handle)->get(db->handle, NULL, &key, &data, 0))
		return NULL;
	char *str = malloc (data.size);
	if (!str)
		return NULL;
	memcpy (str, data.data, data.size);
	return str;
}

char *database_get (struct database *db, char *sha, char *key)
{
	int slen, keylen = strlen (key);
	char *s, *str = database_get_raw (db, sha);
	if (! str)
		return NULL;
	for (s = str; *s;)
	{
		slen = strlen (s);
		if (slen >= keylen + 2
		    && strncmp (key, s, keylen) == 0
		    && s[keylen] == ':')
		{
			char *val = strdup (s + keylen + 1);
			free (str);
			return val;
		}
		s += slen + 1;
	}
	free (str);
	return NULL;
}

void database_set (struct database *db, char *sha, char *key, char *val)
{
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
