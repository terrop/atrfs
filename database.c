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

static void database_dump_elts (struct dbelt *elts)
{
	tmplog("<dump>\n");
	for (;elts; elts = elts->next)
		tmplog (" %s:%s\n", elts->var, elts->val);
	tmplog("</dump>\n");
}

char *database_get (struct database *db, char *sha, char *key)
{
	struct dbelt *elt, *elts = database_get_elts (db, sha);
	char *val = NULL;

	for (elt = elts; elt; elt = elt->next)
	{
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
	tmplog ("set %s:%s\n", key, val);
	struct dbelt *elt, *elts = database_get_elts (db, sha);
	char *data, *pos;
	char buf[128];		/* XXX */

	data = malloc (1024);	/* XXX */
	if (! data)
		abort ();
	pos = data;

	pos += sprintf (pos, "%s:%s", key, val);
	pos++;

	for (elt = elts; elt; elt = elt->next)
		if (strcmp (elt->var, key) && elt->val)
		{
			pos += sprintf (pos, "%s:%s", elt->var, elt->val);
			pos++;
		}

	DBT dkey = {
		.data = sha,
		.size = strlen (sha),
	};
	DBT ddata = {
		.data = data,
		.size = pos - data,
	};

	if (((DB *)db->handle)->put(db->handle, NULL, &dkey, &ddata, 0))
	{
		tmplog("DB problem\n");
		abort ();
	}
	database_free_elts (elts);
	free (data);
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
