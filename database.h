/* database.h - 7.5.2010 - 7.5.2010 Ari & Tero Roponen */

#ifndef DATABASE_H
#define DATABASE_H

struct database
{
	void *handle;
};

struct database *open_database (char *filename);
void close_database (struct database *db);

char *database_get (struct database *db, char *sha, char *key);
void database_set (struct database *db, char *sha, char *key, char *val);

#endif /* ! DATABASE_H */
