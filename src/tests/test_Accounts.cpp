#include "test/test.h"
#include <gtest/gtest.h>

#include <base/str.h>

#include <engine/server/accounts.h>

#include <sodium.h>

#include <chrono>
#include <cstring>
#include <future>
#include <string>
#include <thread>

class CTestAccounts : public ::testing::Test
{
protected:
	static void SetUpTestSuite()
	{
		ASSERT_FALSE(sodium_init() < 0);
		Accounts::Init(1);
	}

	static void TearDownTestSuite()
	{
		Accounts::Shutdown();
	}
};

TEST_F(CTestAccounts, CryptDesiredLength)
{
	char aBuf[PasswordHashHexMaxLength];
	Crypt("pw", "salt", 2, 24, aBuf);
	EXPECT_STREQ(aBuf, "8816cc3aaf71373639c3415049b2b6572f8d2bad8d7641ff");
}

TEST_F(CTestAccounts, CryptPrefixProperty)
{
	char aHex24[PasswordHashHexMaxLength];
	char aHex32[PasswordHashHexMaxLength];
	Crypt("pw", "salt", 2, 24, aHex24);
	Crypt("pw", "salt", 2, 32, aHex32);
	EXPECT_EQ(std::string(aHex32).substr(0, 48), std::string(aHex24));
}

TEST_F(CTestAccounts, CryptZeroLength)
{
	char aBuf[4] = {'x', 'x', 'x', 'x'};
	Crypt("pw", "salt", 2, 0, aBuf);
	EXPECT_EQ(aBuf[0], '\0');
}

TEST_F(CTestAccounts, CryptDoesNotOverrunBuffer)
{
	char aBuf[PasswordHashHexMaxLength + 1];
	memset(aBuf, 0x7f, sizeof(aBuf));
	Crypt("pw", "salt", 2, 24, aBuf);
	EXPECT_EQ(static_cast<unsigned char>(aBuf[24 * 2 + 1]), 0x7f);
}

TEST_F(CTestAccounts, IsLegacyHash)
{
	EXPECT_TRUE(Accounts::IsLegacyHash("deadbeef"));
	EXPECT_FALSE(Accounts::IsLegacyHash("$argon2id$v=19$m=65536,t=2,p=1$c2FsdA$aGFzaA"));
	EXPECT_FALSE(Accounts::IsLegacyHash(""));
}

TEST_F(CTestAccounts, IsPasswordSuitable)
{
	EXPECT_FALSE(Accounts::IsPasswordSuitable("short1A"));
	EXPECT_FALSE(Accounts::IsPasswordSuitable("abcdefghijkl"));
	EXPECT_FALSE(Accounts::IsPasswordSuitable("abcdefghijkl1"));
	EXPECT_FALSE(Accounts::IsPasswordSuitable("abcdefghijkl!"));
	// At least three of the four character classes are required.
	EXPECT_TRUE(Accounts::IsPasswordSuitable("Password123"));
	EXPECT_TRUE(Accounts::IsPasswordSuitable("Password123!"));
	EXPECT_FALSE(Accounts::IsPasswordSuitable("Password 123!"));
}

TEST_F(CTestAccounts, VerifyLegacyHash)
{
	char aLegacy[PasswordHashHexMaxLength];
	char aSalt[128];
	str_format(aSalt, sizeof(aSalt), "%s_ic_%s", "alice", "pepper");
	Crypt("Password123", aSalt, 2, 24, aLegacy);

	const Accounts::SPasswordVerifyResult Result = Accounts::VerifyPasswordHash("Password123", aLegacy, "alice", "pepper");
	EXPECT_TRUE(Result.m_Completed);
	EXPECT_TRUE(Result.m_Verified);
	EXPECT_TRUE(Result.m_NeedsRehash);

	EXPECT_FALSE(Accounts::VerifyPasswordHash("Password124", aLegacy, "alice", "pepper").m_Verified);
	EXPECT_FALSE(Accounts::VerifyPasswordHash("Password123", aLegacy, "bob", "pepper").m_Verified);
	EXPECT_TRUE(Accounts::VerifyPasswordHash("Password123", "abc", "alice", "pepper").m_Completed);
	EXPECT_FALSE(Accounts::VerifyPasswordHash("Password123", "abc", "alice", "pepper").m_Verified);
}

TEST_F(CTestAccounts, VerifyArgon2Hash)
{
	std::string Password = "Password123";
	std::future<Accounts::SPasswordHashResult> Future = Accounts::HashPasswordAsync(Password, nullptr);
	const Accounts::SPasswordHashResult HashResult = Future.get();
	ASSERT_TRUE(HashResult.m_Success);
	ASSERT_FALSE(HashResult.m_Hash.empty());
	EXPECT_EQ(HashResult.m_Hash[0], '$');

	const Accounts::SPasswordVerifyResult Result = Accounts::VerifyPasswordHash("Password123", HashResult.m_Hash, "alice", "pepper");
	EXPECT_TRUE(Result.m_Completed);
	EXPECT_TRUE(Result.m_Verified);
	EXPECT_FALSE(Result.m_NeedsRehash);

	EXPECT_FALSE(Accounts::VerifyPasswordHash("Password124", HashResult.m_Hash, "alice", "pepper").m_Verified);
}

