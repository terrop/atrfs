#include <sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

static int callback(void *data, int ncols, char **values, char **names)
{
	int i;
	for (i = 0; i < ncols; i++)
		printf ("Name: %s, value: %s\n", names[i], values[i]);
	return 0;
}

int main (int argc, char *argv[])
{
	sqlite3 *db;
	char *err;
	int need_setup = false;

	if (access (argv[1], F_OK) != 0)
		need_setup = true;

	sqlite3_open (argv[1], &db);
	if (need_setup)
	{
		sqlite3_exec (db,
			      "CREATE TABLE Files (file FILE);"
			      "ALTER TABLE Files ADD watchcount INT DEFAULT 0;"
			      "ALTER TABLE Files ADD watchtime REAL DEFAULT 0.0;"
//			      "INSERT INTO Files (File) VALUES (\"oma.flv\");"
			      , NULL, NULL, &err);
		if (err)
		{
			printf ("Error: %s\n", err);
			sqlite3_free (err);
			return 1;
		}
		printf ("Created database: %s\n", argv[1]);
	}

	sqlite3_exec (db, "SELECT COUNT(*) FROM Files;", callback, NULL, &err);
	if (err)
	{
		printf ("Error: %s\n", err);
		sqlite3_free (err);
	}
	sqlite3_close (db);
	return 0;
}

/* Local Variables: */
/* compile-command: "gcc -osql sql.c -lsqlite3" */
/* End: */
