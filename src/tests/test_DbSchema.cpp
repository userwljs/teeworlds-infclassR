#include "test/test.h"
#include <gtest/gtest.h>

#include <base/str.h>

#include <engine/server/accounts.h>
#include <engine/server/databases/connection.h>
#include <engine/server/databases/schema.h>
#include <engine/storage.h>

#include <memory>
#include <string>

static int CountSqliteFiles(const char *pName, int IsDir, int Type, void *pUser)
{
	if(!IsDir)
	{
		const int Length = str_length(pName);
		if(Length > 7 && str_comp(pName + Length - 7, ".sqlite") == 0)
			++*static_cast<int *>(pUser);
	}
	return 0;
}

class CTestDbSchema : public ::testing::Test
{
protected:
	CTestInfo m_TestInfo;
	std::unique_ptr<IStorage> m_pStorage;

	void SetUp() override
	{
		m_TestInfo.m_DeleteTestStorageFilesOnSuccess = true;
		m_pStorage = m_TestInfo.CreateTestStorage();
		ASSERT_NE(m_pStorage, nullptr);
	}

	std::string DbPath(const char *pName) const
	{
		char aBuf[IO_MAX_PATH_LENGTH];
		m_pStorage->GetCompletePath(IStorage::TYPE_SAVE, pName, aBuf, sizeof(aBuf));
		return aBuf;
	}

	static bool Exec(const char *pDbPath, const char *pSql)
	{
		auto pConn = CreateSqliteConnection(pDbPath, false);
		char aError[256];
		if(pConn->Connect(aError, sizeof(aError)))
			return false;

		int NumUpdated = 0;
		const bool Success = !pConn->PrepareStatement(pSql, aError, sizeof(aError)) &&
			!pConn->ExecuteUpdate(&NumUpdated, aError, sizeof(aError));

		pConn->Disconnect();
		return Success;
	}

	static int ReadVersion(IDbConnection *pConn)
	{
		char aSql[128];
		str_format(aSql, sizeof(aSql), "SELECT Version FROM %s_db_version LIMIT 1", pConn->GetPrefix());

		char aError[256];
		if(pConn->PrepareStatement(aSql, aError, sizeof(aError)))
			return -1;

		bool End = false;
		if(pConn->Step(&End, aError, sizeof(aError)) || End)
			return -1;
		return pConn->GetInt(1);
	}

	static bool HasPasswordsRegisterIpIndex(IDbConnection *pConn)
	{
		char aSql[256];
		str_format(aSql, sizeof(aSql),
			"SELECT COUNT(*) FROM pragma_index_list('%s_passwords') WHERE name = '%s_passwords_registerip'",
			pConn->GetPrefix(), pConn->GetPrefix());

		char aError[256];
		if(pConn->PrepareStatement(aSql, aError, sizeof(aError)))
			return false;

		bool End = false;
		if(pConn->Step(&End, aError, sizeof(aError)) || End)
			return false;
		return pConn->GetInt(1) > 0;
	}
};

TEST_F(CTestDbSchema, FreshDatabase)
{
	const std::string Path = DbPath("fresh.sqlite");
	char aError[256];
	ASSERT_FALSE(DbSchema::EnsureCurrent(m_pStorage.get(), Path.c_str(), aError, sizeof(aError))) << aError;

	auto pConn = CreateSqliteConnection(Path.c_str(), false);
	ASSERT_FALSE(pConn->Connect(aError, sizeof(aError)));
	EXPECT_EQ(ReadVersion(pConn.get()), DbSchema::CurrentVersion);

	char aTable[128];
	str_format(aTable, sizeof(aTable), "%s_account_access", pConn->GetPrefix());
	bool Exists = false;
	ASSERT_FALSE(pConn->TableExists(aTable, &Exists, aError, sizeof(aError)));
	EXPECT_TRUE(Exists);

	// The flood protection index is part of the schema.
	EXPECT_TRUE(HasPasswordsRegisterIpIndex(pConn.get()));

	pConn->Disconnect();
}

