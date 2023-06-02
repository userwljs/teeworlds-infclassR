#include "bot_utils.h"

#include <engine/shared/protocol.h>
#include <game/server/infclass/bot-player.h>
#include <game/server/infclass/entities/ic_entity.h>

#include <cstdarg>
#include <cstdio>

#ifdef DEBUG_BOTS
#define bot_msg dbg_msg
#else

inline void bot_msg(const char *sys, const char *fmt, ...)
{
	return;
}
#endif

constexpr float GetDistanceForVelocityAccelerationTicks(float Velocity, float Acceleration, int Ticks, float MaxVelocity = 0)
{
	float S = 0;

	int SignForCompare = 1;
	if(MaxVelocity)
	{
		const bool SameSign = (MaxVelocity >= 0) == (Acceleration >= 0);
		if(!SameSign)
		{
			MaxVelocity = -MaxVelocity;
			// Now MaxVelocity has the same direction
		}
		if(MaxVelocity < 0)
		{
			SignForCompare = -1;
		}
	}

	for(int i = 0; i < Ticks; ++i)
	{
		S += Velocity;
		if(MaxVelocity)
		{
			const bool SameSign = (Velocity >= 0) == (Acceleration >= 0);
			if(SameSign)
			{
				if((Velocity + Acceleration) * SignForCompare > MaxVelocity * SignForCompare)
				{
					if(Velocity * SignForCompare < MaxVelocity * SignForCompare)
					{
						// Clamp the velocity only if it was not already greater than the given max
						Velocity = MaxVelocity;
					}
					MaxVelocity = 0;
					Acceleration = 0;
				}
			}
		}
		Velocity += Acceleration;
	}

	return S;
}

constexpr int GetTicksToReachDistance(float Velocity, float Acceleration, float Distance, int MaxTicks = 500, float MaxVelocity = 0)
{
	float S = 0;

	int SignForCompare = 1;
	if(MaxVelocity)
	{
		const bool SameSign = (MaxVelocity >= 0) == (Acceleration >= 0);
		if(!SameSign)
		{
			MaxVelocity = -MaxVelocity;
			// Now MaxVelocity has the same direction
		}
		if(MaxVelocity < 0)
		{
			SignForCompare = -1;
		}
	}

	for(int i = 1; i < MaxTicks; ++i)
	{
		S += Velocity;
		if(MaxVelocity)
		{
			const bool SameSign = (Velocity >= 0) == (Acceleration >= 0);
			if(SameSign)
			{
				if((Velocity + Acceleration) * SignForCompare > MaxVelocity * SignForCompare)
				{
					if(Velocity * SignForCompare < MaxVelocity * SignForCompare)
					{
						// Clamp the velocity only if it was not already greater than the given max
						Velocity = MaxVelocity;
					}
					MaxVelocity = 0;
					Acceleration = 0;
				}
			}
		}
		Velocity += Acceleration;

		if(Velocity < 0)
			continue;

		if(S >= Distance)
		{
			return i;
		}
	}

	return MaxTicks;
}

constexpr float c_GroundJumpImpulse = 13.2f; // Tuning -> GroundJumpImpulse
constexpr float c_GroundControlSpeed = 10.0f; // Tuning -> GroundControlSpeed
constexpr float c_GroundControlAccel = 100.0f / static_cast<int>(SERVER_TICK_SPEED); // Tuning -> GroundControlAccel

constexpr float c_AirJumpImpulse = 12.0f; // Tuning -> AirJumpImpulse
constexpr float c_AirControlSpeed = 250.0f / static_cast<int>(SERVER_TICK_SPEED); // Tuning -> AirControlSpeed
constexpr float c_AirControlAccel = 1.5f; // Tuning -> AirControlAccel

constexpr float c_Gravity = 0.5f; // Tuning -> Gravity
constexpr float c_JumpPessimism = -2.f;

constexpr float c_GroundJumpHeight = GetDistanceForVelocityAccelerationTicks(c_GroundJumpImpulse, -c_Gravity, c_GroundJumpImpulse / c_Gravity) + c_JumpPessimism;
constexpr float c_AirJumpHeight = GetDistanceForVelocityAccelerationTicks(c_AirJumpImpulse, -c_Gravity, c_AirJumpImpulse / c_Gravity) + c_JumpPessimism;

constexpr float c_GroundJumpTiles = c_GroundJumpHeight / TileSizeF;
constexpr float c_AirJumpTiles = c_AirJumpHeight / TileSizeF;

bool ICollision::IsSolid(int x, int y) const
{
	int index = GetTile(x, y);
	return index == TILE_SOLID || index == TILE_NOHOOK;
}

