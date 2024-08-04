#include <gtest/gtest.h>

#include <engine/shared/fixed_point_number.h>

#include <game/gamecore.h>
#include <game/mapitems.h>

#include <game/server/infclass/bot_utils.h>

const int c_MapData1[] = {
	// clang-format off
	// y\x  0  1  2  3  4
	/* 0 */ 1, 1, 1, 1, 1,
	/* 1 */ 1, 0, 0, 0, 1,
	/* 2 */ 1, 0, 0, 1, 1,
	/* 3 */ 1, 0, 0, 1, 1,
	/* 4 */ 1, 1, 1, 1, 1,
	// clang-format on
};

const int c_MapData2[] = {
	// clang-format off
	// y\x  0  1  2  3  4  5
	/* 0 */ 1, 1, 1, 1, 1, 1,
	/* 1 */ 1, 0, 0, 0, 0, 1,
	/* 2 */ 1, 0, 0, 0, 0, 1,
	/* 3 */ 1, 0, 0, 1, 0, 1,
	/* 4 */ 1, 0, 0, 1, 0, 1,
	/* 5 */ 1, 1, 1, 1, 1, 1,
	// clang-format on
};

const int c_MapData3[] = {
	// clang-format off
	// y\x  0  1  2  3  4  5  6
	/* 0 */ 1, 1, 1, 1, 1, 1, 1,
	/* 1 */ 1, 0, 0, 0, 0, 0, 1,
	/* 2 */ 1, 0, 0, 0, 0, 0, 1,
	/* 3 */ 1, 0, 0, 1, 1, 0, 1,
	/* 4 */ 1, 0, 0, 1, 0, 0, 1,
	/* 5 */ 1, 1, 1, 1, 1, 1, 1,
	// clang-format on
};

class CMockCollision : public ICollision
{
public:
	void SetMapData(const int *pData, int Width, int Height)
	{
		m_pTiles = pData;
		m_Width = Width;
		m_Height = Height;
	}

	int GetTile(int x, int y) const override
	{
		int Nx = clamp(x / 32, 0, m_Width - 1);
		int Ny = clamp(y / 32, 0, m_Height - 1);
		int pos = Ny * m_Width + Nx;

		return m_pTiles[pos];
	}

	int GameLayerWidth() const override
	{
		return m_Width;
	}

	int GameLayerHeight() const override
	{
		return m_Height;
	}

protected:
	const int *m_pTiles = nullptr;
	int m_Width = 0;
	int m_Height = 0;
};

class CMockDebugSink : public IDebugSink
{
public:
	bool IsVerbosityEnabled(int VerbosityLevel) const override { return false; }
	void SendFormattedMessage(int VerbosityLevel, const char *pMessage) const override {}
	void HighlightPosition(const vec2 &Position) override {}
	void HighlightLineSegment(const vec2 &From, const vec2 &To) override {}
};

float operator"" _t(long double LengthInTiles)
{
	return LengthInTiles * 32;
}

TEST(BotUtils, GetTicksToMoveDistance)
{
	{
		float Velocity = 10;
		float Acceleration = 0;
		float Distance = 10;
		EXPECT_EQ(CBotUtils::GetTicksToMoveDistance(Velocity, Acceleration, Distance), 1);
	}

	{
		float Velocity = 10;
		float Acceleration = 0;
		float Distance = 20;
		EXPECT_EQ(CBotUtils::GetTicksToMoveDistance(Velocity, Acceleration, Distance), 2);
	}

	{
		float Velocity = 10;
		float Acceleration = 2;
		float Distance = 22;
		EXPECT_EQ(CBotUtils::GetTicksToMoveDistance(Velocity, Acceleration, Distance), 2);
	}

	{
		float Velocity = 10;
		float Acceleration = 2;
		float Distance = 23;
		EXPECT_EQ(CBotUtils::GetTicksToMoveDistance(Velocity, Acceleration, Distance), 3);
	}

	{
		float Velocity = -10;
		float Acceleration = 5;
		float Distance = 20;
		EXPECT_EQ(CBotUtils::GetTicksToMoveDistance(Velocity, Acceleration, Distance), 7);
	}

	{
		float Velocity = -10;
		float Acceleration = 5;
		float Distance = -10;
		EXPECT_EQ(CBotUtils::GetTicksToMoveDistance(Velocity, Acceleration, Distance), 4);
	}

	{
		float Velocity = -10;
		float Acceleration = 5;
		float Distance = 35;
		float MaxVelocity = 15;
		int MaxTicks = 100;
		EXPECT_EQ(CBotUtils::GetTicksToMoveDistance(Velocity, Acceleration, Distance, MaxTicks, MaxVelocity), 8);
	}

	{
		float Velocity = -10;
		float Acceleration = 5;
		float Distance = 35;
		float MaxVelocity = 20;
		int MaxTicks = 100;
		EXPECT_EQ(CBotUtils::GetTicksToMoveDistance(Velocity, Acceleration, Distance, MaxTicks, MaxVelocity), 7);
	}
}

