#pragma once

#include <base/tl/ic_array.h>
#include <base/types.h>

#include <engine/server/databases/connection_pool.h>

#include <chrono>
#include <cstddef>

constexpr std::size_t MaxAuthAttemptsInTimespan{3};
constexpr std::chrono::seconds MaxAuthAttemptsTimespan{60};
constexpr std::size_t UsernameMaxLength{16};
constexpr std::size_t ServerSaltMaxLength{32};
constexpr std::size_t PasswordMaxLength{32};
constexpr std::size_t PasswordHashOutputSize{32};
constexpr std::size_t PasswordHashHexMaxLength{PasswordHashOutputSize * 2 + 1};
constexpr int RegistrationIntervalSeconds{300};

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

struct CSqlRegisterDataRequest : ISqlData
{
	CSqlRegisterDataRequest(std::shared_ptr<CAccountCreationResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}

	char m_aUsername[UsernameMaxLength];
	char m_aPasswordHash[PasswordHashHexMaxLength];
	char m_aIpStr[NETADDR_MAXSTRSIZE]{};
};

struct CAccountLoadAuthDataResult : ISqlResult
{
	CAccountLoadAuthDataResult() = default;
	char m_aUsername[UsernameMaxLength];
	int m_UserId{};
};

struct CSqlLoginDataRequest : ISqlData
{
	CSqlLoginDataRequest(std::shared_ptr<CAccountLoadAuthDataResult> pResult) :
		ISqlData(std::move(pResult))
	{
	}

	char m_aUsername[UsernameMaxLength];
	char m_aPasswordHash[PasswordHashHexMaxLength];
};

struct CAccountsAuthWorker
{
	static bool RegisterAccount(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize);
	static bool LoadAccountAuthData(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize);
};

struct CClientAccountRequests
{
	void Reset()
	{
		m_LoginResult.reset();
		m_RegisterAccountResult.reset();
		m_aIpStr[0] = '\0';
		m_RegistrationRequested = false;
		m_UsingRcon = false;
	}

	using AuthTimePoint = std::chrono::time_point<std::chrono::steady_clock>;
	icArray<AuthTimePoint, MaxAuthAttemptsInTimespan> mAuthAttempts;
	std::shared_ptr<CAccountLoadAuthDataResult> m_LoginResult;
	std::shared_ptr<CAccountCreationResult> m_RegisterAccountResult;
	char m_aIpStr[NETADDR_MAXSTRSIZE]{};
	bool m_RegistrationRequested{};
	bool m_UsingRcon{};
};

class AccountCredentialsHelper
{
public:
	AccountCredentialsHelper(const char *pUsername, const char *pPassword, const char *pServerSalt);
	bool HashGenerated() const;

	char aTrimmedName[UsernameMaxLength]{};
	char aPasswordHash[PasswordHashHexMaxLength]{};
};