TEST_F(CTestAccounts, AsyncHashConsumesPassword)
{
	// The plaintext must not stay in the caller's string: a moved-from
	// std::string can still hold the bytes of a short password.
	Accounts::CCancelFlag pCancel = Accounts::CreateCancelFlag();
	pCancel->store(true);

	std::string Password = "Password123";
	std::future<Accounts::SPasswordHashResult> Future = Accounts::HashPasswordAsync(Password, pCancel);
	EXPECT_TRUE(Password.empty());
	EXPECT_FALSE(Future.get().m_Success);
}

TEST_F(CTestAccounts, AsyncVerifyConsumesPassword)
{
	Accounts::CCancelFlag pCancel = Accounts::CreateCancelFlag();
	pCancel->store(true);

	std::string Password = "Password123";
	std::future<Accounts::SPasswordVerifyResult> Future = Accounts::VerifyPasswordAsync(Password, "", "alice", "pepper", pCancel);
	EXPECT_TRUE(Password.empty());
	EXPECT_FALSE(Future.get().m_Completed);
}

TEST_F(CTestAccounts, VerifyAsyncRehashesLegacyHash)
{
	char aLegacy[PasswordHashHexMaxLength];
	char aSalt[128];
	str_format(aSalt, sizeof(aSalt), "%s_ic_%s", "alice", "pepper");
	Crypt("Password123", aSalt, 2, 24, aLegacy);

	std::string Password = "Password123";
	std::future<Accounts::SPasswordVerifyResult> Future = Accounts::VerifyPasswordAsync(Password, aLegacy, "alice", "pepper", nullptr);
	const Accounts::SPasswordVerifyResult Result = Future.get();
	ASSERT_TRUE(Result.m_Completed);
	ASSERT_TRUE(Result.m_Verified);
	ASSERT_TRUE(Result.m_NeedsRehash);
	ASSERT_FALSE(Result.m_NewHash.empty());
	EXPECT_EQ(Result.m_NewHash[0], '$');
	EXPECT_TRUE(Accounts::VerifyPasswordHash("Password123", Result.m_NewHash, "alice", "pepper").m_Verified);
}

TEST_F(CTestAccounts, VerifyUnknownUsernameDoesDummyWork)
{
	// An empty stored hash means the username was not found. The verification must
	// report a completed but unverified result, and it must still have performed a
	// dummy verification so that the response time does not reveal the username.
	const Accounts::SPasswordVerifyResult Result = Accounts::VerifyPasswordHash("Password123", "", "alice", "pepper");
	EXPECT_TRUE(Result.m_Completed);
	EXPECT_FALSE(Result.m_Verified);
	EXPECT_FALSE(Result.m_NeedsRehash);
	EXPECT_TRUE(Result.m_NewHash.empty());
}

TEST_F(CTestAccounts, VerifyAsyncUnknownUsernameDoesDummyWork)
{
	const auto Start = std::chrono::steady_clock::now();
	std::string Password = "Password123";
	std::future<Accounts::SPasswordVerifyResult> Future = Accounts::VerifyPasswordAsync(Password, "", "alice", "pepper", nullptr);
	const Accounts::SPasswordVerifyResult Result = Future.get();
	const auto Elapsed = std::chrono::steady_clock::now() - Start;
	EXPECT_TRUE(Result.m_Completed);
	EXPECT_FALSE(Result.m_Verified);
	EXPECT_TRUE(Result.m_NewHash.empty());
	// The dummy verification must actually cost a real verification.
	EXPECT_GT(Elapsed, std::chrono::milliseconds(5));
}

TEST_F(CTestAccounts, CanceledHash)
{
	Accounts::CCancelFlag pCancel = Accounts::CreateCancelFlag();
	pCancel->store(true);

	std::string Password = "Password123";
	std::future<Accounts::SPasswordHashResult> Future = Accounts::HashPasswordAsync(Password, pCancel);
	EXPECT_FALSE(Future.get().m_Success);
}

TEST_F(CTestAccounts, ResetKeepsConnectionScopedState)
{
	CClientAccountRequests Requests;
	CAbandonedFutures Abandoned;
	str_copy(Requests.m_aIpStr, "127.0.0.1");
	Requests.m_RegistrationAttempts.Add(std::chrono::steady_clock::now());

	Requests.Reset(Abandoned);

	// Reset also runs after every completed account request, so connection-scoped
	// data must survive it.
	EXPECT_STREQ(Requests.m_aIpStr, "127.0.0.1");
	EXPECT_EQ(Requests.m_RegistrationAttempts.Size(), 1);
	EXPECT_EQ(Requests.m_State, EAccountState::None);
}

TEST_F(CTestAccounts, AbandonedFuturesDoNotBlock)
{
	Accounts::CCancelFlag pCancel = Accounts::CreateCancelFlag();

	CAbandonedFutures Abandoned;
	std::string Password = "Password123";
	Abandoned.Add(Accounts::HashPasswordAsync(Password, pCancel));
	pCancel->store(true);
	Abandoned.Prune();

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	Abandoned.Prune();
	SUCCEED();
}
