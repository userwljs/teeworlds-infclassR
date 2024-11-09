#pragma once

#include <cstdint>

class CMapInfo
{
public:
	std::uint8_t MinimumPlayers = 0;
	std::uint8_t MaximumPlayers = 0;
};

class CMapInfoEx : public CMapInfo
{
public:
	bool mEnabled{};
	int mTimestamp{};

	const char *Name() const { return aMapName; }
	void SetName(const char *pMapName);

	void SetTimestamp(int Timestamp);
	void AddSkippedAt(int Timestamp);
	void ResetData();

protected:
	char aMapName[128];
};
