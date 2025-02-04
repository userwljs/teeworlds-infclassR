#include "accounts.h"

#include <engine/server/crypt.h>
#include <engine/server/databases/connection.h>

bool CAccountsAuthWorker::RegisterAccount(IDbConnection *pSqlServer, const ISqlData *pGameData, Write w, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlRegisterDataRequest *>(pGameData);
	auto *pResult = dynamic_cast<CAccountCreationResult *>(pGameData->m_pResult.get());
	char aBuf[512];
	bool End{};

	// Check for registration flooding
	{
		str_format(aBuf, sizeof(aBuf),
			"SELECT UserId FROM %s_passwords "
			"WHERE RegisterIp = ? AND ((JULIANDAY(CURRENT_TIMESTAMP) - JULIANDAY(RegisterDate)) * 86400) < ? "
			"LIMIT 1",
			pSqlServer->GetPrefix());

		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindString(1, pData->m_aIpStr);
		pSqlServer->BindInt(2, RegistrationIntervalSeconds);

		if(pSqlServer->Step(&End, pError, ErrorSize))
		{
			return true;
		}
		if(!End)
		{
			pResult->m_Result = CAccountCreationResult::Result::FloodProtectedTriggered;
			return false;
		}
	}

	// Check whether the username is already registered
	{
		str_format(aBuf, sizeof(aBuf),
			"SELECT UserId FROM %s_passwords "
			"WHERE Username = ? "
			"LIMIT 1",
			pSqlServer->GetPrefix());

		if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
		{
			return true;
		}
		pSqlServer->BindString(1, pData->m_aUsername);
		if(pSqlServer->Step(&End, pError, ErrorSize))
		{
			return true;
		}
		if(!End)
		{
			pResult->m_Result = CAccountCreationResult::Result::UsernameAlreadyRegistered;
			return false;
		}
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

	pResult->m_Result = CAccountCreationResult::Result::Success;
	int NumInserted;
	return pSqlServer->ExecuteUpdate(&NumInserted, pError, ErrorSize);
}

bool CAccountsAuthWorker::LoadAccountAuthData(IDbConnection *pSqlServer, const ISqlData *pGameData, char *pError, int ErrorSize)
{
	const auto *pData = dynamic_cast<const CSqlLoginDataRequest *>(pGameData);
	auto *pResult = dynamic_cast<CAccountLoadAuthDataResult *>(pGameData->m_pResult.get());

	char aBuf[512];
	str_format(aBuf, sizeof(aBuf),
		"SELECT UserId, PasswordHash FROM %s_passwords WHERE Username=? LIMIT 1",
		pSqlServer->GetPrefix());
	if(pSqlServer->PrepareStatement(aBuf, pError, ErrorSize))
	{
		return true;
	}
	pSqlServer->BindString(1, pData->m_aUsername);

	bool End;
	if(pSqlServer->Step(&End, pError, ErrorSize))
	{
		return true;
	}
	if(!End)
	{
		int UserId = pSqlServer->GetInt(1);
		char aPasswordHash[PasswordHashHexMaxLength];
		pSqlServer->GetString(2, aPasswordHash, sizeof(aPasswordHash));

		if(str_comp(aPasswordHash, pData->m_aPasswordHash) == 0)
		{
			pResult->m_UserId = UserId;
			str_copy(pResult->m_aUsername, pData->m_aUsername);
		}
	}

	return false;
}

AccountCredentialsHelper::AccountCredentialsHelper(const char *pUsername, const char *pPassword, const char *pServerSalt)
{
	str_copy(aTrimmedName, str_utf8_skip_whitespaces(pUsername));
	str_utf8_trim_right(aTrimmedName);

	char aTrimmedPassword[PasswordHashHexMaxLength];
	str_copy(aTrimmedPassword, str_utf8_skip_whitespaces(pPassword));
	str_utf8_trim_right(aTrimmedPassword);

	if(!aTrimmedName[0] || !aTrimmedPassword[0])
	{
		// Invalid input.
		return;
	}

	char aSalt[128];
	str_format(aSalt, sizeof(aSalt), "%s_ic_%s", pUsername, pServerSalt);
	Crypt(pPassword, aSalt, 2, PasswordHashOutputSize, aPasswordHash);
}

bool AccountCredentialsHelper::HashGenerated() const
{
	return aPasswordHash[0];
}
