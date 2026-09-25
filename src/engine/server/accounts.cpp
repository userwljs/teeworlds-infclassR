#include "accounts.h"

#include <base/dbg.h>
#include <base/log.h>
#include <base/str.h>

#include <engine/server/databases/connection.h>

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <sodium.h>

#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

constexpr auto OpsLimit = crypto_pwhash_OPSLIMIT_INTERACTIVE;
constexpr auto MemLimit = crypto_pwhash_MEMLIMIT_INTERACTIVE;

void Crypt(const char *pass, const char *salt, int32_t iterations, uint32_t outputBytes, char *hexResult)
{
	if(hexResult == nullptr)
		return;

	if(outputBytes == 0)
	{
		hexResult[0] = '\0';
		return;
	}

	std::vector<unsigned char> Digest(outputBytes);
	if(PKCS5_PBKDF2_HMAC(pass, str_length(pass), reinterpret_cast<const unsigned char *>(salt), str_length(salt), iterations, EVP_sha512(), outputBytes, Digest.data()) != 1)
	{
		hexResult[0] = '\0';
		return;
	}

	static constexpr char s_aHex[] = "0123456789abcdef";
	for(uint32_t i = 0; i < outputBytes; ++i)
	{
		hexResult[i * 2] = s_aHex[Digest[i] >> 4];
		hexResult[i * 2 + 1] = s_aHex[Digest[i] & 0x0F];
	}
	hexResult[outputBytes * 2] = '\0';

	sodium_memzero(Digest.data(), Digest.size());
}

namespace
{
	// Both the pre-hash check and the registration INSERT have to test the
	// flood protection and the username uniqueness, so the SQL lives here once
	// instead of being duplicated (and drifting apart) in both places.
	bool CheckRegistrationFlood(IDbConnection *pSqlServer, const char *pIpStr, bool *pFlooded, char *pError, int ErrorSize)
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf),
			"SELECT UserId FROM %s_passwords "
			"WHERE RegisterIp = ? AND ((JULIANDAY(CURRENT_TIMESTAMP) - JULIANDAY(RegisterDate)) * 86400) < ? "
			"LIMIT 1",
			pSqlServer->GetPrefix());

		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
			return true;
		pSqlServer->BindString(1, pIpStr);
		pSqlServer->BindInt(2, RegistrationIntervalSeconds);

		bool End = false;
		if(pSqlServer->Step(&End, pError, ErrorSize))
			return true;
		*pFlooded = !End;
		return false;
	}

	bool CheckUsernameTaken(IDbConnection *pSqlServer, const char *pUsername, bool *pTaken, char *pError, int ErrorSize)
	{
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf),
			"SELECT UserId FROM %s_passwords "
			"WHERE Username = ? "
			"LIMIT 1",
			pSqlServer->GetPrefix());

		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
			return true;
		pSqlServer->BindString(1, pUsername);

		bool End = false;
		if(pSqlServer->Step(&End, pError, ErrorSize))
			return true;
		*pTaken = !End;
		return false;
	}
}

bool CAccountsAuthWorker::CheckRegistration(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlRegistrationCheckRequest *>(pGameData);
	if(pData == nullptr)
		return true;
	auto *pResult = dynamic_cast<CAccountRegistrationCheckResult *>(pGameData->m_pResult.get());
	if(pResult == nullptr)
		return true;

	// Check for registration flooding
	bool Flooded = false;
	if(CheckRegistrationFlood(pSqlServer, pData->m_aIpStr, &Flooded, pError, ErrorSize))
		return true;
	if(Flooded)
	{
		pResult->m_Flooded = true;
		return false;
	}

	// Check whether the username is already registered
	bool UsernameTaken = false;
	if(CheckUsernameTaken(pSqlServer, pData->m_aUsername, &UsernameTaken, pError, ErrorSize))
		return true;
	pResult->m_UsernameTaken = UsernameTaken;

	return false;
}

