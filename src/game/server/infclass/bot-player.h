#ifndef INFCLASS_BOT_PLAYER_H
#define INFCLASS_BOT_PLAYER_H

#include "ic_player.h"

#include <base/tl/ic_array.h>
#include <base/tl/ic_fifo.h>

#include <cstdint>
#include <optional>

#include "bot_utils.h"

template<typename T>
T fromString(const char *pString);

class CGameWorld;
class CHiveMind;
class CIcEntity;

using ClientsArray = icArray<int, MAX_CLIENTS>;

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

	vec2 toVec2() const
	{
		return vec2(X * 32 + 16, Y * 32 + 16);
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

struct SCheckPoint
{
	STilePosition TilePos;
	int Tick = 0;
};

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
	CheckPOI,
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
	ThreatAware,
	CanFlee,
	Count,
	Invalid = Count,
};

enum EBotState : uint8_t
{
	Roaming,
	Hunting,
	Fleeing,
	Count,
	Invalid = Count,
};

using TweaksArray = icArray<EBotTweak, static_cast<int>(EBotTweak::Count)>;

const char *toString(EBotTweak Tweak);
const char *toString(EObjection Objection);

struct SBotDecision
{
	STilePosition Position;
	uint32_t Tick = 0;
	int8_t Direction = 0;
	int8_t Uses = 0;
	EObjection Objection = EObjection::Invalid;
	EDecision Decision = EDecision::Invalid;
};

class CBaseBotPlayer : public CIcPlayer
{
public:
	using CIcPlayer::CIcPlayer;

	virtual void UpdateControls() {}
	virtual void OnKilled() {}

	void SetBotConfigId(std::optional<int> ConfigId);
	std::optional<int> GetBotConfigId() const { return m_BotConfigId; }

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

	const char *GetTag() const override { return m_aTag; }
	void SetTag(const char *pTag) { str_copy(m_aTag, pTag); }

	virtual const char *DumpBot() { return ""; };
	virtual void UpdateName() {}

protected:
	TweaksArray m_aTweaks;
	std::optional<int> m_BotConfigId;
	int m_SpawnMinTick = -1;
	int m_MaxLives = 0;
	int m_DropLevel = 0;
	int m_Lives = 0;

	float m_RespawnInterval = 0;
	char m_aTag[16]{};
};

class CBotPlayer : public CBaseBotPlayer
{
	MACRO_ALLOC_POOL_ID()
public:
	using CRecentDecisions = icArray<SBotDecision, 64>;
	using CCheckPoints = icFifoArray<SCheckPoint, 32>;

	CBotPlayer(CIcGameController *pGameController, int UniqueClientId, int ClientID, int Team);
	~CBotPlayer() override;

	static void OnNewRound();

	void SetBotUtilsData(const CBotUtilsSharedData &UtilsData);
	void Snap(int SnappingClient) override;

	void TryRespawn() override;
	bool IsBot() const override { return true; }
	void Tick() override;
	void TickPaused();

	void OnCharacterSpawned(const SpawnContext &Context) override;
	void OnTuningChanged() override;

	void UpdateTarget();
	std::optional<int> GetHumanTarget(const ClientsArray &Targets) const;
	std::optional<int> GetInfectedTarget(const ClientsArray &Targets) const;
	std::optional<vec2> GetNewPOI() const;
	void UpdatePOITarget();
	void OnTargetLost();
	void UpdateControls() override;
	void OnKilled() override;
	void UpdateActiveWeapon();
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
	const CBotUtils &GetBotUtils() const { return m_BotUtils; };

public:
	using DIRECTION = int8_t;
	enum
	{
		DIRECTION_LEFT = -1,
		DIRECTION_NONE = 0,
		DIRECTION_RIGHT = 1,
	};

	bool IsDebugEnabled(int Verbosity) const;
	void BotDebugMessage(int VerbosityLevel, const char *fmt, ...) const
		GNUC_ATTRIBUTE((format(printf, 3, 4)));

	bool HasWallInTheDirection(DIRECTION Direction) const;
	bool HasDamageTiles(const vec2 &From, const vec2 &To, float Radius) const;
	bool HasDangerBelow() const;
	EThreatLevel GetDangerLevelAhead(vec2 *pThreatPosition = nullptr, CIcEntity **ppThreatEntity = nullptr) const;
	EThreatLevel GetDangerLevelOnLine(const vec2 &From, vec2 To, vec2 *pThreatPosition = nullptr, CIcEntity **ppThreatEntity = nullptr) const;
	int GetJumpsNeededToGetOverWall(DIRECTION Direction, int MaxJumps = -1, vec2 *pTargetPosition = nullptr) const;
	int GetJumpsNeededToJumpOnPlatform(DIRECTION Direction, int MaxJumps, vec2 *pTargetPosition = nullptr) const;
	int GetJumpsNeededToJumpOnPlatform(DIRECTION Direction, int MaxJumps, vec2 *pTargetPosition, float MaxHDistance) const;
	int GetAirTilesAbove(DIRECTION Direction, int MaxJumpsMaxJumps) const;
	float GetAirTilesAboveAtX(int MaxTiles, float CheckPosX) const;

