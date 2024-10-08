#pragma once

#include <base/vmath.h>

#include <optional>
#include <vector>

#ifndef GNUC_ATTRIBUTE
#ifdef __GNUC__
#define GNUC_ATTRIBUTE(x) __attribute__(x)
#else
#define GNUC_ATTRIBUTE(x)
#endif
#endif

class CTuningParams;

class CTilePosition
{
public:
	CTilePosition() = default;
	CTilePosition(float TileX, float TileY) :
		X(TileX), Y(TileY)
	{
	}

	static CTilePosition FromPosition(const vec2 &Pos)
	{
		return FromPosition(Pos.x, Pos.y);
	}

	static CTilePosition FromPosition(float PosX, float PosY)
	{
		return CTilePosition(PosX / 32, PosY / 32);
	}

	operator vec2() const
	{
		return vec2(X * 32, Y * 32);
	}

	void Round()
	{
		X = static_cast<int>(X);
		Y = static_cast<int>(Y);
	}

	CTilePosition Rounded() const
	{
		CTilePosition Pos(*this);
		Pos.Round();
		return Pos;
	}

	float X = 0;
	float Y = 0;
};

class CTileRoundedPosition
{
public:
	CTileRoundedPosition() = default;
	CTileRoundedPosition(const vec2 &Position) :
		X(Position.x / 32), Y(Position.y / 32)
	{
	}

	CTileRoundedPosition(int TileX, int TileY) :
		X(TileX), Y(TileY)
	{
	}

	int X = 0;
	int Y = 0;

	CTileRoundedPosition operator-(const CTileRoundedPosition &other) const
	{
		return CTileRoundedPosition(X - other.X, Y - other.Y);
	}

	friend auto operator<=>(const CTileRoundedPosition &, const CTileRoundedPosition &) = default;

	vec2 toVec2() const
	{
		return vec2(X * 32, Y * 32);
	}

	vec2 Center() const
	{
		return vec2(X * 32 + 16, Y * 32 + 16);
	}

	operator vec2() const
	{
		return toVec2();
	}
};

class ICollision
{
public:
	virtual ~ICollision() = default;

	virtual int GetTile(int x, int y) const = 0;
	virtual int GameLayerWidth() const = 0;
	virtual int GameLayerHeight() const = 0;
	bool IsSolid(int x, int y) const;
	bool IsSolid(const vec2 &Position) const { return IsSolid(Position.x, Position.y); }
	bool CheckPoint(float x, float y) const { return IsSolid(round_to_int(x), round(y)); }
	int GetCollisionAt(float x, float y) const { return GetTile(round(x), round(y)); }
	int IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision = nullptr, vec2 *pOutBeforeCollision = nullptr) const;
};

class IDebugSink
{
public:
	virtual ~IDebugSink() = default;
	virtual bool IsVerbosityEnabled(int VerbosityLevel) const = 0;
	virtual void SendFormattedMessage(int Verbosity, const char *pMessage) const = 0;

	void SendMessage(int Verbosity, const char *fmt, ...)
		GNUC_ATTRIBUTE((format(printf, 3, 4)));
	virtual void HighlightPosition(const vec2 &Position) = 0;
	virtual void HighlightLineSegment(const vec2 &From, const vec2 &To) = 0;
};

template<typename T, T DefaultValue>
class CDataCache
{
public:
	void Reset(int Width, int Height)
	{
		m_Width = Width;
		m_vData.clear();
		m_vData.resize(Width * Height, DefaultValue);
	}

	T Get(int X, int Y) const
	{
		if(X < 0 || Y < 0)
			return DefaultValue;

		const std::size_t Index = X + Y * m_Width;
		return Index < m_vData.size() ? m_vData.at(Index) : DefaultValue;
	}

	bool Set(int X, int Y, T Value)
	{
		if(X < 0 || Y < 0)
			return false;

		const std::size_t Index = X + Y * m_Width;
		if(Index >= m_vData.size())
			return false;

		m_vData[Index] = Value;
		return true;
	}

protected:
	std::vector<T> m_vData;
	int m_Width = 0;
};

class CCollisionCache
{
public:
	CDataCache<char, -1> m_AirTilesAboveCache;
};

class CIntersectionData
{
public:
	CTileRoundedPosition Pos1;
	CTileRoundedPosition Pos2;
	int CollisionIndex;
};