bool CAccountsAuthWorker::RegisterAccount(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlRegisterDataRequest *>(pGameData);
	auto *pResult = dynamic_cast<CAccountCreationResult *>(pGameData->m_pResult.get());
	char aBuf[512];

	// The write succeeded on the actual database, drop the staging row.
	if(w == Write::NORMAL_SUCCEEDED)
	{
		str_format(aBuf, sizeof(aBuf), "DELETE FROM %s_passwords_backup WHERE Username=?", pSqlServer->GetPrefix());
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindString(1, pData->m_aUsername);
		int NumDeleted = 0;
		return pSqlServer->ExecuteUpdate(&NumDeleted, pError, ErrorSize);
	}

	// The write failed, keep the staging row so the registration is not lost.
	if(w == Write::NORMAL_FAILED)
	{
		return false;
	}

	// Check for registration flooding
	bool Flooded = false;
	if(CheckRegistrationFlood(pSqlServer, pData->m_aIpStr, &Flooded, pError, ErrorSize))
		return true;
	if(Flooded)
	{
		pResult->m_Result = CAccountCreationResult::Result::FloodProtectedTriggered;
		return false;
	}

	// Check whether the username is already registered
	bool UsernameTaken = false;
	if(CheckUsernameTaken(pSqlServer, pData->m_aUsername, &UsernameTaken, pError, ErrorSize))
		return true;
	if(UsernameTaken)
	{
		pResult->m_Result = CAccountCreationResult::Result::UsernameAlreadyRegistered;
		return false;
	}

	// Add the registration record
	{
		str_format(aBuf, sizeof(aBuf),
			"INSERT INTO %s_passwords%s("
			"	Username, PasswordHash, RegisterIp) "
			"VALUES (?, ?, ?)",
			pSqlServer->GetPrefix(), w == Write::NORMAL ? "" : "_backup");
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindString(1, pData->m_aUsername);
		pSqlServer->BindString(2, pData->m_aPasswordHash);
		pSqlServer->BindString(3, pData->m_aIpStr);
		pSqlServer->Print();
	}

	int NumInserted = 0;
	if(pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize))
	{
		// The check above and the INSERT are not atomic. If a concurrent
		// registration won the race, the UNIQUE constraint fails the INSERT and
		// the client has to be told about the taken username instead of seeing a
		// generic database error.
		bool TakenAfterFailure = false;
		char aCheckError[256] = "";
		if(!CheckUsernameTaken(pSqlServer, pData->m_aUsername, &TakenAfterFailure, aCheckError, sizeof(aCheckError)) && TakenAfterFailure)
		{
			pResult->m_Result = CAccountCreationResult::Result::UsernameAlreadyRegistered;
			return false;
		}
		return true;
	}

	pResult->m_Result = CAccountCreationResult::Result::Success;
	return false;
}

bool CAccountsAuthWorker::LoadAccountAuthData(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlLoginDataRequest *>(pGameData);
	auto *pResult = dynamic_cast<CAccountLoadAuthDataResult *>(pGameData->m_pResult.get());

	char aBuf[512];
	if(pData->m_UserId > 0)
	{
		str_format(aBuf, sizeof(aBuf),
			"SELECT UserId, Username, PasswordHash FROM %s_passwords WHERE UserId=? LIMIT 1",
			pSqlServer->GetPrefix());
	}
	else
	{
		str_format(aBuf, sizeof(aBuf),
			"SELECT UserId, Username, PasswordHash FROM %s_passwords WHERE Username=? LIMIT 1",
			pSqlServer->GetPrefix());
	}
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	if(pData->m_UserId > 0)
	{
		pSqlServer->BindInt(1, pData->m_UserId);
	}
	else
	{
		pSqlServer->BindString(1, pData->m_aUsername);
	}

	bool End = false;
	if(pSqlServer->Step(&End, pError, ErrorSize))
	{
		return true;
	}
	if(!End)
	{
		pResult->m_UserId = pSqlServer->GetInt(1);
		pSqlServer->GetString(2, pResult->m_aUsername, sizeof(pResult->m_aUsername));
		pSqlServer->GetString(3, pResult->m_aPasswordHash, sizeof(pResult->m_aPasswordHash));
	}

	return false;
}

