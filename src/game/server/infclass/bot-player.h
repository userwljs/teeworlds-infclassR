#ifndef INFCLASS_BOT_PLAYER_H
#define INFCLASS_BOT_PLAYER_H

#include "ic_player.h"

#include <base/tl/ic_array.h>

#include <cstdint>

template<typename T>
T fromString(const char *pString);

class CGameWorld;
class CIcEntity;
class CBotUtils;

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

	static STilePosition fromPos(const vec2 &Pos)
	{
		return STilePosition(Pos.x / 32, Pos.y / 32);
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

enum class EObjection : uint8_t
{
	Lookup,
	Relax,
	Jump,
	CheckTheTop,
	CheckTheMid,
	CheckTheBottom,
	CheckTheLastSeen,
	// SECURE_POSITION,
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

enum class EBotTweak : uint8_t
{
	NoHook,
	WeakHook,
	StrongHook,
	Count,
	Invalid = Count,
};

using TweaksArray = icArray<EBotTweak, static_cast<int>(EBotTweak::Count)>;

const char *toString(EBotTweak Tweak);
const char *toString(EObjection Objection);

struct SBotDecision
{
	STilePosition Position;
	int8_t Direction = 0;
	EObjection Objection = EObjection::Invalid;
	EDecision Decision = EDecision::Invalid;
};

class CBaseBotPlayer : public CIcPlayer
{
public:
	using CIcPlayer::CIcPlayer;

	virtual void UpdateControls() {}

	void SetTweaks(const TweaksArray &aTweaks);
	void SetSpawnMinTick(int SpawnTick);
	int Lives() const { return m_Lives; }
	void SetLives(int Lives);
	int MaxLives() const { return m_MaxLives; }
	void SetMaxLives(int Lives);

	int GetDropLevel() const { return m_DropLevel; }
	void SetDropLevel(int Level);

	float GetRespawnInterval() const { return m_RespawnInterval; }
	void SetRespawnInterval(float Interval);

	virtual const char *DumpBot() { return ""; };
	virtual void UpdateName() {}

protected:
	TweaksArray m_aTweaks;
	int m_SpawnMinTick = -1;
	int m_MaxLives = 0;
	int m_DropLevel = 0;
	int m_Lives = 0;

	float m_RespawnInterval = 0;
};

class CBotPlayer : public CBaseBotPlayer
{
	MACRO_ALLOC_POOL_ID()
public:
	CBotPlayer(CIcGameController *pGameController, int UniqueClientId, int ClientID, int Team);
	~CBotPlayer() override;

	void SetBotUtils(CBotUtils *pUtils);
	void Snap(int SnappingClient) override;

	void TryRespawn() override;
	bool IsBot() const override { return true; }
	void Tick() override;
	void TickPaused();

	void OnCharacterSpawned(const SpawnContext &Context) override;

	void UpdateTarget();
	void UpdateControls() override;
	void UpdateControlsRoaming(CNetObj_PlayerInput *pInput);
	void UpdateControlsHunting(CNetObj_PlayerInput *pInput);
	void UpdateHumanBotControls();
	void ScheduleRandomFire();

	void FindPath();

	const char *DumpBot() override;

	static void SetAiEnabled(bool Enabled);
	static void SetObjectionEnabled(EObjection Objection, bool Enabled);
	static void ResetEnabledObjections();
	static const char *GetObjectionName(int Objection);
	static int GetObjectionByName(const char *Objection);

	void UpdateName() override;

	void OnCharacterHPChanged() override;

public:
	enum BOTSTATE
	{
		BOTSTATE_ROAMING,
		BOTSTATE_HUNTING,
	};

	enum DIRECTION : int8_t
	{
		DIRECTION_LEFT = -1,
		DIRECTION_NONE = 0,
		DIRECTION_RIGHT = 1,
	};

	bool IsDebugEnabled(int Verbosity) const;
	void BotDebugMessage(int VerbosityLevel, const char *fmt, ...) const
		GNUC_ATTRIBUTE((format(printf, 3, 4)));

	bool HasWallInTheDirection(DIRECTION Direction) const;
	bool HasDangerInTheDirection(DIRECTION Direction) const;
	bool HasDangerBelow() const;
	EThreatLevel GetDangerLevelAhead(vec2 *pThreatPosition = nullptr, CIcEntity **ppThreatEntity = nullptr) const;
	EThreatLevel GetDangerLevelOnLine(const vec2 &From, vec2 To, vec2 *pThreatPosition = nullptr, CIcEntity **ppThreatEntity = nullptr) const;
	int GetJumpsNeededToGetOverWall(DIRECTION Direction, int MaxJumps = -1, vec2 *pTargetPosition = nullptr) const;
	int GetJumpsNeededToJumpOn(DIRECTION Direction, int MaxJumps, vec2 *pTargetPosition = nullptr) const;
	int GetAirTilesAbove(DIRECTION Direction, int MaxJumpsMaxJumps) const;
	float GetAirTilesAboveAtX(int MaxTiles, float CheckPosX) const;

	int GetMaxJumps() const;
	int GetAvailableJumps() const;
	float GetMaxTilesForJumps(int Jumps) const;
	int GetJumpsToReachTarget(float TileY) const;
	int GetJumpsToReachTarget(const vec2 &VectorToTarget) const;

	void SetState(BOTSTATE NewState);
	void SetRoamingDirection(DIRECTION Direction);
	void SetObjection(EObjection Objection);
	void MaybeChangeRoamingBehavior();
	void ChangeRoamingBehavior();
	void GetNewObjection();
	void MaybeHookTheTarget(float Distance);

	bool IsMovingInDirection(DIRECTION Direction, float MinVelocity) const;

	bool IsSolidTile(float X, float Y) const;
	bool IsSolidTile(const vec2 &Point) const;
	bool IsGrounded() const;

	bool MaybeFallDown() const;
	bool MaybeJumpOverWall(const vec2 &JumpTargetPosition) const;
	bool MaybeJumpOn(const vec2 &JumpTargetPosition) const;
	bool MaybeRandomJumpUp() const;
	int GetJumpsToAvoidDanger(vec2 *pTargetPosition = nullptr) const;

	void PushDecision(EDecision Decision);
	EDecision GetPreviousDecision() const;

	void PushCheckedPosition(const vec2 &Pos);

protected:
	CGameWorld *GameWorld() const;
	void UpdateCharacterState();

	bool CanHook() const;
	bool WeakHook() const;
	bool StrongHook() const;
	float GetMaxHookDistance() const;

	CBotUtils *m_pUtils = nullptr;
	BOTSTATE m_BotState = BOTSTATE_ROAMING;
	DIRECTION m_RoamingDirection = DIRECTION_NONE;
	EObjection m_RoamingObjection = EObjection::Lookup;
	EObjection m_TargetLastSeenDirObjection = EObjection::Invalid;
	int m_RoamingBehaviorTick = 0;
	icArray<EObjection, 5> m_RecentObjections;
	icArray<SBotDecision, 64> m_RecentDecisions;
	icArray<STilePosition, 12> m_CheckedPositions;
	int m_AirJumps = 0;

	float m_JumpExtraProbability = 0;

	int m_LastTarget = -1;
	vec2 m_LastTargetSeenAtPos;
	int m_LastSeenTick = -1;
	int m_LastFireTick = -1;
	int m_NextRandomFireTick = -1;
	int m_HookUntilTick = -1;
	int m_DelayHookUntilTick = -1;

	int m_DecisionTileX = -1;
	int m_DecisionTileY = -1;
	bool m_FallingDown = false;
	int m_WantedJumps = 0;
	int m_LastJumpTick = 0;
	vec2 m_JumpFromPosition;
	vec2 m_JumpTargetPosition;
	int m_JumpTargetingTicks = 0;

	int m_StateUpdateTick = 0;
	bool m_CachedGrounded = false;

	char m_Name[16];
};

#endif // INFCLASS_BOT_PLAYER_H
