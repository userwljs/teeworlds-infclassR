#pragma once

#include <cstddef>

#include <engine/shared/protocol.h>

constexpr std::size_t TIMESTAMP_STR_LENGTH = 20; // 2019-04-02 19:38:36
constexpr std::size_t NUM_CHECKPOINTS = MAX_CHECKPOINTS;

class CPlayerRaceData
{
public:
	CPlayerRaceData()
	{
		Reset();
	}
	~CPlayerRaceData() = default;

	void Reset()
	{
		m_BestTime = 0;
		for(float &BestTimeCp : m_aBestTimeCp)
			BestTimeCp = 0;

		m_RecordStopTick = -1;
	}

	void Set(float Time, const float aTimeCp[NUM_CHECKPOINTS])
	{
		m_BestTime = Time;
		SetBestTimeCp(aTimeCp);
	}

	void SetBestTimeCp(const float aTimeCp[NUM_CHECKPOINTS])
	{
		for(std::size_t i = 0; i < NUM_CHECKPOINTS; i++)
			m_aBestTimeCp[i] = aTimeCp[i];
	}

	float m_BestTime;
	float m_aBestTimeCp[NUM_CHECKPOINTS];

	int m_RecordStopTick;
	float m_RecordFinishTime;
};