bool CAccountsAuthWorker::UpdatePasswordHash(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlUpdatePasswordHashRequest *>(pGameData);
	char aBuf[512];
	int NumUpdated = 0;

	if(w == Write::NORMAL_SUCCEEDED)
	{
		str_format(aBuf, sizeof(aBuf), "DELETE FROM %s_passwords_backup WHERE UserId=?", pSqlServer->GetPrefix());
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindInt(1, pData->m_UserId);
		return pSqlServer->ExecuteUpdate(&NumUpdated, pError, ErrorSize);
	}

	if(w == Write::NORMAL_FAILED)
	{
		return false;
	}

	str_format(aBuf, sizeof(aBuf),
		"UPDATE %s_passwords%s SET PasswordHash=? WHERE UserId=?",
		pSqlServer->GetPrefix(), w == Write::NORMAL ? "" : "_backup");
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		if(pData->m_RehashAfterLogin)
			log_warn("accounts", "can't rehash the password of UserId %d: %s", pData->m_UserId, pError);
		return true;
	}
	pSqlServer->BindString(1, pData->m_aPasswordHash);
	pSqlServer->BindInt(2, pData->m_UserId);
	if(pSqlServer->ExecuteUpdate(&NumUpdated, pError, ErrorSize))
	{
		if(pData->m_RehashAfterLogin)
			log_warn("accounts", "can't rehash the password of UserId %d: %s", pData->m_UserId, pError);
		return true;
	}
	if(pData->m_RehashAfterLogin && NumUpdated == 0)
		log_warn("accounts", "the password of UserId %d was not rehashed (no such account row)", pData->m_UserId);
	return false;
}

bool CAccountsAuthWorker::SetAccountAccessLevel(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlAccountAccessRequest *>(pGameData);
	char aBuf[512];
	int NumUpdated = 0;

	if(w == Write::NORMAL_SUCCEEDED)
	{
		str_format(aBuf, sizeof(aBuf), "DELETE FROM %s_account_access_backup WHERE UserId=?", pSqlServer->GetPrefix());
		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindInt(1, pData->m_UserId);
		return pSqlServer->ExecuteUpdate(&NumUpdated, pError, ErrorSize);
	}

	if(w == Write::NORMAL_FAILED)
	{
		return false;
	}

	str_format(aBuf, sizeof(aBuf),
		"INSERT INTO %s_account_access%s(UserId, AccessLevel, SuperAdmin) VALUES(?, ?, ?) "
		"ON CONFLICT(UserId) DO UPDATE SET AccessLevel=excluded.AccessLevel, SuperAdmin=excluded.SuperAdmin",
		pSqlServer->GetPrefix(), w == Write::NORMAL ? "" : "_backup");
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->BindInt(1, pData->m_UserId);
	pSqlServer->BindInt(2, pData->m_AccessLevel);
	pSqlServer->BindInt(3, pData->m_SuperAdmin ? 1 : 0);
	return pSqlServer->ExecuteUpdate(&NumUpdated, pError, ErrorSize);
}

bool CAccountsAuthWorker::ResetAccountAccessLevel(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlAccountAccessRequest *>(pGameData);
	char aBuf[512];
	int NumDeleted = 0;

	if(w == Write::NORMAL_SUCCEEDED || w == Write::NORMAL_FAILED)
	{
		return false;
	}

	str_format(aBuf, sizeof(aBuf), "DELETE FROM %s_account_access%s WHERE UserId=?", pSqlServer->GetPrefix(), w == Write::NORMAL ? "" : "_backup");
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->BindInt(1, pData->m_UserId);
	return pSqlServer->ExecuteUpdate(&NumDeleted, pError, ErrorSize);
}

namespace Accounts
{
	namespace
	{
		struct SodiumFree
		{
			void operator()(char *p) const noexcept
			{
				if(p)
					sodium_free(p);
			}
		};

		// A fixed number of workers bounds both the number of concurrent argon2
		// computations and the number of OS threads. The queue size is bounded
		// separately so that a flood of requests cannot exhaust memory either.
		constexpr std::size_t MaxQueuedPasswordOperations{256};

		class CPasswordWorkerPool
		{
		public:
			~CPasswordWorkerPool() { Stop(); }

			void Start(int NumWorkers)
			{
				Stop();
				NumWorkers = std::max(NumWorkers, 1);
				{
					const std::lock_guard<std::mutex> Lock(m_Mutex);
					m_Shutdown = false;
				}
				m_vWorkers.reserve(NumWorkers);
				for(int i = 0; i < NumWorkers; ++i)
					m_vWorkers.emplace_back([this]() { WorkerLoop(); });
			}

