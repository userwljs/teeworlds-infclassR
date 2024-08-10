#pragma once

#include <base/system.h>
#include <base/tl/ic_array.h>

static constexpr int MaxWaves = 20;

class SurvivalWaveConfiguration
{
public:
	SurvivalWaveConfiguration() = default;

	void Reset()
	{
		aName[0] = '\0';
	}

	char aName[64]{};
};

class SurvivalGameConfiguration
{
public:
	icArray<SurvivalWaveConfiguration, MaxWaves> SurvivalWaves;

	void Reset()
	{
		SurvivalWaves.Clear();
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
