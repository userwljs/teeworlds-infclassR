#pragma once

#include <base/system.h>
#include <base/tl/ic_array.h>

#include <game/server/infclass/bot-player.h>

static constexpr int MaxWaves = 20;
static constexpr int MaxBotsPerWave = 128;

class SurvivalBotConfiguration
{
public:
	EPlayerClass Class{};
	int SpawnMinTick{};
	int Lives{};
	int HP{};
	int DropLevel{};
	float RespawnInterval{};
	TweaksArray Tweaks;
};

class SurvivalWaveConfiguration
{
public:
	SurvivalWaveConfiguration() = default;
	static constexpr int MaxCommandLength = 64;

	int GetTotalInfectedLives() const;

	void Reset()
	{
		aName[0] = '\0';
		aCommandOnWon[0] = '\0';
		aCommandOnLost[0] = '\0';
		BotConfigurations.Clear();
	}

	char aName[64]{};
	char aCommandOnWon[MaxCommandLength]{};
	char aCommandOnLost[MaxCommandLength]{};
	icArray<SurvivalBotConfiguration, MaxBotsPerWave> BotConfigurations;
};

class SurvivalGameConfiguration
{
public:
	icArray<SurvivalWaveConfiguration, MaxWaves> SurvivalWaves;
	int MaxPlayers{};
	bool HardMode{};

	void Reset()
	{
		SurvivalWaves.Clear();
		MaxPlayers = 0;
		HardMode = false;
	}

	SurvivalWaveConfiguration *AddWave(const char *pWaveName)
	{
		SurvivalWaves.Resize(SurvivalWaves.Size() + 1);
		SurvivalWaveConfiguration *pConfig = &SurvivalWaves.Last();
		pConfig->Reset();
		if(pWaveName && pWaveName[0])
		{
			str_copy(pConfig->aName, pWaveName);
		}
		return pConfig;
	}
};