class CBotUtilsSharedData
{
public:
	ICollision *m_pCollision{};
	IDebugSink *m_pDebugSink{};
	CCollisionCache *m_pCache{};
};

class CBotUtils
{
public:
	void SetCollision(ICollision *pCollision) { m_pCollision = pCollision; }
	ICollision *GetCollision() const { return m_pCollision; }

	void SetDebugSing(IDebugSink *pDebugSink) { m_pDebugSink = pDebugSink; }
	IDebugSink *GetDebugSink() const { return m_pDebugSink; }

	void SetCache(CCollisionCache *pCache) { m_pCollisionCache = pCache; }

	void UpdateTuning(const CTuningParams &Tuning);

	static float GetDistanceForVelocityAccelerationTicks(float Velocity, float Acceleration, int Ticks, float AccelerationMaxVelocity = 0);
	static int GetTicksToFallToHeight(float Velocity, float Acceleration, float Distance, int MaxTicks = 500);
	static int GetTicksToMoveDistance(float Velocity, float Acceleration, float Distance, int MaxTicks = 500, float AccelerationMaxVelocity = 0);
	vec2 PredictMovement(const vec2 &From, const vec2 &Velocity, int Ticks, int Direction) const;

	int GetJumpsToReachHeight(float Height, int MaxJumps, bool FromGround) const;
	float GetMaxTilesForJumps(int Jumps, bool FromGround) const;

	float GetAirJumpTiles() const;
	float GetGroundJumpTiles() const;
	int GetAirJumpTicks() const;
	int GetGroundJumpTicks() const;
	int GetJumpTicksInAir(int MaxJumps, bool FromGround, float JumpMaxHeight = 0, float Velocity = 0) const;

	std::optional<float> GetFirstSolidAbovePosition(const vec2 &Position, int MaxTiles) const;
	std::optional<float> GetFirstAirAbovePosition(const vec2 &Position, int MaxTiles) const;
	std::optional<int> GetFirstAirTileAbovePosition(CTileRoundedPosition TilePosition, int MaxTiles) const;
	bool GetRoughIntersect(CTileRoundedPosition From, CTileRoundedPosition To) const;
	std::optional<vec2> GetHitPos(vec2 From, vec2 Direction, float Curvature, float Speed, float Time, float TimeStep) const;
	std::optional<vec2> GetHitPosVisualized(vec2 From, vec2 Direction, float Curvature, float Speed, float Time, float TimeStep) const;

	float GetAirTilesAbove(const vec2 &Position, int MaxTiles) const;
	int GetAirTilesAbove(const CTileRoundedPosition &TilePosition, int MaxTiles) const;
	std::optional<float> GetSolidBelow(const vec2 &Position, int MaxTiles = 16) const;
	std::optional<int> GetSolidTileBelow(const CTileRoundedPosition &TilePosition, int MaxTiles = 16) const;
	bool IsReachableByGround(const vec2 &From, const vec2 &To, int MaxJumps, int MaxSteps = 1000) const;
	bool IsReachableByGroundTraced(const vec2 &From, const vec2 &To, int MaxJumps, int MaxSteps = 1000) const;

	inline static constexpr float CalcPosY(float Velocity, float Curvature, float Speed, float Time)
	{
		Time *= Speed;
		return Velocity * Time + Curvature / 10000 * (Time * Time);
	}

	inline static constexpr float CalcPosX(float Velocity, float Speed, float Time)
	{
		return Velocity * Time * Speed;
	}

	inline static constexpr float GetProjectileTimeToTop(float Curvature, float Speed, float VelocityY)
	{
		return VelocityY / Speed / Curvature * 10000 / 2;
	}

protected:
	template<bool Trace>
	bool IsReachableByGroundImpl(const vec2 &From, const vec2 &To, int MaxJumps, int MaxSteps) const;

	ICollision *m_pCollision{};
	IDebugSink *m_pDebugSink{};
	CCollisionCache *m_pCollisionCache{};
	mutable std::optional<CIntersectionData> m_IntersectionData;

	float c_GroundJumpImpulse{};
	float c_GroundControlSpeed{};
	float c_GroundControlAccel{};
	float c_AirJumpImpulse{};
	float c_AirControlSpeed{};
	float c_AirControlAccel{};
	float c_Gravity{};
	float c_JumpPessimism{};
	float c_GroundJumpHeight{};
	float c_AirJumpHeight{};
	float c_GroundJumpTiles{};
	float c_AirJumpTiles{};
};
