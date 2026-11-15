#pragma once

#include <base/tl/ic_array.h>
#include <base/types.h>

#include <engine/server/databases/connection_pool.h>

#include <sodium/crypto_pwhash.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

constexpr std::size_t MaxAuthAttemptsInTimespan{3};
constexpr std::chrono::seconds MaxAuthAttemptsTimespan{60};
constexpr std::size_t MaxRegistrationAttemptsInTimespan{3};
constexpr std::chrono::seconds RegistrationAttemptsTimespan{60};
constexpr std::size_t UsernameMaxLength{16};
constexpr std::size_t ServerSaltMaxLength{32};
constexpr std::size_t PasswordMaxLength{32};
constexpr std::size_t MaxLegacyHashLength{64};
constexpr std::size_t PasswordHashHexMaxLength{MaxLegacyHashLength + 1};
constexpr int RegistrationIntervalSeconds{300};
constexpr std::chrono::seconds AccountRequestTimeout{30};

struct CAccountCreationResult : ISqlResult
{
	enum class Result
	{
		Unknown,
		Success,
		UsernameAlreadyRegistered,
		FloodProtectedTriggered,
	};

	CAccountCreationResult() = default;
	Result m_Result{};
};

struct CAccountUpdateResult : ISqlResult
{
	CAccountUpdateResult() = default;
};

struct CSqlRegisterDataRequest : ISqlData
{
	CSqlRegisterDataRequest(std::shared_ptr<CAccountCreationResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}

	char m_aUsername[UsernameMaxLength];
	char m_aPasswordHash[crypto_pwhash_STRBYTES];
	char m_aIpStr[NETADDR_MAXSTRSIZE]{};
};

// Cheap pre-hash check so that obviously rejected registrations do not cost an
// argon2 hash. The final INSERT still repeats both checks and reports a UNIQUE
// conflict as a taken username.
struct CAccountRegistrationCheckResult : ISqlResult
{
	CAccountRegistrationCheckResult() = default;

	bool m_Flooded{};
	bool m_UsernameTaken{};
};

struct CSqlRegistrationCheckRequest : ISqlData
{
	CSqlRegistrationCheckRequest(std::shared_ptr<CAccountRegistrationCheckResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}

	char m_aUsername[UsernameMaxLength]{};
	char m_aIpStr[NETADDR_MAXSTRSIZE]{};
};

struct CAccountLoadAuthDataResult : ISqlResult
{
	CAccountLoadAuthDataResult() = default;
	char m_aUsername[UsernameMaxLength];
	char m_aPasswordHash[crypto_pwhash_STRBYTES];
	int m_UserId{};
};

struct CSqlLoginDataRequest : ISqlData
{
	CSqlLoginDataRequest(std::shared_ptr<CAccountLoadAuthDataResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}

	char m_aUsername[UsernameMaxLength];
	int m_UserId{}; // Look up by username if zero.
};

struct CSqlUpdatePasswordHashRequest : ISqlData
{
	CSqlUpdatePasswordHashRequest() :
		ISqlData(std::make_shared<CAccountUpdateResult>())
	{
	}

	int m_UserId{};
	char m_aPasswordHash[crypto_pwhash_STRBYTES];
	bool m_RehashAfterLogin{};
};

struct CSqlAccountAccessRequest : ISqlData
{
	CSqlAccountAccessRequest() :
		ISqlData(std::make_shared<CAccountUpdateResult>())
	{
	}

	int m_UserId{};
	int m_AccessLevel{};
	bool m_SuperAdmin{};
};

struct CAccountsAuthWorker
{
	static bool CheckRegistration(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool RegisterAccount(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool LoadAccountAuthData(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool UpdatePasswordHash(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool SetAccountAccessLevel(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
	static bool ResetAccountAccessLevel(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
};

// Legacy PBKDF2-HMAC-SHA512 password hash. New passwords use argon2id instead.
// pass and salt must be NUL-terminated.
// outputBytes is the desired digest length in bytes.
// hexResult must provide at least outputBytes * 2 + 1 bytes and is always
// NUL-terminated, it is set to the empty string if hashing fails.
void Crypt(const char *pass, const char *salt, int32_t iterations, uint32_t outputBytes, char *hexResult);

namespace Accounts
{
	using CCancelFlag = std::shared_ptr<std::atomic<bool>>;

	struct SPasswordHashResult
	{
		bool m_Success = false;
		std::string m_Hash;
	};

	struct SPasswordVerifyResult
	{
		bool m_Completed = false;
		bool m_Verified = false;
		bool m_NeedsRehash = false;
		std::string m_NewHash;
	};

	void Init(int MaxConcurrentHashes);
	void Shutdown();

	CCancelFlag CreateCancelFlag();

	// The plaintext password is consumed: the callee takes the string over and
	// scrubs it, so no copy is left behind in the caller's string.
	std::future<SPasswordHashResult> HashPasswordAsync(std::string &Password, CCancelFlag pCancel);
	std::future<SPasswordVerifyResult> VerifyPasswordAsync(std::string &Password, std::string StoredHash, std::string Username, std::string ServerPepper, CCancelFlag pCancel);

	SPasswordVerifyResult VerifyPasswordHash(std::string_view Password, std::string_view StoredHash, std::string_view Username, std::string_view ServerPepper);

	bool NeedsRehash(std::string_view PasswordHash);
	bool IsPasswordSuitable(std::string_view Password);
	bool IsLegacyHash(std::string_view PasswordHash);

	std::string TrimCredential(const char *pInput);
}

struct CAbandonedFutures
{
	void Add(std::future<Accounts::SPasswordHashResult> &&Future);
	void Add(std::future<Accounts::SPasswordVerifyResult> &&Future);
	void Prune();

private:
	std::vector<std::future<Accounts::SPasswordHashResult>> m_vHash;
	std::vector<std::future<Accounts::SPasswordVerifyResult>> m_vVerify;
};

enum class EAccountOp
{
	None,
	Register,
	Login,
	ChangePassword,
};

enum class EAccountState
{
	None,
	RegisterCheck,
	RegisterHash,
	RegisterInsert,
	VerifyLoad,
	VerifyCheck,
	ChangeHash,
	ChangeWrite,
};

struct CClientAccountRequests
{
	void Reset(CAbandonedFutures &Abandoned);

	using AuthTimePoint = std::chrono::time_point<std::chrono::steady_clock>;
	icArray<AuthTimePoint, MaxAuthAttemptsInTimespan> mAuthAttempts;
	// Kept across account requests/resets so reconnecting cannot bypass the
	// registration throttle.
	icArray<AuthTimePoint, MaxRegistrationAttemptsInTimespan> m_RegistrationAttempts;

	EAccountOp m_Op{};
	EAccountState m_State{};
	int m_Generation = -1;
	std::chrono::steady_clock::time_point m_Deadline{};

	Accounts::CCancelFlag m_pCancel;
	std::future<Accounts::SPasswordHashResult> m_HashFuture;
	std::future<Accounts::SPasswordVerifyResult> m_VerifyFuture;

	std::shared_ptr<ISqlResult> m_pDbResult;

	std::string m_PendingUsername;
	std::string m_PendingPassword;
	std::string m_PendingNewPassword;
	int m_PendingUserId{};

	// Connection-scoped, not cleared by Reset().
	char m_aIpStr[NETADDR_MAXSTRSIZE]{};
	bool m_RegistrationRequested{};
	bool m_UsingRcon{};
	bool m_IsPasswordSuitable{true};
};
