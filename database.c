/* database.c - 7.5.2010 - 7.5.2010 Ari & Tero Roponen */

#include <db.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "database.h"

struct dbelt
{
	char *var;
	char *val;
	struct dbelt *next;
};

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

static struct dbelt *database_get_elts (struct database *db, char *sha1)
{
	struct dbelt *elts = NULL;
	char *s, *str = database_get_raw (db, sha1);
	if (! str)
		return NULL;
	for (s = str; *s; s += strlen (s) + 1)
	{
		struct dbelt *elt = malloc (sizeof (*elt));
		if (!elt)
			abort ();
		elt->var = strdup (s);
		elt->val = strchr (elt->var, ':');
		if (elt->val)
		{
			*elt->val = '\0';
			elt->val++;
		} else elt->val = NULL;
		elt->next = elts;
		elts = elt;
	}
	free (str);
	return elts;
}

static void database_free_elts (struct dbelt *elts)
{
	while (elts)
	{
		struct dbelt *next = elts->next;
		free (elts->var);
		free (elts);
		elts = next;
	}
}

char *database_get (struct database *db, char *sha, char *key)
{
	struct dbelt *elt, *elts = database_get_elts (db, sha);
	char *val = NULL;

	for (elt = elts; elt; elt = elt->next)
	{
		tmplog ("%s = %s\n", elt->var, elt->val);
		if (strcmp (elt->var, key) == 0)
		{
			val = strdup (elt->val);
			break;
		}
	}
	database_free_elts (elts);
	return val;
}

void database_set (struct database *db, char *sha, char *key, char *val)
{
	size_t size = strlen (key) + 1 + strlen (val) + 1;
	struct dbelt *elt, *elts = database_get_elts (db, sha);
	char *data;
	int i, len;

	for (elt = elts; elt; elt = elt->next)
		if (elt->val)
			size += strlen (elt->var) + 1 + strlen (elt->val) + 1;
	data = malloc (size);
	if (! data)
		abort ();

	i = sprintf (data, "%s:%s", key, val) + 1;
	for (elt = elts; elt; elt = elt->next)
		if (strcmp (elt->var, key) && elt->val)
		{
			len = sprintf (data + i, "%s:%s", elt->var, elt->val) + 1;
			i += len;
		}

	DBT dkey = {
		.data = sha,
		.size = strlen (sha),
	};
	DBT ddata = {
		.data = data,
		.size = len,
	};

	if (((DB *)db->handle)->put(db->handle, NULL, &dkey, &ddata, 0))
	{
		tmplog("DB problem\n");
		abort ();
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