void IDebugSink::SendMessage(int VerbosityLevel, const char *fmt, ...)
{
	if(!IsVerbosityEnabled(VerbosityLevel))
		return;

	va_list args;
	char aBuf[1024];

	va_start(args, fmt);
#if defined(CONF_FAMILY_WINDOWS)
	_vsnprintf(aBuf, sizeof(aBuf), fmt, args);
#else
	vsnprintf(aBuf, sizeof(aBuf), fmt, args);
#endif
	va_end(args);

	SendFormattedMessage(VerbosityLevel, aBuf);
}

void CBotUtils::SetCollision(ICollision *pCollision)
{
	m_pCollision = pCollision;

	int Width = m_pCollision->GameLayerWidth();
	int Height = m_pCollision->GameLayerHeight();
	m_AirTilesAboveCache.Reset(Width, Height);
}

float CBotUtils::GetDistanceForVelocityAccelerationTicks(float Velocity, float Acceleration, int Ticks, float AccelerationMaxVelocity)
{
	return ::GetDistanceForVelocityAccelerationTicks(Velocity, Acceleration, Ticks, AccelerationMaxVelocity);
}

int CBotUtils::GetTicksToFallToHeight(float Velocity, float Acceleration, float Distance, int MaxTicks)
{
	return ::GetTicksToReachDistance(Velocity, Acceleration, Distance, MaxTicks);
}

int CBotUtils::GetTicksToMoveDistance(float Velocity, float Acceleration, float Distance, int MaxTicks, float AccelerationMaxVelocity)
{
	return ::GetTicksToReachDistance(Velocity, Acceleration, Distance, MaxTicks, AccelerationMaxVelocity);
}

int CBotUtils::GetJumpsToReachHeight(float Height, int MaxJumps, bool FromGround) const
{
	if(Height <= 0)
		return 0;

	if(MaxJumps == 0)
		return 0;

	int UsedJumps = 0;
	if(FromGround)
	{
		Height -= c_GroundJumpHeight;
		UsedJumps = 1;
	}

	do
	{
		if(Height < 0)
		{
			return UsedJumps;
		}
		Height -= c_AirJumpHeight;
		UsedJumps++;
	} while(UsedJumps <= MaxJumps);

	return 0;
}

float CBotUtils::GetMaxTilesForJumps(int Jumps, bool FromGround) const
{
	if(Jumps == 0)
		return 0;

	float MaxTiles = 0;
	if(FromGround)
	{
		MaxTiles = c_GroundJumpTiles;
		Jumps--;
	}

	MaxTiles += Jumps * c_AirJumpTiles;

	return MaxTiles;
}

float CBotUtils::GetAirJumpTiles() const
{
	return c_AirJumpTiles;
}

float CBotUtils::GetGroundJumpTiles() const
{
	return c_GroundJumpTiles;
}

float CBotUtils::GetFirstSolidAbovePosition(const vec2 &Position, int MaxTiles) const
{
	// The Position should be an air tile
	for(int i = 0; i <= MaxTiles; ++i)
	{
		const float CheckPosY = Position.y - i * TileSize;
		int TileIndex = m_pCollision->GetTile(Position.x, CheckPosY);
		m_pDebugSink->SendMessage(VERBOSE_TRACE2, "GetFirstSolidAbovePosition: Checking tile at %.2f x %.2f: %d", Position.x / TileSize, CheckPosY / TileSize, TileIndex);

		if((TileIndex == TILE_SOLID) || (TileIndex == TILE_NOHOOK))
		{
			return CheckPosY;
		}
	}

	m_pDebugSink->SendMessage(VERBOSE_TRACE1, "GetFirstSolidAbovePosition(): no solid, MaxTiles: %d", MaxTiles);
	return Position.y;
}

float CBotUtils::GetFirstAirAbovePosition(const vec2 &Position, int MaxTiles) const
{
	// The Position should be a solid tile
	for(int i = 0; i <= MaxTiles; ++i)
	{
		const float CheckPosY = Position.y - i * TileSize;
		int TileIndex = m_pCollision->GetTile(Position.x, CheckPosY);
		m_pDebugSink->SendMessage(VERBOSE_TRACE2, "GetFirstAirAbovePosition: Checking tile at %.2f x %.2f: %d", Position.x / TileSize, CheckPosY / TileSize, TileIndex);
		if(TileIndex == TILE_AIR)
		{
			return CheckPosY;
		}
	}

	m_pDebugSink->SendMessage(VERBOSE_TRACE1, "GetFirstAirAbovePosition(): no air, MaxTiles: %d", MaxTiles);
	return Position.y;
}

