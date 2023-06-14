#include "bot_config_parser.h"
#include "game/server/infclass/bot-player.h"

#include <base/tl/ic_enum.h>

static const char *gs_aTweakNames[] = {
	"no-hook",
	"weak-hook",
	"strong-hook",
	"threat-aware",
	"invalid",
};

const char *toString(EBotTweak Tweak)
{
	return toStringImpl(Tweak, gs_aTweakNames);
}

template EBotTweak fromString<EBotTweak>(const char *pString);

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

bool ParseTweaks(const char *pStr, TweaksArray *pTweaks)
{
	const char aPrefix[] = "tweaks=";
	if(!str_startswith(pStr, aPrefix))
		return false;

	pTweaks->Clear();
	pStr += strlen(aPrefix);

	while(pStr)
	{
		const char *pTweakStr = pStr;
		const char *pDelimiter = str_find(pStr, ",");
		int TweakLength = pDelimiter ? pDelimiter - pTweakStr : str_length(pTweakStr);
		char aBuf[64];
		str_copy(aBuf, pTweakStr);
		aBuf[TweakLength] = '\0';

		EBotTweak Tweak = fromString<EBotTweak>(aBuf);
		if(Tweak != EBotTweak::Invalid)
			pTweaks->Add(Tweak);

		pStr = pDelimiter ? pDelimiter + 1 : nullptr;
	}

	return true;
}