			void Stop()
			{
				{
					const std::lock_guard<std::mutex> Lock(m_Mutex);
					m_Shutdown = true;
				}
				m_Condition.notify_all();
				for(std::thread &Worker : m_vWorkers)
				{
					if(Worker.joinable())
						Worker.join();
				}
				m_vWorkers.clear();
			}

			template<typename Result, typename F>
			std::future<Result> Submit(F &&Task)
			{
				auto pPromise = std::make_shared<std::promise<Result>>();
				auto pTask = std::make_shared<std::decay_t<F>>(std::forward<F>(Task));
				std::future<Result> Future = pPromise->get_future();

				bool Queued = false;
				{
					const std::lock_guard<std::mutex> Lock(m_Mutex);
					if(!m_Shutdown && !m_vWorkers.empty() && m_Queue.size() < MaxQueuedPasswordOperations)
					{
						m_Queue.emplace_back([pPromise, pTask]() {
							try
							{
								pPromise->set_value((*pTask)());
							}
							catch(...)
							{
								pPromise->set_value(Result{});
							}
						});
						Queued = true;
					}
				}

				if(Queued)
					m_Condition.notify_one();
				else
					pPromise->set_value(Result{});
				return Future;
			}

		private:
			void WorkerLoop()
			{
				while(true)
				{
					std::function<void()> Task;
					{
						std::unique_lock<std::mutex> Lock(m_Mutex);
						m_Condition.wait(Lock, [this]() { return m_Shutdown || !m_Queue.empty(); });
						if(m_Shutdown && m_Queue.empty())
							return;

						Task = std::move(m_Queue.front());
						m_Queue.pop_front();
					}
					Task();
				}
			}

			std::mutex m_Mutex;
			std::condition_variable m_Condition;
			std::deque<std::function<void()>> m_Queue;
			std::vector<std::thread> m_vWorkers;
			bool m_Shutdown = false;
		};

		CPasswordWorkerPool g_PasswordWorkers;
		std::atomic<bool> g_Shutdown{false};

		std::unique_ptr<char, SodiumFree> CopySecret(std::string &Password)
		{
			const std::size_t Length = Password.size();
			std::unique_ptr<char, SodiumFree> pSecret(static_cast<char *>(sodium_malloc(Length > 0 ? Length : 1)));
			dbg_assert(pSecret != nullptr, "sodium_malloc failed");

			if(Length > 0)
			{
				memcpy(pSecret.get(), Password.data(), Length);
				sodium_memzero(Password.data(), Length);
			}
			return pSecret;
		}
	}

	void Init(int MaxConcurrentHashes)
	{
		MaxConcurrentHashes = std::max(MaxConcurrentHashes, 1);
		g_Shutdown = false;
		g_PasswordWorkers.Start(MaxConcurrentHashes);
	}

	void Shutdown()
	{
		g_Shutdown = true;
		g_PasswordWorkers.Stop();
	}

	CCancelFlag CreateCancelFlag()
	{
		return std::make_shared<std::atomic<bool>>(false);
	}

	std::future<SPasswordHashResult> HashPasswordAsync(std::string &Password, CCancelFlag pCancel)
	{
		// Take the plaintext over instead of moving it: a moved-from std::string
		// can still keep the bytes of a short password in its internal buffer.
		std::string Secret;
		Secret.swap(Password);
		const std::size_t Length = Secret.size();
		auto pSecret = CopySecret(Secret);
		if(!pCancel)
			pCancel = CreateCancelFlag();

		return g_PasswordWorkers.Submit<SPasswordHashResult>([pSecret = std::move(pSecret), Length, pCancel]() -> SPasswordHashResult
		{
			if(g_Shutdown || pCancel->load())
				return {};

			char aOut[crypto_pwhash_STRBYTES];
			const int Error = crypto_pwhash_str(aOut, pSecret.get(), Length, OpsLimit, MemLimit);
			if(Error != 0)
			{
				log_error("accounts", "crypto_pwhash_str failed (error %d)", Error);
				return {};
			}

			SPasswordHashResult Result;
			Result.m_Success = true;
			Result.m_Hash = aOut;
			sodium_memzero(aOut, sizeof(aOut));
			return Result;
		});
	}