TEST_F(CTestDbSchema, FloodCheckUsesTheRegisterIpIndex)
{
	// The flood check runs on every registration attempt, so it must be answered
	// by the index instead of scanning the whole passwords table.
	const std::string Path = DbPath("flood-index.sqlite");
	char aError[256];
	ASSERT_FALSE(DbSchema::EnsureCurrent(m_pStorage.get(), Path.c_str(), aError, sizeof(aError))) << aError;

	auto pConn = CreateSqliteConnection(Path.c_str(), false);
	ASSERT_FALSE(pConn->Connect(aError, sizeof(aError)));

	char aSql[512];
	str_format(aSql, sizeof(aSql),
		"EXPLAIN QUERY PLAN "
		"SELECT UserId FROM %s_passwords "
		"WHERE RegisterIp = ? AND ((JULIANDAY(CURRENT_TIMESTAMP) - JULIANDAY(RegisterDate)) * 86400) < 300 "
		"LIMIT 1",
		pConn->GetPrefix());
	ASSERT_FALSE(pConn->PrepareStatement(aSql, aError, sizeof(aError)));
	pConn->BindString(1, "127.0.0.1");

	bool End = false;
	ASSERT_FALSE(pConn->Step(&End, aError, sizeof(aError)));
	ASSERT_FALSE(End);

	char aDetail[256] = "";
	pConn->GetString(4, aDetail, sizeof(aDetail));

	char aIndex[128];
	str_format(aIndex, sizeof(aIndex), "%s_passwords_registerip", pConn->GetPrefix());
	EXPECT_NE(str_find(aDetail, aIndex), nullptr) << aDetail;

	pConn->Disconnect();
}

TEST_F(CTestDbSchema, RegistrationPreCheck)
{
	const std::string Path = DbPath("precheck.sqlite");

	auto pConn = CreateSqliteConnection(Path.c_str(), true);
	char aError[256];
	ASSERT_FALSE(pConn->Connect(aError, sizeof(aError)));

	int NumUpdated = 0;
	ASSERT_FALSE(pConn->PrepareStatement("INSERT INTO accounts_passwords(Username, PasswordHash, RegisterIp) VALUES('alice', 'hash', '10.0.0.1')", aError, sizeof(aError)));
	ASSERT_FALSE(pConn->ExecuteUpdate(&NumUpdated, aError, sizeof(aError)));

	const auto Check = [&](const char *pUsername, const char *pIp, bool *pFlooded, bool *pUsernameTaken) {
		auto pResult = std::make_shared<CAccountRegistrationCheckResult>();
		CSqlRegistrationCheckRequest Data(pResult);
		str_copy(Data.m_aUsername, pUsername);
		str_copy(Data.m_aIpStr, pIp);
		aError[0] = '\0';
		ASSERT_FALSE(CAccountsAuthWorker::CheckRegistration(pConn.get(), &Data, aError, sizeof(aError))) << aError;
		*pFlooded = pResult->m_Flooded;
		*pUsernameTaken = pResult->m_UsernameTaken;
	};

	bool Flooded = false;
	bool UsernameTaken = false;
	Check("alice", "10.0.0.2", &Flooded, &UsernameTaken);
	EXPECT_FALSE(Flooded);
	EXPECT_TRUE(UsernameTaken);

	Check("bob", "10.0.0.1", &Flooded, &UsernameTaken);
	EXPECT_TRUE(Flooded);
	EXPECT_FALSE(UsernameTaken);

	Check("bob", "10.0.0.2", &Flooded, &UsernameTaken);
	EXPECT_FALSE(Flooded);
	EXPECT_FALSE(UsernameTaken);

	pConn->Disconnect();
}

