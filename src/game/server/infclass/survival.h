#pragma once

#include <base/system.h>

#include <game/server/infclass/bot-player.h>

#include <vector>

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
		vBotConfigurations.clear();
	}

	char aName[64]{};
	char aCommandOnWon[MaxCommandLength]{};
	char aCommandOnLost[MaxCommandLength]{};
	std::vector<SurvivalBotConfiguration> vBotConfigurations;
};

class SurvivalGameConfiguration
{
public:
	std::vector<SurvivalWaveConfiguration> vSurvivalWaves;
	bool HardMode{};

	void Reset()
	{
		vSurvivalWaves.clear();
		HardMode = false;
	}

	SurvivalWaveConfiguration *AddWave(const char *pWaveName)
	{
		vSurvivalWaves.resize(vSurvivalWaves.size() + 1);
		SurvivalWaveConfiguration *pConfig = &vSurvivalWaves.back();
		pConfig->Reset();
		if(pWaveName && pWaveName[0])
		{
			str_copy(pConfig->aName, pWaveName);
		}
		return pConfig;
	}
};
