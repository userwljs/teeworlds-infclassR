#ifndef ENGINE_SERVER_DATABASES_SCHEMA_H
#define ENGINE_SERVER_DATABASES_SCHEMA_H

#include <engine/storage.h>

namespace DbSchema
{
	// Version 0 is any database without a version table.
	constexpr int CurrentVersion = 1;

	// Verifies the schema version and migrates the SQLite database if needed.
	// A full backup is written to db-backups/ before any migration.
	// Returns true on failure, pError is set in that case.
	bool EnsureCurrent(IStorage *pStorage, const char *pDbPath, char *pError, int ErrorSize);
}

#endif