TEST_F(CTestDbSchema, LegacyDatabaseIsBackedUpAndMigrated)
{
	const std::string Path = DbPath("legacy.sqlite");
	const char *pLegacyTable =
		"UserId INTEGER PRIMARY KEY AUTOINCREMENT, "
		"Username VARCHAR(15) COLLATE BINARY UNIQUE NOT NULL, "
		"PasswordHash VARCHAR(64) COLLATE BINARY NOT NULL, "
		"RegisterDate TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
		"RegisterIp VARCHAR(64) NOT NULL";
	char aCreateSql[512];
	str_format(aCreateSql, sizeof(aCreateSql), "CREATE TABLE accounts_passwords (%s)", pLegacyTable);
	ASSERT_TRUE(Exec(Path.c_str(), aCreateSql));
	ASSERT_TRUE(Exec(Path.c_str(), "INSERT INTO accounts_passwords(Username, PasswordHash, RegisterIp) VALUES('alice', 'main-hash', '127.0.0.1')"));

	char aError[256];
	ASSERT_FALSE(DbSchema::EnsureCurrent(m_pStorage.get(), Path.c_str(), aError, sizeof(aError))) << aError;

	auto pConn = CreateSqliteConnection(Path.c_str(), false);
	ASSERT_FALSE(pConn->Connect(aError, sizeof(aError)));
	EXPECT_EQ(ReadVersion(pConn.get()), DbSchema::CurrentVersion);

	const auto ReadPasswordHashType = [&]() {
		char aSql[256];
		str_format(aSql, sizeof(aSql), "SELECT type FROM pragma_table_info('%s_passwords') WHERE name='PasswordHash'", pConn->GetPrefix());
		if(pConn->PrepareStatement(aSql, aError, sizeof(aError)))
			return std::string();
		bool End = false;
		if(pConn->Step(&End, aError, sizeof(aError)) || End)
			return std::string();
		char aType[64] = "";
		pConn->GetString(1, aType, sizeof(aType));
		return std::string(aType);
	};
	const auto ReadCount = [&]() {
		char aCountSql[256];
		str_format(aCountSql, sizeof(aCountSql), "SELECT COUNT(*) FROM %s_passwords", pConn->GetPrefix());
		if(pConn->PrepareStatement(aCountSql, aError, sizeof(aError)))
			return -1;
		bool End = false;
		if(pConn->Step(&End, aError, sizeof(aError)) || End)
			return -1;
		return pConn->GetInt(1);
	};

	EXPECT_EQ(ReadPasswordHashType(), "VARCHAR(128)");
	EXPECT_EQ(ReadCount(), 1);
	// The table rebuild in the migration drops the indexes, they have to be
	// recreated as part of the v1 schema.
	EXPECT_TRUE(HasPasswordsRegisterIpIndex(pConn.get()));

	pConn->Disconnect();

	int BackupCount = 0;
	m_pStorage->ListDirectory(IStorage::TYPE_SAVE, "db-backups", CountSqliteFiles, &BackupCount);
	EXPECT_EQ(BackupCount, 1);
}

TEST_F(CTestDbSchema, FutureVersionIsRejected)
{
	const std::string Path = DbPath("future.sqlite");
	ASSERT_TRUE(Exec(Path.c_str(), "CREATE TABLE accounts_db_version (Version INTEGER NOT NULL)"));

	char aSql[128];
	str_format(aSql, sizeof(aSql), "INSERT INTO accounts_db_version(Version) VALUES(%d)", DbSchema::CurrentVersion + 1);
	ASSERT_TRUE(Exec(Path.c_str(), aSql));

	char aError[256];
	EXPECT_TRUE(DbSchema::EnsureCurrent(m_pStorage.get(), Path.c_str(), aError, sizeof(aError)));
	EXPECT_NE(aError[0], '\0');
}

TEST_F(CTestDbSchema, InitializationRunsOnlyOncePerPath)
{
	const std::string Path = DbPath("once.sqlite");
	char aError[256];
	ASSERT_FALSE(DbSchema::EnsureCurrent(m_pStorage.get(), Path.c_str(), aError, sizeof(aError))) << aError;
	EXPECT_FALSE(DbSchema::EnsureCurrent(m_pStorage.get(), Path.c_str(), aError, sizeof(aError)));
}