	std::future<SPasswordVerifyResult> VerifyPasswordAsync(std::string &Password, std::string StoredHash, std::string Username, std::string ServerPepper, CCancelFlag pCancel)
	{
		// Take the plaintext over instead of moving it: a moved-from std::string
		// can still keep the bytes of a short password in its internal buffer.
		std::string Secret;
		Secret.swap(Password);
		const std::size_t Length = Secret.size();
		auto pSecret = CopySecret(Secret);
		if(!pCancel)
			pCancel = CreateCancelFlag();

		return g_PasswordWorkers.Submit<SPasswordVerifyResult>([pSecret = std::move(pSecret), Length, StoredHash = std::move(StoredHash), Username = std::move(Username), ServerPepper = std::move(ServerPepper), pCancel]() -> SPasswordVerifyResult
		{
			if(g_Shutdown || pCancel->load())
				return {};

			SPasswordVerifyResult Result = VerifyPasswordHash(std::string_view(pSecret.get(), Length), StoredHash, Username, ServerPepper);
			if(!Result.m_Completed || !Result.m_Verified || !Result.m_NeedsRehash)
				return Result;

			if(pCancel->load())
			{
				Result.m_NeedsRehash = false;
				Result.m_NewHash.clear();
				return Result;
			}

			char aOut[crypto_pwhash_STRBYTES];
			const int Error = crypto_pwhash_str(aOut, pSecret.get(), Length, OpsLimit, MemLimit);
			if(Error != 0)
			{
				log_error("accounts", "crypto_pwhash_str failed during rehash (error %d)", Error);
				Result.m_NeedsRehash = false;
				return Result;
			}

			Result.m_NewHash = aOut;
			sodium_memzero(aOut, sizeof(aOut));
			return Result;
		});
	}

	// Performs the cost of one password verification without anything to verify.
	// Used for unknown usernames and malformed stored hashes so that the response
	// time does not reveal whether an account exists.
	void RunDummyVerify()
	{
		char aBuf[crypto_pwhash_STRBYTES];
		constexpr std::string_view DummyPassword = "12345678";
		const int Error = crypto_pwhash_str(aBuf, DummyPassword.data(), DummyPassword.size(), OpsLimit, MemLimit);
		if(Error != 0)
			log_error("accounts", "crypto_pwhash_str failed during dummy verification (error %d)", Error);
	}

	SPasswordVerifyResult VerifyPasswordHash(std::string_view Password, std::string_view StoredHash, std::string_view Username, std::string_view ServerPepper)
	{
		SPasswordVerifyResult Result;
		Result.m_Completed = true;

		if(StoredHash.empty())
		{
			RunDummyVerify();
			return Result;
		}

		if(IsLegacyHash(StoredHash))
		{
			if(StoredHash.size() % 2 != 0 || StoredHash.size() > MaxLegacyHashLength)
			{
				RunDummyVerify();
				return Result;
			}

			char aSalt[128];
			str_format(aSalt, sizeof(aSalt), "%s_ic_%s", std::string(Username).c_str(), std::string(ServerPepper).c_str());

			char aHex[PasswordHashHexMaxLength];
			Crypt(std::string(Password).c_str(), aSalt, 2, static_cast<uint32_t>(StoredHash.size() / 2), aHex);

			Result.m_Verified = str_length(aHex) == static_cast<int>(StoredHash.size()) && sodium_memcmp(aHex, StoredHash.data(), StoredHash.size()) == 0;
			Result.m_NeedsRehash = Result.m_Verified;
			if(!Result.m_Verified)
			{
				// Legacy hashing is much cheaper than the argon2 verification of a
				// current account, so the dummy work keeps wrong passwords on legacy
				// accounts from being distinguishable by their response time.
				RunDummyVerify();
			}
			return Result;
		}

		if(StoredHash.size() >= crypto_pwhash_STRBYTES)
		{
			RunDummyVerify();
			return Result;
		}

		const std::string HashStr(StoredHash);
		const std::string PasswordStr(Password);
		Result.m_Verified = crypto_pwhash_str_verify(HashStr.c_str(), PasswordStr.c_str(), PasswordStr.size()) == 0;
		if(Result.m_Verified)
			Result.m_NeedsRehash = NeedsRehash(StoredHash);
		return Result;
	}

