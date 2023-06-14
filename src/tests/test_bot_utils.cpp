#include <gtest/gtest.h>

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

	CBotUtils Utils;
	Utils.SetDebugSing(&DebugSink);
	Utils.SetCollision(&Collision);

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
