#ifndef INFCLASS_BOT_PLAYER_H
#define INFCLASS_BOT_PLAYER_H

#include "ic_player.h"

#include <base/tl/ic_array.h>

#include <cstdint>

class CGameWorld;
class CIcEntity;

enum Verbosity
{
	VERBOSE_MAIN,
	VERBOSE_STEPS,
	VERBOSE_TRACE1,
	VERBOSE_TRACE2,
};

struct STilePosition
{
	STilePosition() = default;
	STilePosition(int16_t TileX, int16_t TileY) :
		X(TileX), Y(TileY)
	{
	}

	static STilePosition fromPosXY(float PosX, float PosY)
	{
		return STilePosition(PosX / 32, PosY / 32);
	}

	int16_t X = 0;
	int16_t Y = 0;
};

inline bool operator==(const STilePosition &lhs, const STilePosition &rhs)
{
	return lhs.X == rhs.X && lhs.Y == rhs.Y;
}

inline bool operator!=(const STilePosition &lhs, const STilePosition &rhs)
{
	return lhs.X != rhs.X || lhs.Y != rhs.Y;
}

enum class EDecision : uint8_t
{
	TurnLeft,
	TurnRight,
	Jump,
	NoJump,
	Count,
	Invalid = Count,
};

enum class EThreatLevel : uint8_t
{
	Zero, // It is safe there
	Suspicious,
	Dangerous,
	Deadly,
};

struct SBotDecision
{
	STilePosition Position;
	int8_t Direction = 0;
	EDecision Decision = EDecision::Invalid;
};

class CBaseBotPlayer : public CIcPlayer
{
public:
	using CIcPlayer::CIcPlayer;

	virtual void UpdateControls() {}

	void SetSpawnMinTick(int SpawnTick);
	int Lives() const { return m_Lives; }
	void SetLives(int Lives);
	int MaxLives() const { return m_MaxLives; }
	void SetMaxLives(int Lives);

	float GetRespawnInterval() const { return m_RespawnInterval; }
	void SetRespawnInterval(float Interval);

	virtual const char *DumpBot() { return ""; };
	virtual void UpdateName() {}

protected:
	int m_SpawnMinTick = -1;
	int m_MaxLives = 0;
	int m_Lives = 0;

	float m_RespawnInterval = 0;
};

class CBotPlayer : public CBaseBotPlayer
{
	MACRO_ALLOC_POOL_ID()
public:
	CBotPlayer(CIcGameController *pGameController, int UniqueClientId, int ClientID, int Team);
};

#endif // INFCLASS_BOT_PLAYER_H