TEST(BotUtils, Collisions)
{
	CMockCollision Collision;
	Collision.SetMapData(c_MapData1, 5, 5);
	static_assert(std::size(c_MapData1) == 5 * 5);
	EXPECT_EQ(Collision.IsSolid(CTilePosition(0, 0)), true);
	EXPECT_EQ(Collision.IsSolid(CTilePosition(1, 1)), false);
	EXPECT_EQ(Collision.IsSolid(CTilePosition(4, 4)), true);
}

TEST(BotUtils, Jumps)
{
	CMockDebugSink DebugSink;
	CBotUtils Utils;
	Utils.SetDebugSing(&DebugSink);
	Utils.UpdateTuning(CTuningParams());

	int MaxTiles = 10;

	enum
	{
		JumpFromAir,
		JumpFromGround,
	};

	// Max 1 jump
	EXPECT_EQ(Utils.GetJumpsToReachHeight(1.0_t, 1, JumpFromGround), 1);
	EXPECT_EQ(Utils.GetJumpsToReachHeight(2.0_t, 1, JumpFromGround), 1);
	EXPECT_EQ(Utils.GetJumpsToReachHeight(3.0_t, 1, JumpFromGround), 1);
	EXPECT_EQ(Utils.GetJumpsToReachHeight(4.0_t, 1, JumpFromGround), 1);
	EXPECT_EQ(Utils.GetJumpsToReachHeight(5.0_t, 1, JumpFromGround), 1);

	EXPECT_EQ(Utils.GetJumpsToReachHeight(6.0_t, 1, JumpFromGround), 0);

	// Max 2 jumps
	EXPECT_EQ(Utils.GetJumpsToReachHeight(1.0_t, 2, JumpFromGround), 1);
	EXPECT_EQ(Utils.GetJumpsToReachHeight(2.0_t, 2, JumpFromGround), 1);
	EXPECT_EQ(Utils.GetJumpsToReachHeight(3.0_t, 2, JumpFromGround), 1);
	EXPECT_EQ(Utils.GetJumpsToReachHeight(4.0_t, 2, JumpFromGround), 1);
	EXPECT_EQ(Utils.GetJumpsToReachHeight(5.0_t, 2, JumpFromGround), 1);

	EXPECT_EQ(Utils.GetJumpsToReachHeight(6.0_t, 2, JumpFromGround), 2);
	EXPECT_EQ(Utils.GetJumpsToReachHeight(7.0_t, 2, JumpFromGround), 2);
	EXPECT_EQ(Utils.GetJumpsToReachHeight(8.0_t, 2, JumpFromGround), 2);
}

