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
	std::int16_t SpawnMinTick{};

	// SpawnPointId == 0 means any; indices starts with 1
	std::optional<std::uint16_t> SpawnPointId{};

	// Witch ClientID
	std::optional<std::uint8_t> SpawnWitchId{};

	// Call LUA to get scripted spawn position
	bool ScriptedSpawn{};

	int Lives{};
	int HP{};
	int DropLevel{};
	float RespawnInterval{};
	TweaksArray Tweaks;
	char Tag[16]{};
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
	bool HardMode{};

	void Reset()
	{
		SurvivalWaves.Clear();
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