std::optional<int> CBotUtils::GetFirstAirTileAbovePosition(CTileRoundedPosition TilePosition, int MaxTiles) const
{
	dbg_assert(MaxTiles > 0, "Invalid limit for first air lookup");
	// The initial Position should be a solid tile
	const int TilesAbove = TilePosition.Y + 1;
	if(MaxTiles > TilesAbove)
	{
		MaxTiles = TilesAbove;
	}

	for(int i = 0; i < MaxTiles; ++i)
	{
		--TilePosition.Y;
		if(!m_pCollision->IsSolid(TilePosition))
		{
			return TilePosition.Y;
		}
	}

	return std::nullopt;
}

float CBotUtils::GetAirTilesAbove(const vec2 &Position, int MaxTiles) const
{
	CTileRoundedPosition RoundedTilePos(Position);
	float DeltaY = Position.y / TileSize - RoundedTilePos.Y;

	return GetAirTilesAbove(RoundedTilePos, MaxTiles) + DeltaY;
}

int CBotUtils::GetAirTilesAbove(const CTileRoundedPosition &TilePosition, int MaxTiles) const
{
	char CachedValue = m_AirTilesAboveCache.Get(TilePosition.X, TilePosition.Y);
	if(CachedValue >= 0)
		return CachedValue;

	int AirTilesAbove = 0;

	const float CheckPosX = TilePosition.X * TileSize;
	const float BasePosY = TilePosition.Y * TileSize;
	for(int i = 1; i <= MaxTiles; ++i)
	{
		float CheckPosY = BasePosY - i * TileSize;
		// TODO: MapIndex
		bool Solid = m_pCollision->IsSolid(CheckPosX, CheckPosY);
		if(Solid)
		{
			break;
		}
		AirTilesAbove = i;
	}

	{
		CachedValue = static_cast<char>(AirTilesAbove);
		m_AirTilesAboveCache.Set(TilePosition.X, TilePosition.Y, CachedValue);
	}

	return AirTilesAbove;
}

float CBotUtils::GetSolidBelow(const vec2 &Position, int MaxTiles) const
{
	int SolidTileY = GetSolidBelow(CTileRoundedPosition(Position), MaxTiles);
	return SolidTileY * TileSize;
}

int CBotUtils::GetSolidBelow(const CTileRoundedPosition &TilePosition, int MaxTiles) const
{
	const float CheckPosX = TilePosition.X * TileSize;
	const float BasePosY = TilePosition.Y * TileSize;

	for(int i = 1; i <= MaxTiles; ++i)
	{
		if(!m_pCollision->IsSolid(CheckPosX, BasePosY + TileSize * i))
		{
			continue;
		}

		return TilePosition.Y + i;
	}

	return TilePosition.Y + MaxTiles;
}

bool CBotUtils::IsReachableByGround(const vec2 &From, const vec2 &To, int MaxJumps) const
{
	CTileRoundedPosition FromTile(From);
	CTileRoundedPosition ToTile(To);

	// Ground the positions:
	FromTile.Y = GetSolidBelow(FromTile) - 1;
	ToTile.Y = GetSolidBelow(ToTile) - 1;

	const int DirectionSign = To.x > From.x ? +1 : -1;

	//	int TileY = TileFrom.Y;
	int MaxTiles = GetMaxTilesForJumps(MaxJumps, true);

	int SanityCheckSteps = 0;

	CTileRoundedPosition CurrentTile = FromTile;

	while(CurrentTile.X != ToTile.X)
	{
		CurrentTile.X += DirectionSign;

		bool POIisSolid = m_pCollision->IsSolid(CurrentTile);

		if(POIisSolid)
		{
			std::optional<int> FirstAirAbove = GetFirstAirTileAbovePosition(CurrentTile, MaxTiles + 1);
			if(!FirstAirAbove.has_value())
			{
				// Too high to jump
				return false;
			}
			CTileRoundedPosition PreviousPOI(CurrentTile.X - DirectionSign, FirstAirAbove.value());
			if(m_pCollision->IsSolid(PreviousPOI))
			{
				// Unreachable (for now)
				return false;
			}

			int LastAirBelow = GetSolidBelow(PreviousPOI) - 1;

			if(LastAirBelow != CurrentTile.Y)
			{
				// The wanted floor on the previous tile is unreachable (with the current algo)
				return false;
			}

			CurrentTile.Y = FirstAirAbove.value();
		}

		SanityCheckSteps++;

		if(SanityCheckSteps > 1000)
		{
			dbg_msg("BotUtils", "IsReachableByGround: Seems to stuck");
			return false;
		}
	}

	CurrentTile.Y = GetSolidBelow(CurrentTile) - 1;

	if(CurrentTile.Y != ToTile.Y)
	{
		bot_msg("bot", "Not reachable");
		return false;
	}

	bot_msg("bot", "Reachable");
	return true;
}
