#pragma once

#include <base/flags.h>

class CIcEntity;

enum class EFilterFlag
{
	None,
	Follower = 1 << 0,
	FreeSpec = 1 << 1,
	AnySpec = 1 << 2,
	Restricted = 1 << 3,
	SameTeam = 1 << 4,
	Demo = 1 << 5,
};

DECLARE_FLAGS(FilterFlags, EFilterFlag);

bool SnapFiltersPassed(const CIcEntity *pEntity, int SnappingClient, FilterFlags Flags);