	bool NeedsRehash(std::string_view PasswordHash)
	{
		if(PasswordHash.empty())
			return false;
		if(IsLegacyHash(PasswordHash))
			return true;

		const std::string HashStr(PasswordHash);
		const int Result = crypto_pwhash_str_needs_rehash(HashStr.c_str(), OpsLimit, MemLimit);
		return Result == 1;
	}

	bool IsPasswordSuitable(std::string_view Password)
	{
		constexpr icArray<char, 32> AllowedSymbols
		{
			'!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/', ':', ';', '<', '=', '>', '?',
			'@', '[', '\\', ']', '^', '_', '`', '{', '|', '}', '~'
		};
		if(Password.length() < 8)
		{
			return false;
		}
		bool HasDigit = false;
		bool HasLower = false;
		bool HasUpper = false;
		bool HasSymbol = false;
		for(const auto &c : Password)
		{
			if(c >= '0' && c <= '9')
			{
				HasDigit = true;
			}
			else if(c >= 'a' && c <= 'z')
			{
				HasLower = true;
			}
			else if(c >= 'A' && c <= 'Z')
			{
				HasUpper = true;
			}
			else if(AllowedSymbols.Contains(c))
			{
				HasSymbol = true;
			}
			else
			{
				return false;
			}
		}
		if(HasDigit + HasLower + HasUpper + HasSymbol < 3)
		{
			return false;
		}
		return true;
	}

	bool IsLegacyHash(std::string_view PasswordHash)
	{
		return !PasswordHash.empty() && PasswordHash[0] != '$';
	}

	std::string TrimCredential(const char *pInput)
	{
		std::string Value(pInput);
		char *pStart = Value.data();
		pStart = const_cast<char *>(str_utf8_skip_whitespaces(pStart));
		str_utf8_trim_right(pStart);
		std::string Credential(pStart);
		// The input is a password: do not leave the untrimmed copy behind.
		if(!Value.empty())
			sodium_memzero(Value.data(), Value.size());
		return Credential;
	}
}

void CAbandonedFutures::Add(std::future<Accounts::SPasswordHashResult> &&Future)
{
	if(Future.valid())
		m_vHash.push_back(std::move(Future));
}

void CAbandonedFutures::Add(std::future<Accounts::SPasswordVerifyResult> &&Future)
{
	if(Future.valid())
		m_vVerify.push_back(std::move(Future));
}

void CAbandonedFutures::Prune()
{
	std::erase_if(m_vHash, [](std::future<Accounts::SPasswordHashResult> &Future) {
		return !Future.valid() || Future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
	});
	std::erase_if(m_vVerify, [](std::future<Accounts::SPasswordVerifyResult> &Future) {
		return !Future.valid() || Future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
	});
}

void CClientAccountRequests::Reset(CAbandonedFutures &Abandoned)
{
	if(m_pCancel)
		m_pCancel->store(true);

	Abandoned.Add(std::move(m_HashFuture));
	Abandoned.Add(std::move(m_VerifyFuture));

	if(!m_PendingPassword.empty())
		sodium_memzero(m_PendingPassword.data(), m_PendingPassword.size());
	if(!m_PendingNewPassword.empty())
		sodium_memzero(m_PendingNewPassword.data(), m_PendingNewPassword.size());

	m_Op = EAccountOp::None;
	m_State = EAccountState::None;
	m_Generation = -1;
	m_Deadline = {};
	m_pCancel.reset();
	m_pDbResult.reset();
	m_PendingUsername.clear();
	m_PendingPassword.clear();
	m_PendingNewPassword.clear();
	m_PendingUserId = 0;
	// m_aIpStr is connection-scoped: Reset is also called when an account
	// request finishes, so clearing it here would lose the registration IP.
	// m_RegistrationAttempts is intentionally kept as well.
	m_RegistrationRequested = false;
	m_UsingRcon = false;
	m_IsPasswordSuitable = true;
}
