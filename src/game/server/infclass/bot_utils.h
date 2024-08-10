#pragma once

#include <base/vmath.h>
#include <vector>

#ifndef GNUC_ATTRIBUTE
#ifdef __GNUC__
#define GNUC_ATTRIBUTE(x) __attribute__(x)
#else
#define GNUC_ATTRIBUTE(x)
#endif
#endif

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

	vec2 toVec2() const
	{
		return vec2(X * 32, Y * 32);
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
};

class IDebugSink
{
public:
	virtual ~IDebugSink() = default;
	virtual bool IsVerbosityEnabled(int VerbosityLevel) const = 0;
	virtual void SendFormattedMessage(int Verbosity, const char *pMessage) const = 0;

	void SendMessage(int Verbosity, const char *fmt, ...)
		GNUC_ATTRIBUTE((format(printf, 3, 4)));
};

template<typename T, T DefaultValue>
class DataCache
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

class CBotUtils
{
public:
	void SetCollision(ICollision *pCollision);
	ICollision *GetCollision() const { return m_pCollision; }

	void SetDebugSing(IDebugSink *pDebugSink) { m_pDebugSink = pDebugSink; }
	IDebugSink *GetDebugSink() const { return m_pDebugSink; }

	static float GetDistanceForVelocityAccelerationTicks(float Velocity, float Acceleration, int Ticks, float AccelerationMaxVelocity = 0);
	static int GetTicksToFallToHeight(float Velocity, float Acceleration, float Distance, int MaxTicks = 500);
	static int GetTicksToMoveDistance(float Velocity, float Acceleration, float Distance, int MaxTicks = 500, float AccelerationMaxVelocity = 0);

	int GetJumpsToReachHeight(float Height, int MaxJumps, bool FromGround) const;
	float GetMaxTilesForJumps(int Jumps, bool FromGround) const;

	float GetAirJumpTiles() const;
	float GetGroundJumpTiles() const;

	float GetFirstSolidAbovePosition(const vec2 &Position, int MaxTiles) const;
	float GetFirstAirAbovePosition(const vec2 &Position, int MaxTiles) const;
	int GetFirstAirTileAbovePosition(CTileRoundedPosition TilePosition, int MaxTiles) const;

	float GetAirTilesAbove(const vec2 &Position, int MaxTiles) const;
	int GetAirTilesAbove(const CTileRoundedPosition &TilePosition, int MaxTiles) const;
	float GetSolidBelow(const vec2 &Position, int MaxTiles = 16) const;
	int GetSolidBelow(const CTileRoundedPosition &TilePosition, int MaxTiles = 16) const;
	bool IsReachableByGround(const vec2 &From, const vec2 &To, int MaxJumps) const;

protected:
	ICollision *m_pCollision = nullptr;
	IDebugSink *m_pDebugSink = nullptr;

	mutable DataCache<char, -1> m_AirTilesAboveCache;
};