TEST(BotUtils, AirTiles)
{
	CMockDebugSink DebugSink;
	CMockCollision Collision;
	Collision.SetMapData(c_MapData1, 5, 5);
	static_assert(std::size(c_MapData1) == 5 * 5);

	static CCollisionCache Cache;

	int Width = Collision.GameLayerWidth();
	int Height = Collision.GameLayerHeight();
	Cache.m_AirTilesAboveCache.Reset(Width, Height);

	CBotUtils Utils;
	Utils.SetDebugSing(&DebugSink);
	Utils.SetCollision(&Collision);
	Utils.SetCache(&Cache);

	int MaxTiles = 10;

	EXPECT_EQ(Utils.GetAirTilesAbove(CTilePosition(1, 1), MaxTiles), 0);
	EXPECT_EQ(Utils.GetAirTilesAbove(CTilePosition(1, 2), MaxTiles), 1);
	EXPECT_EQ(Utils.GetAirTilesAbove(CTilePosition(1, 3), MaxTiles), 2);

	EXPECT_EQ(Utils.GetAirTilesAbove(CTilePosition(1, 1), MaxTiles), 0);
	EXPECT_EQ(Utils.GetAirTilesAbove(CTilePosition(1, 1.5f), MaxTiles), 0.5f);
	EXPECT_EQ(Utils.GetAirTilesAbove(CTilePosition(1, 2), MaxTiles), 1.0f);
}

TEST(BotUtils, SolidTiles)
{
	CMockDebugSink DebugSink;
	CMockCollision Collision;
	Collision.SetMapData(c_MapData2, 6, 6);
	static_assert(std::size(c_MapData2) == 6 * 6);

	CBotUtils Utils;
	Utils.SetDebugSing(&DebugSink);
	Utils.SetCollision(&Collision);

	int MaxTiles = 10;

	EXPECT_EQ(Utils.GetSolidBelow(CTilePosition(1, 1), MaxTiles), 5.0_t);
	EXPECT_EQ(Utils.GetSolidBelow(CTilePosition(1, 2), MaxTiles), 5.0_t);
	EXPECT_EQ(Utils.GetSolidBelow(CTilePosition(1, 3), MaxTiles), 5.0_t);
	EXPECT_EQ(Utils.GetSolidBelow(CTilePosition(1, 4), MaxTiles), 5.0_t);

	EXPECT_EQ(Utils.GetSolidBelow(CTilePosition(2, 1), MaxTiles), 5.0_t);

	EXPECT_EQ(Utils.GetSolidBelow(CTilePosition(3, 1), MaxTiles), 3.0_t);
	EXPECT_EQ(Utils.GetSolidBelow(CTilePosition(3, 2), MaxTiles), 3.0_t);

	EXPECT_EQ(Utils.GetSolidBelow(CTilePosition(4, 1), MaxTiles), 5.0_t);
}

// TEST(BotUtils, JumpsToJumpOn)
//{
//	CMockDebugSink DebugSink;
//	CMockCollision Collision;
//	CBotUtils Utils;
//	Utils.SetDebugSing(&DebugSink);
//	Utils.SetCollision(&Collision);

//	Collision.SetMapData(c_MapData2, 6, 6);
//	static_assert(std::size(c_MapData2) == 6 * 6);

//	int MaxJumps = 1;
//	EXPECT_TRUE(Utils.GetJumpsNeededToJumpOn(CTilePosition(1, 4), CTilePosition(1, 4), MaxJumps));
//}

TEST(BotUtils, ReachableByGround1)
{
	CMockDebugSink DebugSink;
	CMockCollision Collision;
	Collision.SetMapData(c_MapData2, 6, 6);
	static_assert(std::size(c_MapData2) == 6 * 6);

	CBotUtils Utils;
	Utils.SetDebugSing(&DebugSink);
	Utils.SetCollision(&Collision);
	Utils.UpdateTuning(CTuningParams());

	// Sanity check for the map:
	EXPECT_EQ(Collision.IsSolid(CTilePosition(1, 4)), false);
	EXPECT_EQ(Collision.IsSolid(CTilePosition(1, 5)), true);
	EXPECT_EQ(Collision.IsSolid(CTilePosition(4, 4)), false);
	EXPECT_EQ(Collision.IsSolid(CTilePosition(4, 5)), true);

	int MaxJumps = 1;
	EXPECT_TRUE(Utils.IsReachableByGround(CTilePosition(1, 4), CTilePosition(1, 4), MaxJumps));

	EXPECT_TRUE(Utils.IsReachableByGround(CTilePosition(1, 4), CTilePosition(2, 4), MaxJumps));
	EXPECT_TRUE(Utils.IsReachableByGround(CTilePosition(1, 4), CTilePosition(3, 2), MaxJumps));
	EXPECT_TRUE(Utils.IsReachableByGround(CTilePosition(1, 4), CTilePosition(4, 4), MaxJumps));

	EXPECT_TRUE(Utils.IsReachableByGround(CTilePosition(2, 4), CTilePosition(1, 4), MaxJumps));
	EXPECT_TRUE(Utils.IsReachableByGround(CTilePosition(3, 2), CTilePosition(1, 4), MaxJumps));
	EXPECT_TRUE(Utils.IsReachableByGround(CTilePosition(4, 4), CTilePosition(1, 4), MaxJumps));
}