TEST_F(CTestDbSchema, VersionTableWithoutPasswordsTableIsRepaired)
{
	// A database that has a version table but no account tables (e.g. after a
	// partially restored backup) must not break startup: the missing tables are
	// created by the connection setup before the version is migrated.
	const std::string Path = DbPath("broken.sqlite");
	ASSERT_TRUE(Exec(Path.c_str(), "CREATE TABLE accounts_db_version (Version INTEGER NOT NULL)"));
	ASSERT_TRUE(Exec(Path.c_str(), "INSERT INTO accounts_db_version(Version) VALUES(0)"));

	char aError[256];
	ASSERT_FALSE(DbSchema::EnsureCurrent(m_pStorage.get(), Path.c_str(), aError, sizeof(aError))) << aError;

	auto pConn = CreateSqliteConnection(Path.c_str(), false);
	ASSERT_FALSE(pConn->Connect(aError, sizeof(aError)));
	EXPECT_EQ(ReadVersion(pConn.get()), DbSchema::CurrentVersion);

	char aTable[128];
	str_format(aTable, sizeof(aTable), "%s_passwords", pConn->GetPrefix());
	bool Exists = false;
	ASSERT_FALSE(pConn->TableExists(aTable, &Exists, aError, sizeof(aError)));
	EXPECT_TRUE(Exists);

	// The empty table can be used for a registration.
	auto pResult = std::make_shared<CAccountCreationResult>();
	CSqlRegisterDataRequest Data(pResult);
	str_copy(Data.m_aUsername, "alice");
	str_copy(Data.m_aPasswordHash, "$argon2id$v=19$m=65536,t=2,p=1$c2FsdA$aGFzaA");
	ASSERT_FALSE(CAccountsAuthWorker::RegisterAccount(pConn.get(), &Data, aError, sizeof(aError)));
	EXPECT_EQ(pResult->m_Result, CAccountCreationResult::Result::Success);

	pConn->Disconnect();
}

TEST_F(CTestDbSchema, StaleMigrationTableIsReplaced)
{
	// A table left behind by an interrupted migration must not break the retry.
	const std::string Path = DbPath("stale.sqlite");
	ASSERT_TRUE(Exec(Path.c_str(),
		"CREATE TABLE accounts_passwords ("
		"UserId INTEGER PRIMARY KEY AUTOINCREMENT, "
		"Username VARCHAR(15) COLLATE BINARY UNIQUE NOT NULL, "
		"PasswordHash VARCHAR(64) COLLATE BINARY NOT NULL, "
		"RegisterDate TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
		"RegisterIp VARCHAR(64) NOT NULL)"));
	ASSERT_TRUE(Exec(Path.c_str(),
		"CREATE TABLE accounts_passwords_new ("
		"UserId INTEGER PRIMARY KEY AUTOINCREMENT, "
		"Username VARCHAR(15) COLLATE BINARY UNIQUE NOT NULL, "
		"PasswordHash VARCHAR(128) COLLATE BINARY NOT NULL, "
		"RegisterDate TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, "
		"RegisterIp VARCHAR(64) NOT NULL)"));

	char aError[256];
	ASSERT_FALSE(DbSchema::EnsureCurrent(m_pStorage.get(), Path.c_str(), aError, sizeof(aError))) << aError;

	auto pConn = CreateSqliteConnection(Path.c_str(), false);
	ASSERT_FALSE(pConn->Connect(aError, sizeof(aError)));
	EXPECT_EQ(ReadVersion(pConn.get()), DbSchema::CurrentVersion);

	char aSql[256];
	str_format(aSql, sizeof(aSql), "SELECT type FROM pragma_table_info('%s_passwords') WHERE name='PasswordHash'", pConn->GetPrefix());
	ASSERT_FALSE(pConn->PrepareStatement(aSql, aError, sizeof(aError)));
	bool End = false;
	ASSERT_FALSE(pConn->Step(&End, aError, sizeof(aError)));
	ASSERT_FALSE(End);
	char aType[64] = "";
	pConn->GetString(1, aType, sizeof(aType));
	EXPECT_STREQ(aType, "VARCHAR(128)");

	pConn->Disconnect();
}