	int GetMaxJumps() const;
	int GetAvailableJumps() const;
	float GetMaxTilesForJumps(int Jumps) const;
	int GetJumpsToReachTarget(float TileY) const;
	int GetJumpsToReachTarget(const vec2 &VectorToTarget) const;

	EBotState GetState() const { return m_BotState; }
	void SetState(EBotState NewState);

	DIRECTION GetRoamingDirection() const { return m_RoamingDirection; }
	void SetRoamingDirection(DIRECTION Direction);
	void SetObjection(EObjection Objection);

	bool IsBehaviorChangeQueued() const;
	void QueueBehaviorChange();
	void QueueBehaviorChange(float Delay);
	void MaybeChangeRoamingBehavior();
	void ChangeRoamingBehavior();
	void GetNewObjection();
	void MaybeHookTheTarget(float Distance);
	void ResetHooking();

	bool IsMovingInDirection(DIRECTION Direction, float MinVelocity) const;

	bool IsSolidTile(float X, float Y) const;
	bool IsSolidTile(const vec2 &Point) const;
	bool IsGrounded() const;

	bool MaybeFallDown() const;
	bool MaybeJumpOverWall(const vec2 &JumpTargetPosition) const;
	bool MaybeJumpOnPlatform(const vec2 &JumpTargetPosition, bool ForceIgnoreIfChecked = false);
	bool MaybeRandomJumpUp() const;
	int GetJumpsToAvoidDanger(vec2 *pTargetPosition = nullptr) const;

	void PushDecision(EDecision Decision, std::optional<DIRECTION> OptDirection = std::nullopt);
	EDecision GetPreviousDecision() const;
	EDecision GetGoodDecision(std::optional<DIRECTION> OptDirection = std::nullopt) const;

	const CRecentDecisions &GetRecentDecisions() const { return m_RecentDecisions; }
	const CCheckPoints &GetRecentCheckPoints() const { return ma_CheckPoints; }

	void PushCheckedPosition(const STilePosition &ShortPos);
	void PushIgnoredPosition(const vec2 &Pos);
	void PushIgnoredPosition(const STilePosition &ShortPos);

	STilePosition m_DecisionPos{};
protected:
	CGameWorld *GameWorld() const;
	void UpdateCharacterState();
	void UpdatePOIState();
	CHiveMind &GetHiveMind() const;

	void PickBestWeapon(float DistanceToTarget);
	DIRECTION DoLandingManeuves() const;

	bool CanHook() const;
	bool WeakHook() const;
	bool StrongHook() const;
	bool IsThreatAware() const;
	bool CanFlee() const;
	bool CanJumpOnce() const;
	float GetMaxHookDistance() const;
	EThreatLevel CareAboutThreatLevel() const;
	float GetLookupRadius() const;
	float GetLookupOffset() const;

	STilePosition JumpPosToShortPos(const vec2 &JumpTarget, const vec2 &JumpFromPosition) const;
	void SetJumpTargetPosition(const vec2 &JumpTarget, const vec2 &JumpFromPosition);
	void SetPOI(std::optional<vec2> newPOI);

	CBotUtils m_BotUtils;
	EBotState m_BotState = EBotState::Roaming;
	int m_BotStateTick = -1;
	DIRECTION m_RoamingDirection = DIRECTION_NONE;
	EObjection m_RoamingObjection = EObjection::Lookup;
	EObjection m_TargetLastSeenDirObjection = EObjection::Invalid;
	int m_RoamingBehaviorTick = 0;
	int m_RoamingBehaviorChangeTick = 0;
	icArray<EObjection, 5> m_RecentObjections;
	CRecentDecisions m_RecentDecisions;
	icArray<int, 4> ma_RecentFailedAttackTicks;
	CCheckPoints ma_CheckPoints;
	icArray<SCheckPoint, 4> ma_IgnorePoints;
	int m_AirJumps = 0;

	float m_JumpExtraProbability = 0;

	int m_LastTarget = -1;
	std::optional<int> m_IgnoreTarget{};
	int m_IgnoreTargetUntil{};
	vec2 m_LastTargetSeenAtPos;
	std::optional<vec2> m_POIPos;
	int m_LookForPoiDisabledUntilTick = -1;
	int m_FleeingSinceTick = -1;
	int m_TargetUnreachableTicks = 0;
	int m_LastSeenTick = -1;
	int m_TargetSinceTick{};
	int m_LastFireTick = -1;
	int m_LastWeaponSwitchTick = 0;
	int m_NextRandomFireTick = 0;
	int m_NextHuntingJumpTick = 0;
	int m_HookUntilTick = -1;
	std::optional<int> m_HookAimingRemainingTicks;
	int m_KeepingDistanceTick = -1;
	DIRECTION m_KeepingDistanceDirection = DIRECTION_NONE;

	bool m_FallingDown = false;
	int m_WantedJumps = 0;
	int m_LastJumpTick = 0;
	vec2 m_JumpFromPosition;
	vec2 m_JumpTargetPosition;
	int m_JumpTargetingTicks = 0;

	int m_StateUpdateTick = 0;
	bool m_CachedGrounded = false;
	bool m_CachedPOIReachableByGround = false;

	bool m_CachedSameGroundTarget = false;
	int m_CachedSameGroundTargetUntilTick = 0;

	char m_Name[16];
};

#endif // INFCLASS_BOT_PLAYER_H