TEST(BotUtils, ReachableByGround2)
{
	CMockDebugSink DebugSink;
	CMockCollision Collision;
	Collision.SetMapData(c_MapData3, 7, 6);
	static_assert(std::size(c_MapData3) == 7 * 6);

	CBotUtils Utils;
	Utils.SetDebugSing(&DebugSink);
	Utils.SetCollision(&Collision);
	Utils.UpdateTuning(CTuningParams());

	// Sanity check for the map:
	EXPECT_EQ(Collision.IsSolid(CTilePosition(1, 4)), false);
	EXPECT_EQ(Collision.IsSolid(CTilePosition(1, 5)), true);
	EXPECT_EQ(Collision.IsSolid(CTilePosition(4, 4)), false);
	EXPECT_EQ(Collision.IsSolid(CTilePosition(4, 5)), true);

	int MaxJumps = 1;
	EXPECT_TRUE(Utils.IsReachableByGround(CTilePosition(1, 4), CTilePosition(1, 4), MaxJumps));

	EXPECT_TRUE(Utils.IsReachableByGround(CTilePosition(1, 4), CTilePosition(2, 4), MaxJumps));
	EXPECT_TRUE(Utils.IsReachableByGround(CTilePosition(1, 4), CTilePosition(3, 2), MaxJumps));
	// Currently unreachable (requires direction change)
	// EXPECT_TRUE(Utils.IsReachableByGround(CTilePosition(1, 4), CTilePosition(4, 4), MaxJumps));
	EXPECT_TRUE(Utils.IsReachableByGround(CTilePosition(1, 4), CTilePosition(5, 4), MaxJumps));

	EXPECT_FALSE(Utils.IsReachableByGround(CTilePosition(5, 4), CTilePosition(1, 4), MaxJumps));
}

TEST(BotUtils, ProjectilePrediction)
{
	CMockDebugSink DebugSink;
	CMockCollision Collision;
	Collision.SetMapData(c_MapData3, 7, 6);
	static_assert(std::size(c_MapData3) == 7 * 6);

	CTuningParams Tuning;

	CBotUtils Utils;
	Utils.SetDebugSing(&DebugSink);
	Utils.SetCollision(&Collision);
	Utils.UpdateTuning(Tuning);

	const vec2 ProjStartPos = CTilePosition(1.5, 4);
	EXPECT_EQ(Collision.IsSolid(ProjStartPos), false);

	const vec2 VectorToTarget(8 * 32, -8 * 32);
	constexpr float Curvature{7.0f};
	constexpr float Speed{1000.0f};

	float Time = 1.0f;
	vec2 Dir{normalize(VectorToTarget)};
	const std::optional<vec2> Hit = Utils.GetHitPos(ProjStartPos, Dir, Curvature, Speed, Time * 1.1f, 1.0f / SERVER_TICK_SPEED);
	ASSERT_TRUE(Hit.has_value());
	const int HitX = Hit.value().x;
	const int HitY = Hit.value().y;
	EXPECT_EQ(HitX, 165);
	EXPECT_EQ(HitY, 31);

	CFixedPointNumber FixedTime = CBotUtils::GetProjectileTimeToTop(Curvature, Speed, 1.0f);
	EXPECT_EQ(static_cast<float>(FixedTime), 0.714f);

	FixedTime = CBotUtils::GetProjectileTimeToTop(Curvature, Speed, 0.5f);
	EXPECT_EQ(static_cast<float>(FixedTime), 0.357f);
}
