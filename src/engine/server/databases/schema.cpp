#include "schema.h"

#include "connection.h"

#include <base/fs.h>
#include <base/log.h>
#include <base/str.h>
#include <base/time.h>

#include <mutex>
#include <set>
#include <string>

namespace DbSchema
{
	namespace
	{
		bool TableExists(IDbConnection *pConn, const char *pTable, bool *pExists, char *pError, int ErrorSize)
		{
			char aName[128];
			str_format(aName, sizeof(aName), "%s_%s", pConn->GetPrefix(), pTable);
			return pConn->TableExists(aName, pExists, pError, ErrorSize);
		}

		bool GetVersion(IDbConnection *pConn, int *pVersion, char *pError, int ErrorSize)
		{
			*pVersion = 0;

			bool Exists = false;
			if(TableExists(pConn, "db_version", &Exists, pError, ErrorSize))
				return true;
			if(!Exists)
				return false;

			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "SELECT Version FROM %s_db_version LIMIT 1", pConn->GetPrefix());
			if(pConn->PrepareStatement(aBuf, pError, ErrorSize))
				return true;

			bool End = false;
			if(pConn->Step(&End, pError, ErrorSize))
				return true;
			if(!End)
				*pVersion = pConn->GetInt(1);
			return false;
		}

		bool SetVersion(IDbConnection *pConn, int Version, char *pError, int ErrorSize)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "CREATE TABLE IF NOT EXISTS %s_db_version (Version INTEGER NOT NULL)", pConn->GetPrefix());
			if(pConn->PrepareStatement(aBuf, pError, ErrorSize))
				return true;
			int NumUpdated = 0;
			if(pConn->ExecuteUpdate(&NumUpdated, pError, ErrorSize))
				return true;

			str_format(aBuf, sizeof(aBuf), "DELETE FROM %s_db_version", pConn->GetPrefix());
			if(pConn->PrepareStatement(aBuf, pError, ErrorSize))
				return true;
			if(pConn->ExecuteUpdate(&NumUpdated, pError, ErrorSize))
				return true;

			str_format(aBuf, sizeof(aBuf), "INSERT INTO %s_db_version(Version) VALUES(?)", pConn->GetPrefix());
			if(pConn->PrepareStatement(aBuf, pError, ErrorSize))
				return true;
			pConn->BindInt(1, Version);
			return pConn->ExecuteUpdate(&NumUpdated, pError, ErrorSize);
		}

		bool Exec(IDbConnection *pConn, const char *pSql, char *pError, int ErrorSize)
		{
			if(pConn->PrepareStatement(pSql, pError, ErrorSize))
				return true;
			int NumUpdated = 0;
			return pConn->ExecuteUpdate(&NumUpdated, pError, ErrorSize);
		}

		// SQLite has no ALTER COLUMN, the table is rebuilt from an explicit v1 schema
		// instead. Each table (main and backup) is rebuilt from its own rows so that
		// writes staged in the backup table are not lost during the migration.
		bool RebuildPasswordsTable(IDbConnection *pConn, bool Backup, char *pError, int ErrorSize)
		{
			const char *pSuffix = Backup ? "_backup" : "";
			const char *pCollate = pConn->BinaryCollate();

			char aOldTable[128];
			char aNewTable[128];
			str_format(aOldTable, sizeof(aOldTable), "%s_passwords%s", pConn->GetPrefix(), pSuffix);
			str_format(aNewTable, sizeof(aNewTable), "%s_passwords%s_new", pConn->GetPrefix(), pSuffix);

			// A previous migration may have been interrupted after creating this table.
			char aSql[1024];
			str_format(aSql, sizeof(aSql), "DROP TABLE IF EXISTS %s", aNewTable);
			if(Exec(pConn, aSql, pError, ErrorSize))
				return true;

			str_format(aSql, sizeof(aSql),
				"CREATE TABLE %s ("
				"  UserId INTEGER PRIMARY KEY AUTOINCREMENT, "
				"  Username VARCHAR(%d) COLLATE %s UNIQUE NOT NULL, "
				"  PasswordHash VARCHAR(%d) COLLATE %s NOT NULL, "
				"  RegisterDate TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
				"  RegisterIp VARCHAR(64) NOT NULL"
				")",
				aNewTable, MAX_NAME_LENGTH_SQL, pCollate, crypto_pwhash_STRBYTES, pCollate);
			if(Exec(pConn, aSql, pError, ErrorSize))
				return true;

			str_format(aSql, sizeof(aSql),
				"INSERT INTO %s(UserId, Username, PasswordHash, RegisterDate, RegisterIp) "
				"SELECT UserId, Username, PasswordHash, RegisterDate, RegisterIp FROM %s",
				aNewTable, aOldTable);
			if(Exec(pConn, aSql, pError, ErrorSize))
				return true;

			// The rows have been copied already, so the old table can be replaced.
			str_format(aSql, sizeof(aSql), "DROP TABLE %s", aOldTable);
			if(Exec(pConn, aSql, pError, ErrorSize))
				return true;

			str_format(aSql, sizeof(aSql), "ALTER TABLE %s RENAME TO %s", aNewTable, aOldTable);
			return Exec(pConn, aSql, pError, ErrorSize);
		}

		// Upgrades exactly one version step and is idempotent.
		// Version 1 widens PasswordHash to crypto_pwhash_STRBYTES and adds the
		// RegisterIp index used by the registration flood protection.
		// The passwords tables are guaranteed to exist here: the migration connection is
		// opened with Setup enabled, which creates any missing table before this runs.
		bool MigrateToV1(IDbConnection *pConn, char *pError, int ErrorSize)
		{
			if(Exec(pConn, "BEGIN", pError, ErrorSize))
				return true;

			char aError[256] = "";
			if(RebuildPasswordsTable(pConn, /* Backup */ false, aError, sizeof(aError)) ||
				RebuildPasswordsTable(pConn, /* Backup */ true, aError, sizeof(aError)))
			{
				Exec(pConn, "ROLLBACK", pError, ErrorSize);
				str_copy(pError, aError, ErrorSize);
				return true;
			}

			// Rebuilding the table drops its indexes, so the v1 index has to be
			// created again here.
			char aIndexSql[512];
			pConn->FormatCreatePasswordsIndex(aIndexSql, sizeof(aIndexSql));
			if(Exec(pConn, aIndexSql, aError, sizeof(aError)))
			{
				Exec(pConn, "ROLLBACK", pError, ErrorSize);
				str_copy(pError, aError, ErrorSize);
				return true;
			}

			if(Exec(pConn, "COMMIT", aError, sizeof(aError)))
			{
				Exec(pConn, "ROLLBACK", pError, ErrorSize);
				str_copy(pError, aError, ErrorSize);
				return true;
			}

			return false;
		}

		std::mutex g_InitMutex;
		std::set<std::string> g_Initialized;

		bool IsInitialized(const char *pDbPath)
		{
			const std::lock_guard<std::mutex> Lock(g_InitMutex);
			return g_Initialized.contains(pDbPath);
		}

		void MarkInitialized(const char *pDbPath)
		{
			const std::lock_guard<std::mutex> Lock(g_InitMutex);
			g_Initialized.insert(pDbPath);
		}

		bool MakeBackupPath(IStorage *pStorage, const char *pDbPath, int Version, char *pBuf, int BufSize, char *pError, int ErrorSize)
		{
			if(!pStorage->FolderExists("db-backups", IStorage::TYPE_SAVE) &&
				!pStorage->CreateFolder("db-backups", IStorage::TYPE_SAVE))
			{
				str_copy(pError, "Can't create the directory 'db-backups'", ErrorSize);
				return true;
			}

			const char *pBaseName = str_rchr(pDbPath, '/');
			const char *pAltBaseName = str_rchr(pDbPath, '\\');
			if(pAltBaseName != nullptr && (pBaseName == nullptr || pAltBaseName > pBaseName))
				pBaseName = pAltBaseName;
			pBaseName = pBaseName != nullptr ? pBaseName + 1 : pDbPath;

			char aTimestamp[64];
			str_timestamp(aTimestamp, sizeof(aTimestamp));

			char aName[IO_MAX_PATH_LENGTH];
			str_format(aName, sizeof(aName), "db-backups/%s-v%d-%s.sqlite", pBaseName, Version, aTimestamp);
			pStorage->GetCompletePath(IStorage::TYPE_SAVE, aName, pBuf, BufSize);
			return false;
		}
	}

	bool EnsureCurrent(IStorage *pStorage, const char *pDbPath, char *pError, int ErrorSize)
	{
		if(IsInitialized(pDbPath))
			return false;

		const bool FileExists = fs_is_file(pDbPath) != 0;
		int Version = 0;

		if(FileExists)
		{
			auto pProbe = CreateSqliteConnection(pDbPath, false);
			if(pProbe->Connect(pError, ErrorSize))
				return true;

			const bool Failure = GetVersion(pProbe.get(), &Version, pError, ErrorSize);
			pProbe->Disconnect();
			if(Failure)
				return true;
		}

		if(Version > CurrentVersion)
		{
			str_format(pError, ErrorSize, "Database schema version %d is newer than the supported version %d", Version, CurrentVersion);
			return true;
		}

		if(Version == CurrentVersion)
		{
			MarkInitialized(pDbPath);
			return false;
		}

		if(!FileExists)
		{
			auto pConn = CreateSqliteConnection(pDbPath, true);
			if(pConn->Connect(pError, ErrorSize))
				return true;

			const bool Failure = SetVersion(pConn.get(), CurrentVersion, pError, ErrorSize);
			pConn->Disconnect();
			if(Failure)
				return true;

			MarkInitialized(pDbPath);
			return false;
		}

		char aBackupPath[IO_MAX_PATH_LENGTH];
		if(MakeBackupPath(pStorage, pDbPath, Version, aBackupPath, sizeof(aBackupPath), pError, ErrorSize))
			return true;

		{
			auto pConn = CreateSqliteConnection(pDbPath, false);
			if(pConn->Connect(pError, ErrorSize))
				return true;

			log_info("sql-migration", "backing up the database to '%s'", aBackupPath);
			const bool Failure = pConn->BackupDatabase(aBackupPath, pError, ErrorSize);
			pConn->Disconnect();
			if(Failure)
				return true;
		}

		auto pConn = CreateSqliteConnection(pDbPath, true);
		if(pConn->Connect(pError, ErrorSize))
			return true;

		for(int Current = Version; Current < CurrentVersion; ++Current)
		{
			bool Failure = true;
			switch(Current)
			{
			case 0:
				Failure = MigrateToV1(pConn.get(), pError, ErrorSize);
				break;
			default:
				str_format(pError, ErrorSize, "No migration from database schema version %d", Current);
				break;
			}

			if(Failure)
			{
				pConn->Disconnect();
				return true;
			}

			if(SetVersion(pConn.get(), Current + 1, pError, ErrorSize))
			{
				pConn->Disconnect();
				return true;
			}
		}

		pConn->Disconnect();
		MarkInitialized(pDbPath);
		return false;
	}
}
