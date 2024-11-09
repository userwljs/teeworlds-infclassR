#include "map_info.h"

#include <base/system.h>

void CMapInfoEx::SetName(const char *pMapName)
{
	str_copy(aMapName, pMapName);
}

void CMapInfoEx::SetTimestamp(int Timestamp)
{
	mTimestamp = Timestamp;
}

void CMapInfoEx::AddSkippedAt(int Timestamp)
{
	if(Timestamp > mTimestamp)
	{
		mTimestamp = Timestamp;
	}
}

void CMapInfoEx::ResetData()
{
	mTimestamp = 0;
}
