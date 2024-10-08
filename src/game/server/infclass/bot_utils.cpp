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

bool ICollision::IsSolid(int x, int y) const
{
	int index = GetTile(x, y);
	return index == TILE_SOLID || index == TILE_NOHOOK;
}

// The method is a copy of CCollision::IntersectLine()
int ICollision::IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision) const
{
	if(Pos0 == Pos1)
		return CheckPoint(Pos0.x, Pos0.y);

	vec2 Pos1Pos0 = Pos1 - Pos0;
	float Distance = length(Pos1Pos0);
	int End(Distance+1);
	vec2 Last = Pos0;

	for(int i = 0; i < End; i++)
	{
		float a = i/Distance;
		vec2 Pos = Pos0 + Pos1Pos0 * a;
		if(CheckPoint(Pos.x, Pos.y))
		{
			if(pOutCollision)
				*pOutCollision = Pos;
			if(pOutBeforeCollision)
				*pOutBeforeCollision = Last;
			return GetCollisionAt(Pos.x, Pos.y);
		}
		Last = Pos;
	}
	if(pOutCollision)
		*pOutCollision = Pos1;
	if(pOutBeforeCollision)
		*pOutBeforeCollision = Pos1;
	return 0;
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

void CBotUtils::UpdateTuning(const CTuningParams &Tuning)
{
	c_GroundJumpImpulse = Tuning.m_GroundJumpImpulse;
	c_GroundControlSpeed = Tuning.m_GroundControlSpeed;
	c_GroundControlAccel = Tuning.m_GroundControlAccel;

	c_AirJumpImpulse = Tuning.m_AirJumpImpulse;
	c_AirControlSpeed = Tuning.m_AirControlSpeed;
	c_AirControlAccel = Tuning.m_AirControlAccel;

	c_Gravity = Tuning.m_Gravity;
	c_GroundJumpHeight = GetDistanceForVelocityAccelerationTicks(c_GroundJumpImpulse, -c_Gravity, c_GroundJumpImpulse / c_Gravity);
	c_AirJumpHeight = GetDistanceForVelocityAccelerationTicks(c_AirJumpImpulse, -c_Gravity, c_AirJumpImpulse / c_Gravity);

	c_GroundJumpTiles = c_GroundJumpHeight / TileSizeF;
	c_AirJumpTiles = c_AirJumpHeight / TileSizeF;
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

vec2 CBotUtils::PredictMovement(const vec2 &From, const vec2 &Velocity, int Ticks, int Direction) const
{
	const float XAcceleration = c_AirControlAccel * Direction;
	float X = GetDistanceForVelocityAccelerationTicks(Velocity.x, XAcceleration, Ticks, c_AirControlSpeed * Direction);
	float Y = Velocity.y == 0 ? 0 : GetDistanceForVelocityAccelerationTicks(Velocity.y, c_Gravity, Ticks, 0);

	return From + vec2(X, Y);
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

int CBotUtils::GetAirJumpTicks() const
{
	return c_AirJumpImpulse / c_Gravity;
}

int CBotUtils::GetGroundJumpTicks() const
{
	return c_GroundJumpImpulse / c_Gravity;
}

int CBotUtils::GetJumpTicksInAir(int MaxJumps, bool FromGround, float JumpMaxHeight, float Velocity) const
{
	if ((MaxJumps == 0) && (Velocity == 0.0f))
		return 0;

	dbg_assert(MaxJumps >= 0, "MaxJumps can't be a negative number");
	dbg_assert(!FromGround || Velocity == 0.0f, "Ground jump can't have initial velocity");

	int MaxTicks = 0;
	if(FromGround && (MaxJumps > 0))
	{
		MaxTicks += GetGroundJumpTicks();
		MaxJumps -= 1;
	}
	else if(Velocity < 0)
	{
		int ExtraTicks = -Velocity / c_Gravity;
		MaxTicks += ExtraTicks;
	}

	MaxTicks += GetAirJumpTicks() * MaxJumps;

	if(JumpMaxHeight)
	{
		float PositiveVelocity = std::max<float>(-Velocity, FromGround ? c_GroundJumpImpulse : c_AirJumpImpulse);
		float S = 0;
		// There is a ceiling
		for(int i = 0; i < MaxTicks; ++i)
		{
			S += PositiveVelocity;
			PositiveVelocity -= c_Gravity;

			if(S > JumpMaxHeight)
			{
				return i;
			}
			if(PositiveVelocity < 0)
			{
				// Falling down, didn't hit the ceiling
				break;
			}
		}
	}

	return MaxTicks;
}

std::optional<float> CBotUtils::GetFirstSolidAbovePosition(const vec2 &Position, int MaxTiles) const
{
	// The Position should be an air tile
	for(int i = 0; i <= MaxTiles; ++i)
	{
		// TODO: Can use air cache here
		const float CheckPosY = Position.y - i * TileSize;
		int TileIndex = m_pCollision->GetTile(Position.x, CheckPosY);
		m_pDebugSink->SendMessage(VERBOSE_TRACE2, "GetFirstSolidAbovePosition: Checking tile at %.2f x %.2f: %d", Position.x / TileSize, CheckPosY / TileSize, TileIndex);

		if((TileIndex == TILE_SOLID) || (TileIndex == TILE_NOHOOK))
		{
			return CheckPosY;
		}
	}

	m_pDebugSink->SendMessage(VERBOSE_TRACE1, "GetFirstSolidAbovePosition(): no solid, MaxTiles: %d", MaxTiles);
	return std::nullopt;
}

std::optional<float> CBotUtils::GetFirstAirAbovePosition(const vec2 &Position, int MaxTiles) const
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
	return std::nullopt;
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

bool CBotUtils::GetRoughIntersect(CTileRoundedPosition From, CTileRoundedPosition To) const
{
	if(m_IntersectionData.has_value())
	{
		if(m_IntersectionData->Pos1 == From && m_IntersectionData->Pos2 == To)
			return m_IntersectionData->CollisionIndex;
	}
	vec2 Pos0 = From.toVec2() + Tile2DHalfSize;
	vec2 Pos1 = To.toVec2() + Tile2DHalfSize;
	int Index = GetCollision()->IntersectLine(Pos0, Pos1);
	m_IntersectionData = CIntersectionData{.Pos1 = From, .Pos2 = To, .CollisionIndex = Index};

	return Index;
}

std::optional<vec2> CBotUtils::GetHitPos(vec2 From, vec2 Direction, float Curvature, float Speed, float Time, float TimeStep) const
{
	float SimulatedTime = 0.0f;
	while(SimulatedTime < Time)
	{
		SimulatedTime += TimeStep;
		vec2 NewPos = CalcPos(From, Direction, Curvature, Speed, SimulatedTime);
		int Collide = m_pCollision->IntersectLine(From, NewPos, &NewPos);
		if(Collide)
		{
			return NewPos;
		}
	}

	return std::nullopt;
}

std::optional<vec2> CBotUtils::GetHitPosVisualized(vec2 From, vec2 Direction, float Curvature, float Speed, float Time, float TimeStep) const
{
	float SimulatedTime = 0.0f;
	while(SimulatedTime < Time)
	{
		SimulatedTime += TimeStep;
		vec2 NewPos = CalcPos(From, Direction, Curvature, Speed, SimulatedTime);
		m_pDebugSink->HighlightLineSegment(From, NewPos);
		int Collide = m_pCollision->IntersectLine(From, NewPos, &NewPos);
		if(Collide)
		{
			m_pDebugSink->HighlightPosition(NewPos);
			return NewPos;
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
	auto &m_AirTilesAboveCache = m_pCollisionCache->m_AirTilesAboveCache;
	char CachedValue = m_AirTilesAboveCache.Get(TilePosition.X, TilePosition.Y);
	if(CachedValue >= 0)
		return std::min<int>(MaxTiles, CachedValue);

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

std::optional<float> CBotUtils::GetSolidBelow(const vec2 &Position, int MaxTiles) const
{
	std::optional<int> SolidTileY = GetSolidTileBelow(CTileRoundedPosition(Position), MaxTiles);
	if (SolidTileY.has_value())
		return SolidTileY.value() * TileSize;

	return std::nullopt;
}

std::optional<int> CBotUtils::GetSolidTileBelow(const CTileRoundedPosition &TilePosition, int MaxTiles) const
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

	return std::nullopt;
}


bool CBotUtils::IsReachableByGround(const vec2 &From, const vec2 &To, int MaxJumps, int MaxSteps) const
{
	return IsReachableByGroundImpl<false>(From, To, MaxJumps, MaxSteps);
}

bool CBotUtils::IsReachableByGroundTraced(const vec2 &From, const vec2 &To, int MaxJumps, int MaxSteps) const
{
	return IsReachableByGroundImpl<true>(From, To, MaxJumps, MaxSteps);
}

template<bool Trace>
bool CBotUtils::IsReachableByGroundImpl(const vec2 &From, const vec2 &To, int MaxJumps, int MaxSteps) const
{
	CTileRoundedPosition FromTile(From);
	CTileRoundedPosition ToTile(To);

	// Ground the positions:
	std::optional<int> SolidBelowFrom = GetSolidTileBelow(FromTile);
	if (!SolidBelowFrom.has_value())
		return false;

	std::optional<int> SolidBelowTo = GetSolidTileBelow(ToTile);
	if (!SolidBelowTo.has_value())
		return false;

	FromTile.Y = SolidBelowFrom.value() - 1;
	ToTile.Y = SolidBelowTo.value() - 1;

	const int DirectionSign = To.x > From.x ? +1 : -1;

	//	int TileY = TileFrom.Y;
	int MaxTiles = GetMaxTilesForJumps(MaxJumps, true);
	int MaxFallTiles = 2;

	int Steps = 0;

	CTileRoundedPosition CurrentTile = FromTile;
	while(CurrentTile.X != ToTile.X)
	{
		if constexpr(Trace)
		{
			GetDebugSink()->HighlightPosition(CurrentTile.Center());
		}

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

			std::optional<int> SolidBelowPrevPOI = GetSolidTileBelow(PreviousPOI);
			if (!SolidBelowPrevPOI.has_value())
				return false;

			int LastAirBelow = SolidBelowPrevPOI.value() - 1;

			if(LastAirBelow != CurrentTile.Y)
			{
				// The wanted floor on the previous tile is unreachable (with the current algo)
				return false;
			}

			if constexpr(Trace)
			{
				for(int Y = CurrentTile.Y - 1; Y > FirstAirAbove.value(); --Y)
				{
					GetDebugSink()->HighlightPosition(CTileRoundedPosition(CurrentTile.X, Y).Center());
				}
			}
			CurrentTile.Y = FirstAirAbove.value();
		}

		Steps++;

		if(Steps > MaxSteps)
		{
			return false;
		}
	}

	std::optional<int> SolidBelowCurent = GetSolidTileBelow(CurrentTile);
	if (!SolidBelowCurent.has_value())
		return false;

	CurrentTile.Y = SolidBelowCurent.value() - 1;

	if(CurrentTile.Y != ToTile.Y)
	{
		bot_msg("bot", "Not reachable");
		return false;
	}

	bot_msg("bot", "Reachable");
	return true;
}
