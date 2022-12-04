#include "bot_config_parser.h"

#include <base/system.h>

int ParseSpawnTime(const char *pStr, bool *pOk)
{
	bool ownOk;
	if(!pOk)
		pOk = &ownOk;

	*pOk = false;

	const char aPrefix[] = "spawn=";
	if(!str_startswith(pStr, aPrefix))
		return 0;

	pStr += strlen(aPrefix);

	if(str_comp_nocase(pStr, "asap") == 0)
	{
		*pOk = true;
		return 0;
	}

	if(str_endswith(pStr, "s") || str_endswith(pStr, "sec") || str_endswith(pStr, "secs"))
	{
		int ReadValue = str_toint(pStr);
		if((ReadValue > 0) && (ReadValue < 1000))
		{
			*pOk = true;
			return ReadValue;
		}
	}

	return 0;
}

int ParseLives(const char *pStr, bool *pOk)
{
	bool ownOk;
	if(!pOk)
		pOk = &ownOk;

	*pOk = false;

	const char aPrefix[] = "lives=";
	if(!str_startswith(pStr, aPrefix))
		return 0;

	pStr += strlen(aPrefix);

	int ReadValue = str_toint(pStr);
	if(ReadValue == -1)
	{
		*pOk = true;
		return 0;
	}
	if((ReadValue > 0) && (ReadValue < 1000))
	{
		*pOk = true;
		return ReadValue;
	}

	return 0;
}

int ParseHP(const char *pStr, bool *pOk)
{
	bool ownOk;
	if(!pOk)
		pOk = &ownOk;

	*pOk = false;

	const char aPrefix[] = "hp=";
	if(!str_startswith(pStr, aPrefix))
		return 0;

	pStr += strlen(aPrefix);

	int ReadValue = str_toint(pStr);
	if((ReadValue > 0) && (ReadValue < 10000))
	{
		*pOk = true;
		return ReadValue;
	}

	return 0;
}

int ParseDropLevel(const char *pStr, bool *pOk)
{
	bool ownOk;
	if(!pOk)
		pOk = &ownOk;

	*pOk = false;

	const char aPrefix[] = "drop_level=";
	if(!str_startswith(pStr, aPrefix))
		return 0;

	pStr += strlen(aPrefix);

	int ReadValue = str_toint(pStr);
	if((ReadValue > 0) && (ReadValue < 1000))
	{
		*pOk = true;
		return ReadValue;
	}

	return 0;
}

float ParseRespawn(const char *pStr, bool *pOk)
{
	bool ownOk;
	if(!pOk)
		pOk = &ownOk;

	*pOk = false;

	const char aPrefix[] = "respawn=";
	if(!str_startswith(pStr, aPrefix))
		return 0;

	pStr += strlen(aPrefix);

	if(str_endswith(pStr, "s") || str_endswith(pStr, "sec") || str_endswith(pStr, "secs"))
	{
		float ReadValue = str_tofloat(pStr);
		if((ReadValue > 0) && (ReadValue < 100))
		{
			*pOk = true;
			return ReadValue;
		}
	}
	return 0;
}
