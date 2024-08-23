#include "bot-player.h"

#include <base/tl/ic_enum.h>
#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/server/infclass/bot_utils.h>
#include <game/server/infclass/classes/ic_playerclass.h>
#include <game/server/infclass/entities/biologist-mine.h>
#include <game/server/infclass/entities/control-point.h>
#include <game/server/infclass/entities/engineer-wall.h>
#include <game/server/infclass/entities/ic_character.h>
#include <game/server/infclass/entities/ic_entity.h>
#include <game/server/infclass/entities/looper-wall.h>
#include <game/server/infclass/entities/merc-bomb.h>
#include <game/server/infclass/entities/scatter-grenade.h>
#include <game/server/infclass/entities/scientist-mine.h>
#include <game/server/infclass/ic_gamecontroller.h>

static constexpr int InfinityLives = -1;

#define THROTTLE_BOTS

class CHiveMind
{
public:
	void Reset();
	void UpdateTick(CIcGameController *pGameController, int Tick);

	void ReportKilled(CBotPlayer *pPlayer);
	void ReportTargetFound(const CBotPlayer *pPlayer, const vec2 &TargetPos);

	bool TryAttack(int TargetId);
	bool TryHook(int HookerId, int TargetId);
	bool TryToComputeDecision(CBotPlayer *pPlayer);
	std::optional<vec2> PickPOI(const vec2 &FromPos) const;
	bool HasPOI() const;

	EDecision GetGoodDecision(const CBotPlayer *pPlayer, std::optional<CBotPlayer::DIRECTION> OptDirection = std::nullopt);

	void PushCheckedPosition(STilePosition ShortPos, int Tick);
	bool IsPositionChecked(STilePosition ShortPos) const;

	void ValidateDirection(CBotPlayer *pPlayer);

	const icArray<vec2, MAX_CLIENTS> &GetHumanPositions()
	{
		return m_aHumanPositions;
	}

protected:
	static constexpr int MaxAttacksInTimespan = 2;
	static constexpr int MaxHooksInTimespan = 10;
	static constexpr float Timespan = 0.75f; // in seconds

	void CleanupUncheckedPositions();

	struct HiveVictim
	{
		icArray<int, MaxAttacksInTimespan> aAttacks;
		icArray<int, MaxHooksInTimespan> aHooks;
	};

	HiveVictim *GetVictim(int ClientID);

	int m_Tick = 0;
	HiveVictim m_aVictims[MAX_CLIENTS];
	icArray<vec2, 10> m_aPOIs;
	int m_HumansTick = 0;
	icArray<vec2, MAX_CLIENTS> m_aHumanPositions;
	icArray<int, MAX_CLIENTS> m_aInfectedBots;
	icArray<int, MAX_CLIENTS> m_aBotsProcessingQueue;
	icFifoArray<STilePosition, 320> m_aCheckedPos;
	icFifoArray<STilePosition, 32> m_aUncheckedPos;
	icFifoArray<SBotDecision, 8> m_aGoodDecisions;
};

void CHiveMind::Reset()
{
	m_Tick = -1;
	m_HumansTick = -1;
	for(HiveVictim &Victim : m_aVictims)
	{
		Victim = {};
	}
	m_aPOIs.Clear();
	m_aCheckedPos.Clear();
	m_aGoodDecisions.Clear();
}

void CHiveMind::UpdateTick(CIcGameController *pGameController, int Tick)
{
	if(Tick == m_Tick)
		return;

	m_Tick = Tick;
	for(std::size_t VictimId = 0; VictimId < std::size(m_aVictims); ++VictimId)
	{
		HiveVictim &Victim = m_aVictims[VictimId];

		while(!Victim.aAttacks.IsEmpty())
		{
			int OldAttack = Victim.aAttacks.First();
			if(OldAttack + SERVER_TICK_SPEED * Timespan <= m_Tick)
			{
				Victim.aAttacks.RemoveAt(0);
			}
			else
			{
				break;
			}
		}

		// Copy aHooks
		const auto aHooks = Victim.aHooks;
		for (int HookerId : aHooks)
		{
			const CIcCharacter *pCharacter = pGameController->GetCharacter(HookerId);
			if(pCharacter && pCharacter->GetHookedPlayer() == VictimId)
			{
				// Still hooking
				continue;
			}

			Victim.aHooks.RemoveOne(HookerId);
		}
	}

	m_aPOIs.Clear();
	CGameWorld *pGameWorld = pGameController->GameWorld();
	// Find other players
	for(TEntityPtr<CControlPoint> p = pGameWorld->FindFirst<CControlPoint>(); p; ++p)
	{
		if(p->IsMarkedForDestroy())
			continue;
		if(p->IsTaken() && p->IsInfected())
			continue;

		m_aPOIs.Add(p->GetPos());
	}

	constexpr int MaxDecisionUses = 4;
	for(int i = m_aGoodDecisions.Size() - 1; i >= 0; --i)
	{
		if(m_aGoodDecisions.At(i).Uses > MaxDecisionUses)
		{
			m_aGoodDecisions.RemoveAt(i);
		}
	}

	m_aHumanPositions.Clear();
	m_aInfectedBots.Clear();
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		const CInfClassPlayer *pPlayer = pGameController->GetPlayer(i);
		if(!pPlayer || !pPlayer->GetCharacter() || !pPlayer->GetCharacter()->IsAlive())
			continue;

		if (pPlayer->IsHuman())
		{
			m_aHumanPositions.Add(pPlayer->GetCharacter()->GetPos());
		}
		else if (pPlayer->IsBot())
		{
			m_aInfectedBots.Add(i);
		}
	}
}

void CHiveMind::ReportKilled(CBotPlayer *pPlayer)
{
	// Does not work
//	const auto &aRecentCheckPoints = pPlayer->GetRecentCheckPoints();
//	for(int i = 0; i < aRecentCheckPoints.Size(); ++i)
//	{
//		STilePosition Pos = aRecentCheckPoints.At(i).TilePos;
//		if(m_aUncheckedPos.Contains(Pos))
//			continue;

//		if(m_aUncheckedPos.Capacity() == m_aUncheckedPos.Size())
//		{
//			CleanupUncheckedPositions();
//		}
//		m_aUncheckedPos.Add(Pos);
//	}
}

void CHiveMind::ReportTargetFound(const CBotPlayer *pPlayer, const vec2 &TargetPos)
{
	if(pPlayer->GetState() == EBotState::Roaming)
	{
		const auto &aRecentDecisions = pPlayer->GetRecentDecisions();
		int MaxCheckpoints = std::max<int>(aRecentDecisions.Size(), 10);
		for(int i = aRecentDecisions.Size() - MaxCheckpoints; i < aRecentDecisions.Size(); ++i)
		{
			m_aGoodDecisions.Add(aRecentDecisions.At(i));
		}
		m_aGoodDecisions.Last().Tick = m_Tick;
	}

	const auto &aRecentCheckPoints = pPlayer->GetRecentCheckPoints();
	for(int i = 0; i < aRecentCheckPoints.Size(); ++i)
	{
		STilePosition Pos = aRecentCheckPoints.At(i).TilePos;
		if(m_aUncheckedPos.Contains(Pos))
			continue;

		if(m_aUncheckedPos.Capacity() == m_aUncheckedPos.Size())
		{
			CleanupUncheckedPositions();
		}
		m_aUncheckedPos.Add(Pos);
	}
}

bool CHiveMind::TryAttack(int TargetId)
{
	HiveVictim *pVictim = GetVictim(TargetId);
	const auto &aAttacks = pVictim->aAttacks;
	if(aAttacks.Size() == aAttacks.Capacity())
		return false;

	pVictim->aAttacks.Add(m_Tick);
	return true;
}

bool CHiveMind::TryHook(int HookerId, int TargetId)
{
	HiveVictim *pVictim = GetVictim(TargetId);
	const auto &aHooks = pVictim->aHooks;

	if (aHooks.Contains(HookerId))
		return true;

	if(aHooks.Size() == aHooks.Capacity())
		return false;

	if(aHooks.Size() >= g_Config.m_InfMaxHiveHooks)
		return false;

	pVictim->aHooks.Add(HookerId);
	return true;
}

bool CHiveMind::TryToComputeDecision(CBotPlayer *pPlayer)
{
#ifdef THROTTLE_BOTS
	if((m_Tick % 3) == 0)
		return false;
#endif

	if(pPlayer->IsHuman())
		return true;

	const STilePosition Pos = STilePosition::fromPos(pPlayer->GetCharacter()->GetPos());
	if(pPlayer->m_DecisionPos == Pos)
		return false;

	// m_aBotsProcessingQueue.Add(pPlayer->GetCID());
	// m_aInfectedBots?

	pPlayer->m_DecisionPos = Pos;

	return true;
}

std::optional<vec2> CHiveMind::PickPOI(const vec2 &FromPos) const
{
	float BestDistance2 = 800 * 800;
	std::optional<vec2> BestPOI;
	for (const vec2 &POIPos : m_aPOIs)
	{
		vec2 VectorToPOI = POIPos - FromPos;
		const float Distance2 = length_squared(VectorToPOI);
		if (Distance2 > BestDistance2)
			continue;
		BestDistance2 = Distance2;
		BestPOI = POIPos;
	}

	return BestPOI;
}

bool CHiveMind::HasPOI() const
{
	return !m_aPOIs.IsEmpty();
}

EDecision CHiveMind::GetGoodDecision(const CBotPlayer *pPlayer, std::optional<CBotPlayer::DIRECTION> OptDirection)
{
	CBotPlayer::DIRECTION Direction = OptDirection.has_value() ? OptDirection.value() : pPlayer->GetRoamingDirection();
	const vec2 &Pos = pPlayer->GetCharacter()->GetPos();
	const STilePosition Position = STilePosition::fromPos(Pos);
	for(int i = 0; i < m_aGoodDecisions.Size(); ++i)
	{
		SBotDecision &BotDecision = m_aGoodDecisions[i];

		if(BotDecision.Position != Position)
			continue;

		if(BotDecision.Direction != Direction)
			continue;

		BotDecision.Uses++;
		return BotDecision.Decision;
	}

	return EDecision::Invalid;
}

void CHiveMind::PushCheckedPosition(STilePosition ShortPos, int Tick)
{
	m_aCheckedPos.Add(ShortPos);
}

bool CHiveMind::IsPositionChecked(STilePosition ShortPos) const
{
	return m_aCheckedPos.Contains(ShortPos) && !m_aUncheckedPos.Contains(ShortPos);
}

void CHiveMind::ValidateDirection(CBotPlayer *pPlayer)
{
#ifdef THROTTLE_BOTS
	if(m_Tick % 3)
		return;
#endif

	if(m_aHumanPositions.IsEmpty() && m_aPOIs.IsEmpty())
		return;

	if(!pPlayer->GetCharacter())
		return;

	if(pPlayer->GetState() != EBotState::Roaming)
		return;

	if(pPlayer->IsBehaviorChangeQueued())
		return;

	const vec2 Pos = pPlayer->GetCharacter()->GetPos();
	const int DirectionSign = pPlayer->GetRoamingDirection();
	const int Limit = TileSize * 30;

	int TargetsOnLeft = 0;
	int TargetsOnRight = 0;
	int TargetsInMid = 0;

	float LeftPos = Pos.x - Limit;
	float RightPos = Pos.x + Limit;

	for(const vec2 &HumanPos : m_aHumanPositions)
	{
		if(HumanPos.x > RightPos)
		{
			TargetsOnRight++;
		}
		else if(HumanPos.x < LeftPos)
		{
			TargetsOnLeft++;
		}
		else
		{
			TargetsInMid++;
		}
	}

	if(pPlayer->IsInfected())
	{
		for(const vec2 &PoiPos : m_aPOIs)
		{
			if(PoiPos.x > RightPos)
			{
				TargetsOnRight++;
			}
			else if(PoiPos.x < LeftPos)
			{
				TargetsOnLeft++;
			}
			else
			{
				TargetsInMid++;
			}
		}
	}

	bool OutOfBoundaries = false;
	if(DirectionSign == CBotPlayer::DIRECTION_LEFT)
	{
		OutOfBoundaries = !TargetsOnLeft && !TargetsInMid;
	}
	if(DirectionSign == CBotPlayer::DIRECTION_RIGHT)
	{
		OutOfBoundaries = !TargetsOnRight && !TargetsInMid;
	}

	if(OutOfBoundaries)
	{
		float RandomValue = random_float();
		if(RandomValue > 0.9)
		{
			RandomValue += 10;
		}
		pPlayer->QueueBehaviorChange(0.5f + RandomValue * 5.0f);
	}
}

void CHiveMind::CleanupUncheckedPositions()
{
	for(int i = m_aCheckedPos.Size() - 1; i >= 0; --i)
	{
		STilePosition Pos = m_aCheckedPos.At(i);
		if(m_aUncheckedPos.Contains(Pos))
		{
			m_aCheckedPos.RemoveAt(i);
		}
	}

	m_aUncheckedPos.Clear();
}

CHiveMind::HiveVictim *CHiveMind::GetVictim(int ClientID)
{
	return &m_aVictims[ClientID];
}

static CHiveMind s_HiveMind;

void CBaseBotPlayer::SetTweaks(const TweaksArray &aTweaks)
{
	m_aTweaks = aTweaks;
}

void CBaseBotPlayer::SetSpawnMinTick(int SpawnTick)
{
	m_SpawnMinTick = SpawnTick;
}

void CBaseBotPlayer::SetLives(int Lives)
{
	m_Lives = Lives;
	UpdateName();
}

void CBaseBotPlayer::SetMaxLives(int Lives)
{
	if(Lives == 0)
	{
		Lives = InfinityLives;
	}

	m_MaxLives = Lives;

	if(m_Lives != Lives)
	{
		SetLives(Lives);
	}
}

void CBaseBotPlayer::SetDropLevel(int Level)
{
	m_DropLevel = Level;
}

void CBaseBotPlayer::SetRespawnInterval(float Interval)
{
	m_RespawnInterval = Interval;
}

inline float distance2(const vec2 &from, const vec2 &to)
{
	const float x = to.x - from.x;
	const float y = to.y - from.y;
	return x * x + y * y;
}

static bool AiEnabled = 1;
static icArray<EObjection, static_cast<int>(EObjection::Count)> AiBannedObjections;

static int c_JumpsHardLimit = 10;

constexpr float c_AirControlSpeed = 250.0f / SERVER_TICK_SPEED; // Tuning -> AirControlSpeed
constexpr float c_AirControlAccel = 1.5f; // Tuning -> AirControlAccel

static constexpr icArray<EPlayerClass, 3> aHookyClasses = {
	EPlayerClass::Spider,
	EPlayerClass::Bat,
};

static const char *gs_aDecisionNames[] = {
	"TurnLeft",
	"TurnRight",
	"Jump",
	"NoJump",
	"Invalid",
};

const char *toString(EDecision Decision)
{
	return toStringImpl(Decision, gs_aDecisionNames);
}

static const char *gs_aObjectionNames[] = {
	"lookup",
	"relax",
	"jump",
	"check-top",
	"check-mid",
	"check-bottom",
	"check-last-seen",
	"check-poi",
	"invalid",
};

const char *toString(EObjection Objection)
{
	return toStringImpl(Objection, gs_aObjectionNames);
}

static const char *gs_aBotStateNames[] = {
	"roaming",
	"hunting",
	"fleeing",
	"invalid",
};

const char *toString(EBotState State)
{
	return toStringImpl(State, gs_aBotStateNames);
}

template EObjection fromString<EObjection>(const char *pString);

static CBotPlayer::DIRECTION OppositeDirection(CBotPlayer::DIRECTION Direction)
{
	return CBotPlayer::DIRECTION(Direction * -1);
}

MACRO_ALLOC_POOL_ID_IMPL(CBotPlayer, MAX_CLIENTS)

CBotPlayer::CBotPlayer(CIcGameController *pGameController, int UniqueClientId, int ClientID, int Team) :
	CBaseBotPlayer(pGameController, UniqueClientId, ClientID, Team)
{
	m_IsInGame = true;
	m_IsReady = true;
}

CBotPlayer::~CBotPlayer()
{
}

void CBotPlayer::OnNewRound()
{
	s_HiveMind.Reset();
}

void CBotPlayer::SetBotUtilsData(const CBotUtilsSharedData &UtilsData)
{
	m_BotUtils.SetCollision(UtilsData.m_pCollision);
	m_BotUtils.SetDebugSing(UtilsData.m_pDebugSink);
	m_BotUtils.SetCache(UtilsData.m_pCache);
}

void CBotPlayer::Snap(int SnappingClient)
{
	if(m_ClientId == SnappingClient)
		return;

	CIcPlayer::Snap(SnappingClient);
}

void CBotPlayer::TryRespawn()
{
	if(m_Lives == 0)
		return;

	if((m_SpawnMinTick >= 0) && (GameController()->GetInfectionTick() < m_SpawnMinTick))
	{
		// Too early to spawn
		return;
	}

	CIcPlayer::TryRespawn();
}

void CBotPlayer::Tick()
{
	if(GameController()->IsGameOver())
		return;

	s_HiveMind.UpdateTick(GameController(), Server()->Tick());

	UpdateCharacterState();

	if(m_Team == TEAM_SPECTATORS)
	{
		if((m_SpawnMinTick >= 0) && (GameController()->GetInfectionTick() < m_SpawnMinTick))
		{
			// Too early to spawn, do nothing
		}
		else
		{
			const bool DoChatMsg = false;
			GameController()->DoTeamChange(this, 0, DoChatMsg);
		}
	}

	CIcPlayer::Tick();

	if(GameServer()->m_World.m_Paused)
	{
		TickPaused();
	}
	else
	{
		const int T = Server()->Tick();
		const int TickSpeed = Server()->TickSpeed();
		float IgnoreForTime = 5.0f;

		for(int i = ma_IgnorePoints.Size() - 1; i >= 0; --i)
		{
			if(T >= ma_IgnorePoints.At(i).Tick + TickSpeed * IgnoreForTime)
			{
				ma_IgnorePoints.RemoveAt(i);
			}
		}

		const CIcCharacter *pCharacter = GetCharacter();
		bool Controllable = pCharacter && pCharacter->IsAlive() && !pCharacter->IsFrozen() && !pCharacter->IsSleeping();
		if(Controllable)
		{
			if(m_JumpTargetingTicks > 0)
			{
				m_JumpTargetingTicks--;
			}
			if (m_HookAimingRemainingTicks.has_value())
			{
				m_HookAimingRemainingTicks.value()--;
			}

			UpdateTarget();
			s_HiveMind.ValidateDirection(this);
		}
		else
		{
			m_JumpTargetingTicks = 0;
			m_HookAimingRemainingTicks.reset();
		}
	}

	m_Latency.m_Avg = 0;
	m_Latency.m_Max = 0;
	m_Latency.m_Min = 0;
	m_Latency.m_Accum = 0;
	m_Latency.m_AccumMin = 0;
	m_Latency.m_AccumMax = 0;
}

void CBotPlayer::TickPaused()
{
	m_RoamingBehaviorTick++;
	if(m_RoamingBehaviorChangeTick)
		m_RoamingBehaviorChangeTick++;
	m_FleeingSinceTick++;
	m_LastJumpTick++;
	m_LastSeenTick++;
	m_LastFireTick++;
	if(m_NextRandomFireTick)
		m_NextRandomFireTick++;
	m_HookUntilTick++;
	if (m_HookAimingRemainingTicks.has_value())
		m_HookAimingRemainingTicks.value()++;

	for(auto &IgnoredPosition : ma_IgnorePoints)
	{
		IgnoredPosition.Tick++;
	}

	for(int &FailedAttackTick : ma_RecentFailedAttackTicks)
	{
		FailedAttackTick++;
	}
}

void CBotPlayer::OnCharacterSpawned(const SpawnContext &Context)
{
	CIcPlayer::OnCharacterSpawned(Context);
	GetCharacter()->SetDropLevel(m_DropLevel);

	m_LastFireTick = -1;
	m_NextRandomFireTick = 0;
	m_RecentObjections.Clear();
	m_RecentDecisions.Clear();
	ma_RecentFailedAttackTicks.Clear();
	m_BotState = EBotState::Roaming;

	SetObjection(EObjection::Lookup);
	ChangeRoamingBehavior();
}

void CBotPlayer::UpdateTarget()
{
	const CIcCharacter *pCharacter = GetCharacter();
	if(!pCharacter)
	{
		return;
	}

	if(m_BotState == EBotState::Fleeing)
	{
		float FleeingDuration = 0.65f;
		if(Server()->Tick() >= m_FleeingSinceTick + Server()->TickSpeed() * FleeingDuration)
		{
			SetState(EBotState::Roaming);
		}
		return;
	}

	// Lookup for humans
	ClientsArray Targets;

	const vec2 &Pos = pCharacter->GetPos();
	if(!pCharacter->IsBlind())
	{
		const float LookupRadius = GetLookupRadius();
		const float LookupOffset = GetLookupOffset();
		const vec2 LookupFromPos = Pos + pCharacter->GetDirection() * LookupOffset;
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			const CIcCharacter *pChar = GameController()->GetCharacter(i);
			if(pChar && pChar->IsHuman() && pChar->IsAlive() && !pChar->IsInvisible())
			{
				Targets.Add(i);
			}
		}

		GameController()->SortCharactersByDistance(&Targets, LookupFromPos, LookupRadius);
	}

	int TargetId = -1;

	const int HookedPlayer = m_pCharacter->Core()->HookedPlayer();
	if(Targets.Contains(HookedPlayer))
	{
		TargetId = HookedPlayer;
	}
	else
	{
		if(IsHuman())
		{

			TargetId = UpdateHumanTarget(Targets);
		}
		else
		{
			TargetId = UpdateInfectedTarget(Targets);
		}
	}

	if(TargetId < 0)
	{
		if(m_BotState == EBotState::Roaming)
		{
			UpdatePOITarget();
		}
		else if (m_BotState == EBotState::Hunting)
		{
			OnTargetLost();
		}
		return;
	}

	m_LastTarget = TargetId;
	m_LastTargetSeenAtPos = GameController()->GetCharacter(TargetId)->GetPos();
	m_LastSeenTick = Server()->Tick();

	if(IsInfected())
	{
		s_HiveMind.ReportTargetFound(this, m_LastTargetSeenAtPos);
		ma_CheckPoints.Clear();
	}

	SetState(EBotState::Hunting);
}

int CBotPlayer::UpdateHumanTarget(const ClientsArray &Targets)
{
	const vec2 &Pos = m_pCharacter->GetPos();
	int AvailableMedicID = -1;
	int CanHealID = -1;
	int NearestHumanID = -1;
	int NeedTargets = 3;

	for(int CandidateId : Targets)
	{
		const CIcCharacter *pChar = GameController()->GetCharacter(CandidateId);
		vec2 Out;
		vec2 Before;

		if(GameServer()->Collision()->IntersectLine(Pos, pChar->GetPos(), &Out, &Before))
		{
			continue;
		}

		if(NearestHumanID < 0)
		{
			NearestHumanID = CandidateId;
			NeedTargets -=1;
		}

		if(AvailableMedicID < 0)
		{
			if(pChar->GetPlayerClass() == EPlayerClass::Medic)
			{
				AvailableMedicID = CandidateId;
				NeedTargets -=1;
			}
		}

		if(CanHealID < 0)
		{
			if((pChar->GetPlayerClass() != EPlayerClass::Hero) && pChar->GetArmor() < 10)
			{
				CanHealID = CandidateId;
				NeedTargets -=1;
			}
		}

		if (NeedTargets == 0)
		{
			break;
		}
	}

	if ((AvailableMedicID >= 0) && (GetCharacter()->GetArmor() < 10))
	{
		return AvailableMedicID;
	}
	else if (CanHealID >= 0)
	{
		return CanHealID;
	}
	else
	{
		return NearestHumanID;
	}
}

int CBotPlayer::UpdateInfectedTarget(const ClientsArray &Targets)
{
	const vec2 &Pos = m_pCharacter->GetPos();

	int BestTarget = -1;

	const std::set<int> &aAttachedPlayers = m_pCharacter->Core()->m_AttachedPlayers;
	const auto PlayerAttached = [&aAttachedPlayers](int ClientID) -> bool {
		for(int AttachedCID : aAttachedPlayers)
		{
			if(ClientID == AttachedCID)
			{
				return true;
			}
		}

		return false;
	};

	for(int CandidateId : Targets)
	{
		const CIcCharacter *pChar = GameController()->GetCharacter(CandidateId);
		if(BestTarget >= 0)
		{
			if(!PlayerAttached(CandidateId))
				continue;
		}

		if(GameServer()->Collision()->IntersectLine(Pos, pChar->GetPos()))
		{
			continue;
		}

		if(BestTarget < 0)
		{
			BestTarget = CandidateId;
			if(aAttachedPlayers.empty())
			{
				break;
			}
		}

		if(PlayerAttached(CandidateId))
		{
			BestTarget = CandidateId;
			break;
		}
	}

	return BestTarget;
}

std::optional<vec2> CBotPlayer::GetNewPOI() const
{
	if (IsHuman())
		return std::nullopt;

	if (!s_HiveMind.HasPOI())
		return std::nullopt;

	float TargetCooldown = 2.0f;
	if (Server()->Tick() < m_LastSeenTick + Server()->TickSpeed() * TargetCooldown)
	{
		return std::nullopt;
	}

	const vec2 &Pos = GetCharacter()->GetPos();
	std::optional<vec2> newPOI = s_HiveMind.PickPOI(Pos);
	if(!newPOI.has_value())
		return std::nullopt;

	bool HasObstruction = GameServer()->Collision()->IntersectLine(Pos, newPOI.value());
	if(HasObstruction)
		return std::nullopt;

	return newPOI;
}

void CBotPlayer::UpdatePOITarget()
{
	std::optional<vec2> newPOI = GetNewPOI();
	if(newPOI.has_value() && (m_RoamingObjection != EObjection::CheckPOI))
	{
		const int Tick = Server()->Tick();
		if(Tick > m_LookForPoiDisabledUntilTick)
		{
			const float MaxLookingForPOI = 5.0f;
			m_LookForPoiDisabledUntilTick = Tick + Server()->TickSpeed() * MaxLookingForPOI;
			SetObjection(EObjection::CheckPOI);
		}
	}

	if(m_RoamingObjection == EObjection::CheckPOI)
	{
		if(newPOI.has_value())
		{
			const vec2 &Pos = GetCharacter()->GetPos();
			SetRoamingDirection(newPOI.value().x > Pos.x ? DIRECTION_RIGHT : DIRECTION_LEFT);
		}
		else
		{
			GetNewObjection();
		}
	}

	SetPOI(newPOI);
}

void CBotPlayer::OnTargetLost()
{
	BotDebugMessage(VERBOSE_MAIN, "Target lost");
	const vec2 &Pos = m_pCharacter->GetPos();

	SetState(EBotState::Roaming);
	const CIcCharacter *pExTarget = GameController()->GetCharacter(m_LastTarget);
	EObjection SameDirObjection = EObjection::Invalid;
	{
		std::optional<int> SolidBelowTheTarget = m_BotUtils.GetSolidTileBelow(m_LastTargetSeenAtPos);
		std::optional<int> SolidBelowTheBot = m_BotUtils.GetSolidTileBelow(Pos);

		if(SolidBelowTheTarget == SolidBelowTheBot)
		{
			SameDirObjection = EObjection::CheckTheMid;
		}
		else if(m_LastTargetSeenAtPos.y > Pos.y)
		{
			SameDirObjection = EObjection::CheckTheBottom;
		}
		else
		{
			SameDirObjection = EObjection::CheckTheTop;
		}
	}

	if(pExTarget && pExTarget->IsAlive())
	{
		SetObjection(EObjection::CheckTheLastSeen);
		m_TargetLastSeenDirObjection = SameDirObjection;
		SetRoamingDirection(m_LastTargetSeenAtPos.x > Pos.x ? DIRECTION_RIGHT : DIRECTION_LEFT);
	}
	else
	{
		SetObjection(SameDirObjection);
	}
}

void CBotPlayer::UpdateControls()
{
	if(!m_pCharacter)
		return;

	const int Tick = Server()->Tick();

	CNetObj_PlayerInput NewInput;
	NewInput.m_Direction = 0;
	NewInput.m_TargetX = 0;
	NewInput.m_TargetY = 0;
	NewInput.m_Jump = 0;
	NewInput.m_Fire = 0;
	NewInput.m_Hook = 0;
	NewInput.m_PlayerFlags = 0;
	NewInput.m_WantedWeapon = 0;
	NewInput.m_NextWeapon = 0;
	NewInput.m_PrevWeapon = 0;

	if(IsHuman())
	{
		UpdateHumanBotControls();
	}

	switch(m_BotState)
	{
	case EBotState::Roaming:
	case EBotState::Fleeing:
		UpdateControlsRoaming(&NewInput);
		break;
	case EBotState::Hunting:
		UpdateControlsHunting(&NewInput);
		break;
	case EBotState::Invalid:
		break;
	}

	if(!AiEnabled)
	{
		NewInput.m_Direction = 0;
		// NewInput.m_TargetX = 0;
		// NewInput.m_TargetY = 0;
		NewInput.m_Jump = 0;
		NewInput.m_Fire = 0;
		NewInput.m_Hook = 0;
		NewInput.m_PlayerFlags = 0;
		NewInput.m_WantedWeapon = 0;
		NewInput.m_NextWeapon = 0;
		NewInput.m_PrevWeapon = 0;
	}

	ScheduleRandomFire();

	m_pCharacter->OnPredictedInput(&NewInput);
	OnDirectInput(&NewInput);
}

void CBotPlayer::OnKilled()
{
	if(m_Lives >= 1)
	{
		SetLives(m_Lives - 1);
	}

	if(!IsHuman())
	{
		s_HiveMind.ReportKilled(this);
	}
}

void CBotPlayer::UpdateControlsRoaming(CNetObj_PlayerInput *pInput)
{
	if(m_RoamingDirection == DIRECTION_NONE)
	{
		SetRoamingDirection(DIRECTION(random_int(-1, 1)));
		return;
	}

	MaybeChangeRoamingBehavior();

	const int Tick = Server()->Tick();
	const int TickSpeed = Server()->TickSpeed();
	const vec2 &Pos = GetCharacter()->GetPos();
	const float Radius = GetCharacter()->GetProximityRadius();

	const bool CanJump = GetCharacter()->CanJump();
	int MaxJumps = GetAvailableJumps();

	bool WantToJump = false;

	const bool HasWallInRoamingDirection = HasWallInTheDirection(m_RoamingDirection);
	bool HasDangerInRoamingHorizontalDirection = false;

	if(!HasWallInRoamingDirection)
	{
		HasDangerInRoamingHorizontalDirection = HasDangerInTheDirection(m_RoamingDirection);
	}

	int KeepMoving = 1;
	const int DirectionSign = m_RoamingDirection;
	const float VelX = m_pCharacter->Core()->m_Vel.x;

	const bool NoWantedJump = m_RoamingObjection == EObjection::CheckPOI && m_CachedPOIReachableByGround;
	if(IsGrounded())
	{
		if(HasWallInRoamingDirection)
		{
			BotDebugMessage(VERBOSE_STEPS, "Has wall");
			vec2 JumpTarget;
			const int WantJumps = GetJumpsNeededToGetOverWall(m_RoamingDirection, MaxJumps, &JumpTarget);
			if(WantJumps)
			{
				if(MaybeJumpOverWall(JumpTarget))
				{
					BotDebugMessage(VERBOSE_STEPS, "Decide to jump over the wall in %d jumps", WantJumps);
					WantToJump = true;
					SetJumpTargetPosition(JumpTarget, Pos);
					PushCheckedPosition(JumpPosToShortPos(m_JumpTargetPosition, Pos));
					m_WantedJumps = WantJumps;
				}
			}

			if(!WantToJump)
			{
				ChangeRoamingBehavior();
			}
		}
		else if(HasDangerInRoamingHorizontalDirection)
		{
			ChangeRoamingBehavior();
			BotDebugMessage(VERBOSE_STEPS, "Change the direction to avoid the danger");
		}
		else
		{
			BotDebugMessage(VERBOSE_TRACE1, "No wall");
			if(s_HiveMind.TryToComputeDecision(this))
			{
				vec2 JumpTarget;
				if(!NoWantedJump && g_Config.m_InfBotBackjump && !IsHuman())
				{
					constexpr float MaxBackjumpXDistance = TileSizeF * 5;
					DIRECTION BackwardDirection = OppositeDirection(m_RoamingDirection);
					const int WantBackJumps = GetJumpsNeededToJumpOnPlatform(BackwardDirection, MaxJumps, &JumpTarget, MaxBackjumpXDistance);

					if(WantBackJumps && !m_BotUtils.IsReachableByGround(Pos, JumpTarget, MaxJumps))
					{
						STilePosition DecisionPos = JumpPosToShortPos(JumpTarget, Pos);
						constexpr bool IgnoreIfChecked = true;
						if(MaybeJumpOnPlatform(JumpTarget, IgnoreIfChecked))
						{
							SetJumpTargetPosition(JumpTarget, Pos);
							PushCheckedPosition(DecisionPos);
							BotDebugMessage(VERBOSE_STEPS, "Decided to 'back jump on' to %.2fx%.2f", JumpTarget.x / TileSizeF, JumpTarget.y / TileSizeF);
							WantToJump = true;
							m_WantedJumps = WantBackJumps;
							SetRoamingDirection(BackwardDirection);
						}
						else
						{
							PushIgnoredPosition(DecisionPos);
							BotDebugMessage(VERBOSE_STEPS, "Ignore 'back jump on' to %.2fx%.2f", JumpTarget.x / TileSizeF, JumpTarget.y / TileSizeF);
						}

						PushDecision(WantToJump ? EDecision::Jump : EDecision::NoJump, BackwardDirection);
					}
				}

				const int WantJumps = NoWantedJump ? 0 : GetJumpsNeededToJumpOnPlatform(m_RoamingDirection, MaxJumps, &JumpTarget);
				if(!WantToJump && WantJumps)
				{
					STilePosition DecisionPos = JumpPosToShortPos(JumpTarget, Pos);
					if(MaybeJumpOnPlatform(JumpTarget))
					{
						SetJumpTargetPosition(JumpTarget, Pos);
						PushCheckedPosition(DecisionPos);
						BotDebugMessage(VERBOSE_STEPS, "Decided to 'jump on' to %.2fx%.2f", JumpTarget.x / TileSizeF, JumpTarget.y / TileSizeF);
						WantToJump = true;
						m_WantedJumps = WantJumps;
					}
					else
					{
						PushIgnoredPosition(DecisionPos);
						BotDebugMessage(VERBOSE_STEPS, "Ignore 'jump on' to %.2fx%.2f", JumpTarget.x / TileSizeF, JumpTarget.y / TileSizeF);
					}

					PushDecision(WantToJump ? EDecision::Jump : EDecision::NoJump);
				}

				if(!WantToJump && IsSolidTile(Pos.x, Pos.y + Radius + 5))
				{
					CTileRoundedPosition CheckTile = Pos;
					CheckTile.X += 1 * m_RoamingDirection;

					std::optional<int> SolidInTheNextTile = m_BotUtils.GetSolidTileBelow(CheckTile, 2);
					if(!SolidInTheNextTile.has_value())
					{
						BotDebugMessage(VERBOSE_TRACE1, "Going to fall");

						if(MaybeFallDown())
						{
							BotDebugMessage(VERBOSE_STEPS, "Decide to fall down");
							m_FallingDown = true;
						}
						else
						{
							BotDebugMessage(VERBOSE_STEPS, "Decide to jump over");
							WantToJump = true;
						}

						PushDecision(WantToJump ? EDecision::Jump : EDecision::NoJump);
						PushCheckedPosition(STilePosition::fromPos(Pos));
					}
				}

				if(!WantToJump)
				{
					if(m_RoamingObjection == EObjection::Jump)
					{
						int TilesAbove = GetAirTilesAbove(m_RoamingDirection, MaxJumps);
						if(TilesAbove > m_BotUtils.GetGroundJumpTiles() && random_prob(m_JumpExtraProbability))
						{
							BotDebugMessage(VERBOSE_STEPS, "Jump just because!");
							WantToJump = true;
							m_JumpFromPosition = Pos;
							m_WantedJumps = std::max(MaxJumps, 3);
							m_JumpTargetPosition.x = std::numeric_limits<float>::quiet_NaN();
						}
					}
				}
			}
		}

		if(!WantToJump)
		{
			vec2 JumpTarget;
			int JumpsToAvoidDanger = GetJumpsToAvoidDanger(&JumpTarget);
			if(JumpsToAvoidDanger)
			{
				constexpr float AvoidDangerProba = 1.0f;
				if(random_prob(AvoidDangerProba) && m_BotUtils.IsReachableByGround(Pos, JumpTarget, MaxJumps))
				{
					WantToJump = true;

					m_WantedJumps = JumpsToAvoidDanger;
					SetJumpTargetPosition(JumpTarget, Pos);
					DIRECTION NewDirection = JumpTarget.x > Pos.x ? DIRECTION_RIGHT : DIRECTION_LEFT;
					m_RoamingDirection = NewDirection;
				}
			}
		}
	}
	else
	{
		// 'Not grounded' branch
		if(HasDangerInRoamingHorizontalDirection)
		{
			if(VelX * DirectionSign > 0.1)
			{
				BotDebugMessage(VERBOSE_TRACE1, "Brake!");
				KeepMoving = -1;
			}
			else
			{
				KeepMoving = 0;
			}

			BotDebugMessage(VERBOSE_STEPS, "Hold on! The danger is there");
		}

		if(CanJump)
		{
			vec2 JumpTarget;
			if((m_WantedJumps <= 0) && (m_pCharacter->Core()->m_Vel.y > -3) && s_HiveMind.TryToComputeDecision(this))
			{
				BotDebugMessage(VERBOSE_TRACE1, "Considering jump from the air");
				int MaybeWantJumps = 0;
				if(HasWallInRoamingDirection)
				{
					if((MaybeWantJumps = NoWantedJump ? 0 : GetJumpsNeededToGetOverWall(m_RoamingDirection, MaxJumps, &JumpTarget)))
					{
						if(m_pCharacter->Core()->m_Vel.y < 0)
						{
							// We're moving up
							if(MaybeJumpOverWall(JumpTarget))
							{
								SetJumpTargetPosition(JumpTarget, Pos);
								PushCheckedPosition(JumpPosToShortPos(m_JumpTargetPosition, Pos));
								BotDebugMessage(VERBOSE_STEPS, "'jump over' from the air to %.2fx%.2f",
									m_JumpTargetPosition.x / TileSizeF, m_JumpTargetPosition.y / TileSizeF);
								m_WantedJumps = MaybeWantJumps;
							}
						}
						else
						{
							if(MaybeFallDown())
							{
								BotDebugMessage(VERBOSE_STEPS, "Decide to fall down");
							}
							else
							{
								BotDebugMessage(VERBOSE_STEPS, "Jump over from the air");
								m_WantedJumps = MaybeWantJumps;
							}
						}
					}
				}
				else if((MaybeWantJumps = NoWantedJump ? 0 : GetJumpsNeededToJumpOnPlatform(m_RoamingDirection, MaxJumps, &JumpTarget)))
				{
					STilePosition DecisionPos = JumpPosToShortPos(JumpTarget, Pos);
					if(MaybeJumpOnPlatform(JumpTarget))
					{
						SetJumpTargetPosition(JumpTarget, Pos);
						PushCheckedPosition(DecisionPos);
						BotDebugMessage(VERBOSE_STEPS, "Jump on from the air");
						m_WantedJumps = MaybeWantJumps;
					}
					else
					{
						PushIgnoredPosition(DecisionPos);
					}
				}
				else if(m_AirJumps == 0)
				{
					if(MaybeRandomJumpUp())
					{
						BotDebugMessage(VERBOSE_STEPS, "Random jump from the air");
						m_WantedJumps = 1; // GetAvailableJumps();
						JumpTarget = Pos + m_pCharacter->Core()->m_Vel * Server()->TickSpeed();
						SetJumpTargetPosition(JumpTarget, Pos);
					}
				}

				if(MaybeWantJumps)
				{
					PushDecision(m_WantedJumps > 0 ? EDecision::Jump : EDecision::NoJump);
				}
			}

			if(m_WantedJumps > 0)
			{
				WantToJump = true;
				++m_AirJumps;
			}

			if(!WantToJump)
			{
				vec2 JumpTarget;
				int JumpsToAvoidDanger = GetJumpsToAvoidDanger(&JumpTarget);
				if(JumpsToAvoidDanger)
				{
					constexpr float AvoidDangerProba = 1.0f;
					if(random_prob(AvoidDangerProba))
					{
						WantToJump = true;

						m_WantedJumps = JumpsToAvoidDanger;
						SetJumpTargetPosition(JumpTarget, Pos);
						DIRECTION NewDirection = JumpTarget.x > Pos.x ? DIRECTION_RIGHT : DIRECTION_LEFT;
						m_RoamingDirection = NewDirection;
					}
				}
			}
		}
	}

	const vec2 VectorToTarget = m_JumpTargetPosition - Pos;
	if(m_JumpTargetingTicks)
	{
		if(IsGrounded())
		{
			m_JumpTargetingTicks = 0;
		}
	}

	pInput->m_Direction = m_RoamingDirection * KeepMoving;
	if (KeepMoving != 1)
	{
		// We're stopping or braking and can't not do the landing maneuvers
	}
	else if(WantToJump || m_JumpTargetingTicks)
	{
		DIRECTION LandingDir = DoLandingManeuves();
		pInput->m_Direction = LandingDir;
	}

	if(WantToJump)
	{
		const float Gravity = m_NextTuningParams.m_Gravity;
		const float GroundJumpImpulse = m_NextTuningParams.m_GroundJumpImpulse;
		const float AirJumpImpulse = m_NextTuningParams.m_AirJumpImpulse;

		const float dYTiles = (Pos.y - m_JumpTargetPosition.y) / TileSizeF;
		bool MaximizeJump = true;
		if(!IsGrounded())
		{
			if(dYTiles < m_BotUtils.GetAirJumpTiles())
			{
				MaximizeJump = false;
				// Reduce the interval for the last jump that fits
			}
		}

		bool Jump = false;
		if(MaximizeJump)
		{
			Jump = m_pCharacter->Core()->m_Vel.y > -Gravity;
		}
		else
		{
			Jump = true;
		}

		if(Jump && Tick > m_LastJumpTick + TickSpeed * 0.15f)
		{
			pInput->m_Jump = 1;
			m_LastJumpTick = Tick;
			m_WantedJumps--;
		}

		if(m_WantedJumps == 0)
		{
			float Impulse = IsGrounded() ? GroundJumpImpulse : AirJumpImpulse;
			float Velocity = m_pCharacter->Core()->m_Vel.y;
			int TicksToFall = CBotUtils::GetTicksToFallToHeight(Velocity - Impulse, Gravity, m_JumpTargetPosition.y - Pos.y, SERVER_TICK_SPEED * 3);

			BotDebugMessage(VERBOSE_STEPS, "Ticks to fall: %d", TicksToFall);

			m_JumpTargetingTicks = TicksToFall;
		}
	}

	if(!std::isnan(m_JumpTargetPosition.x) && (WantToJump || (!CanJump && ((VelX > 0) == (VectorToTarget.x > 0))))) // todo: match direction
	{
		pInput->m_TargetX = VectorToTarget.x;
		pInput->m_TargetY = VectorToTarget.y;
	}
	else
	{
		pInput->m_TargetX = m_RoamingDirection * TileSize * 4;
	}

	if(GetClass() == EPlayerClass::Slug)
	{
		if(m_RoamingObjection != EObjection::Relax)
		{
			pInput->m_TargetY = TileSize * 2;
		}
	}

	if(m_NextRandomFireTick && Tick > m_NextRandomFireTick)
	{
		pInput->m_Fire = true;
		m_NextRandomFireTick = 0;
	}

	if(GetClass() == EPlayerClass::Ghost)
	{
		pInput->m_Fire = false;
	}

	if(GetClass() == EPlayerClass::Boomer)
	{
		pInput->m_Fire = false;
	}
}

void CBotPlayer::UpdateControlsHunting(CNetObj_PlayerInput *pInput)
{
	const int Tick = Server()->Tick();
	const int TickSpeed = Server()->TickSpeed();
	const vec2 &Pos = GetCharacter()->GetPos();
	int TileX = Pos.x / TileSize;

	const float Gravity = m_NextTuningParams.m_Gravity;
	const vec2 VectorToTarget = m_LastTargetSeenAtPos - Pos;
	const float AbsXToTarget = fabs(VectorToTarget.x);
	const bool FallingDown = m_pCharacter->Core()->m_Vel.y > -Gravity;
	const int AvailableJumps = GetAvailableJumps();

	const EInfclassWeapon WeaponType = GetCharacter()->GetInfWeaponId();

	bool WantToJump = false;
	bool WantGoDown = false;

	const float ProximityRadius = m_pCharacter->GetProximityRadius();
	if(Pos.y > m_LastTargetSeenAtPos.y + ProximityRadius * 2)
	{
		if(IsGrounded() || (FallingDown && AvailableJumps > 0))
		{
			vec2 VectorToTarget = m_LastTargetSeenAtPos - Pos;
			const float HorizontalDistance = std::fabs(VectorToTarget.x);
			if(!CanHook() || (HorizontalDistance < GetMaxHookDistance()))
			{
				int NeedJumps = GetJumpsToReachTarget(VectorToTarget);
				int Jumps = std::min<int>(AvailableJumps, NeedJumps);
				float JumpTiles = m_BotUtils.GetMaxTilesForJumps(Jumps, IsGrounded());
				JumpTiles = m_BotUtils.GetAirTilesAbove(Pos, JumpTiles);
				vec2 PosIfJumped = vec2(Pos.x, Pos.y - JumpTiles * TileSizeF);
				if(m_BotUtils.GetRoughIntersect(PosIfJumped, m_LastTargetSeenAtPos))
				{
					// No jump!
				}
				else
				{
					m_WantedJumps = Jumps;
					SetJumpTargetPosition(m_LastTargetSeenAtPos, Pos);
				}
			}
		}

		WantToJump = m_WantedJumps > 0;
	}
	else if(Pos.y < m_LastTargetSeenAtPos.y)
	{
		WantGoDown = true;
	}

	const float Distance = distance(Pos, m_LastTargetSeenAtPos);
	DIRECTION DirectionToTarget = DIRECTION_NONE;

	if(Pos.x + ProximityRadius < m_LastTargetSeenAtPos.x)
	{
		DirectionToTarget = DIRECTION_RIGHT;
	}
	else if(Pos.x > m_LastTargetSeenAtPos.x + ProximityRadius)
	{
		DirectionToTarget = DIRECTION_LEFT;
	}
	else if(!HasWallInTheDirection(m_RoamingDirection))
	{
		DirectionToTarget = m_RoamingDirection;
	}

	DIRECTION Direction = DirectionToTarget;

	float PreferredDistance = 0.f;
	float FleeDistance = 0.f;
	if(WeaponType == EInfclassWeapon::INFECTED_GRENADE)
	{
		if(ma_RecentFailedAttackTicks.Size() < 3)
		{
			PreferredDistance = TileSize * 20;
			FleeDistance = TileSize * 16;
		}
	}
	if(CanFlee())
	{
		if(GetClass() == EPlayerClass::Witch)
		{
			PreferredDistance = TileSize * 14;
			FleeDistance = TileSize * 7;
		}
	}

	bool KeepingDistance = false;
	if(Distance < FleeDistance)
	{
		Direction = OppositeDirection(DirectionToTarget);
		KeepingDistance = true;
	}
	else if(Distance < PreferredDistance)
	{
		if(m_KeepingDistanceTick < Tick + TickSpeed * 1.0f)
		{
			if(random_prob(0.4f))
			{
				m_KeepingDistanceDirection = OppositeDirection(Direction);
			}
			else
			{
				m_KeepingDistanceDirection = DIRECTION_NONE;
			}
			m_KeepingDistanceTick = Tick;
		}

		Direction = m_KeepingDistanceDirection;
		KeepingDistance = true;
	}

	if((AbsXToTarget < TileSize * 3) && WantGoDown)
	{
		int YDiff = (m_LastTargetSeenAtPos.y - Pos.y) / TileSizeF;
		float AirTilesAboveTarget = m_BotUtils.GetAirTilesAbove(m_LastTargetSeenAtPos, YDiff + 1);
		if(YDiff > AirTilesAboveTarget)
		{
			// float GroundLevelBelow = m_BotUtils.GetSolidBelow(Pos, YDiff);
			Direction = OppositeDirection(Direction);
		}
	}

	if(Direction != DIRECTION_NONE)
	{
		bool HasWall = HasWallInTheDirection(Direction);

		BotDebugMessage(VERBOSE_TRACE1, HasWall ? "HasWall" : "HasNoWall");

		if((WantToJump || KeepingDistance) && HasWall)
		{
			int NeededJumps = GetJumpsNeededToGetOverWall(Direction);
			if(NeededJumps)
			{
				WantToJump = true;
				m_WantedJumps = NeededJumps;
			}
			else
			{
				WantToJump = false;
				// TODO: Find another way
			}
		}
	}

	const float HookMaxDistance = GetMaxHookDistance();
	const float GroundControlSpeed = GameServer()->Tuning()->m_GroundControlSpeed;

	if(!WantToJump && Distance > HookMaxDistance && (Distance > TileSize * 5))
	{
		vec2 JumpTarget;
		int JumpsToAvoidDanger = GetJumpsToAvoidDanger(&JumpTarget);
		if(JumpsToAvoidDanger)
		{
			constexpr float AvoidDangerProba = 1.0f;
			if(random_prob(AvoidDangerProba) && m_BotUtils.IsReachableByGround(Pos, JumpTarget, AvailableJumps))
			{
				WantToJump = true;

				m_WantedJumps = JumpsToAvoidDanger;
				SetJumpTargetPosition(JumpTarget, Pos);
				DIRECTION NewDirection = JumpTarget.x > Pos.x ? DIRECTION_RIGHT : DIRECTION_LEFT;
				m_RoamingDirection = NewDirection;
			}
		}
	}

	bool ConsiderHookingOut = false;

	if(CanHook() && !WeakHook() && (Distance < HookMaxDistance - TileSize * 1))
	{
		vec2 ToTarget = m_LastTargetSeenAtPos - Pos;
		const float Len2 = ToTarget.x * ToTarget.x + ToTarget.y * ToTarget.y;
		static const int MaxLookupDistance = TileSize * 4;
		static const int MaxLookup_2 = MaxLookupDistance * MaxLookupDistance;
		if(Len2 > MaxLookup_2)
		{
			ToTarget = normalize(ToTarget) * MaxLookupDistance;
		}

		const EThreatLevel LevelOfDanger = GetDangerLevelOnLine(Pos, Pos + ToTarget);

		bool StopAndHook = false;
		switch(LevelOfDanger)
		{
		case EThreatLevel::Zero:
			break;
		case EThreatLevel::Suspicious:
		case EThreatLevel::Dangerous:
		case EThreatLevel::Deadly:
			StopAndHook = true;
			break;
		}

		if(StopAndHook)
		{
			Direction = DIRECTION_NONE;
			ConsiderHookingOut = true;
		}
	}

	if(m_pCharacter->Core()->HookedPlayer() == m_LastTarget)
	{
		if(Distance > TileSize * 5 || IsMovingInDirection(OppositeDirection(DirectionToTarget), GroundControlSpeed * 0.3f))
		{
			ConsiderHookingOut = true;
		}

		if(ConsiderHookingOut)
		{
			Direction = OppositeDirection(DirectionToTarget);

			if(!WantGoDown)
			{
				// Go move away with a jump
				WantToJump = true;
			}
		}
	}

	if(WantToJump)
	{
		const float dYTiles = (Pos.y - m_JumpTargetPosition.y) / TileSizeF;
		bool MaximizeJump = true;
		if(!IsGrounded())
		{
			if(dYTiles < m_BotUtils.GetAirJumpTiles())
			{
				MaximizeJump = false;
				// Reduce the interval for the last jump that fits
			}
		}

		bool Jump = false;
		if(MaximizeJump)
		{
			Jump = m_pCharacter->Core()->m_Vel.y > -Gravity;
		}
		else
		{
			Jump = true;
		}

		if(Jump && Tick > m_LastJumpTick + TickSpeed * 0.15f)
		{
			pInput->m_Jump = 1;
			m_LastJumpTick = Tick;
			m_WantedJumps--;
		}
	}

	int KeepMoving = 1;
	const float VelX = m_pCharacter->Core()->m_Vel.x;

	if(HasDangerInTheDirection(Direction))
	{
		const int DirectionSign = Direction;
		if(VelX * DirectionSign > 0.1)
		{
			BotDebugMessage(VERBOSE_TRACE1, "Brake!");
			KeepMoving = -1;
		}
		else
		{
			KeepMoving = 0;
		}

		BotDebugMessage(VERBOSE_STEPS, "Hold on! The danger is there");
	}

	SetRoamingDirection(Direction);

	BotDebugMessage(VERBOSE_TRACE1, WantToJump ? "WantToJump: yes" : "WantToJump: no");

	pInput->m_Direction = m_RoamingDirection * KeepMoving;
	pInput->m_TargetX = m_LastTargetSeenAtPos.x - Pos.x;
	pInput->m_TargetY = m_LastTargetSeenAtPos.y - Pos.y;

	float FirePerSecond = 0.35f;
	// TODO: This should be the target proximity radius
	float HitDistance = 0;
	float HorizontalHitRatio = 1.0f;

	switch(WeaponType)
	{
	case EInfclassWeapon::HAMMER:
	case EInfclassWeapon::JAWS:
	case EInfclassWeapon::SLIME:
	case EInfclassWeapon::INFECTED_HAMMER:
	case EInfclassWeapon::STUNNING_HAMMER:
		HitDistance = GetCharacterClass()->GetHammerProjOffset() + GetCharacterClass()->GetHammerRange() + m_pCharacter->GetProximityRadius();
		break;
	case EInfclassWeapon::BOOMER_EXPLOSION:
		FirePerSecond = 0.1f;
		HitDistance = 60.0;
		break;
	case EInfclassWeapon::INFECTED_GRENADE:
		HitDistance = 700;
		FirePerSecond = 0.45f;
		HorizontalHitRatio = std::abs(Pos.x - m_LastTargetSeenAtPos.x) / HitDistance;
		pInput->m_TargetY -= (2 + HorizontalHitRatio * 9) * TileSizeF;
		break;
	default:
		break;
	}

	if(GetCharacter()->IsInvisible() && Distance > HitDistance * 2)
	{
		return;
	}
	if(Tick < m_BotStateTick + TickSpeed * 0.15f)
	{
		// Do not attack immediately
		return;
	}

	if(Distance < HitDistance)
	{
		if(GetCharacter()->GetReloadTimer() <= 0)
		{
			if(Tick > m_LastFireTick + TickSpeed * FirePerSecond)
			{
				pInput->m_Fire = true;
			}
		}
	}
	else if (WeaponType == EInfclassWeapon::HAMMER)
	{
		if(Distance < HitDistance * 2)
		{
			FirePerSecond += random_float() * 0.5f;

			if(Tick > m_LastFireTick + TickSpeed * FirePerSecond)
			{
				pInput->m_Fire = true;
			}
		}
		else if(GetClass() == EPlayerClass::Slug)
		{
			pInput->m_TargetX = m_RoamingDirection * TileSize * 4;
			pInput->m_TargetY = TileSize * 2;
		}
		else if(Distance < HitDistance * 4)
		{
			FirePerSecond += random_float() * 1.0f;

			if(Tick > m_LastFireTick + TickSpeed * FirePerSecond)
			{
				pInput->m_Fire = true;
			}
		}
	}

	MaybeHookTheTarget(Distance);

	if(GetClass() == EPlayerClass::Boomer)
	{
		bool DecideToGetEvenCloser = random_prob(0.3f);
		if((pInput->m_Fire && (Distance > 60)) || DecideToGetEvenCloser)
		{
			pInput->m_Fire = false;
			m_LastFireTick = Tick;
		}
	}

	if(m_HookUntilTick >= Tick)
	{
		m_HookAimingRemainingTicks.reset();
		if(s_HiveMind.TryHook(GetCid(), m_LastTarget))
		{
			pInput->m_Hook = 1;
		}
		else
		{
			// Back to aiming
			ResetHooking();
		}
	}

	if(!IsHuman())
	{
		if(pInput->m_Fire)
		{
			if(WeaponType == EInfclassWeapon::HAMMER)
			{
				pInput->m_Fire = s_HiveMind.TryAttack(m_LastTarget);
			}
		}
	}

	if(m_NextRandomFireTick && Tick > m_NextRandomFireTick)
	{
		pInput->m_Fire = true;
		m_NextRandomFireTick = 0;
	}

	bool WantFlee = CanFlee() && (Distance < FleeDistance);
	if(pInput->m_Fire)
	{
		m_LastFireTick = Tick;

		if(WeaponType == EInfclassWeapon::INFECTED_GRENADE)
		{
			pInput->m_TargetY -= random_float(-2.0f, 2.5f) * TileSizeF;

			const vec2 Dir{normalize(vec2(pInput->m_TargetX, pInput->m_TargetY))};
			float Curvature = GameServer()->Tuning()->m_GrenadeCurvature;
			float Speed = GameServer()->Tuning()->m_GrenadeSpeed;
			float Time = Distance / Speed;

			const vec2 ProjStartPos = Pos + Dir * m_pCharacter->GetProximityRadius() * 0.75f;
			const std::optional<vec2> Hit = m_BotUtils.GetHitPos(ProjStartPos, Dir, Curvature, Speed, Time * 1.1f, 1.0f / Server()->TickSpeed());
			constexpr float MaxDistanceSquared = TileSize * 4.5 * TileSize * 4.5;
			if(Hit.has_value() && distance_squared(m_LastTargetSeenAtPos, Hit.value()) > MaxDistanceSquared)
			{
				// Adjust the fire
				pInput->m_Fire = 0;
				m_LastFireTick += FirePerSecond * 0.5 * TickSpeed;
				if(ma_RecentFailedAttackTicks.Size() == ma_RecentFailedAttackTicks.Capacity())
				{
					ma_RecentFailedAttackTicks.RemoveAt(0);
				}
				ma_RecentFailedAttackTicks.Add(Tick);
			}
			else
			{
				m_LastFireTick += (FirePerSecond + random_float(0.8f)) * TickSpeed;
				ma_RecentFailedAttackTicks.Clear();
			}
		}
	}

	if(WeaponType == EInfclassWeapon::INFECTED_GRENADE)
	{
		WantFlee = WantFlee && pInput->m_Fire;
	}

	if(WantFlee)
	{
		SetState(EBotState::Fleeing);
		SetRoamingDirection(OppositeDirection(DirectionToTarget));
		m_FleeingSinceTick = Tick;
	}
}

void CBotPlayer::UpdateHumanBotControls()
{
	m_pCharacter->SetWeapon(WEAPON_HAMMER);

	const float LookupRadius = GetLookupRadius();
	const vec2 &Pos = m_pCharacter->GetPos();
	const vec2 LookupFromPos = Pos + m_pCharacter->GetDirection() * GetLookupOffset();
	int Tick = Server()->Tick();

	icArray<CIcCharacter *, MAX_CLIENTS> aCharacters;
	int Results = GameWorld()->FindEntities(LookupFromPos, LookupRadius, reinterpret_cast<CEntity **>(aCharacters.Data()), aCharacters.Capacity(), CIcCharacter::EntityId);
	aCharacters.Resize(Results);

	vec2 Out;
	vec2 Before;

	float Distance2ToTheEnemy = LookupRadius * LookupRadius + 1;
	const CIcCharacter *pClosestSeenEnemy = nullptr;
	for(const CIcCharacter *pCharacter : aCharacters)
	{
		if(pCharacter->IsHuman() || pCharacter->IsInvisible())
		{
			continue;
		}

		float Distance2ToTheCharacter = distance2(Pos, pCharacter->GetPos());
		if(Distance2ToTheCharacter > Distance2ToTheEnemy)
			continue;

		if(GameServer()->Collision()->IntersectLine(Pos, pCharacter->GetPos(), &Out, &Before))
		{
			continue;
		}

		pClosestSeenEnemy = pCharacter;
		Distance2ToTheEnemy = Distance2ToTheCharacter;
	}

	if(pClosestSeenEnemy)
	{
		SetState(EBotState::Fleeing);
		SetRoamingDirection(pClosestSeenEnemy->GetPos().x < Pos.x ? DIRECTION_RIGHT : DIRECTION_LEFT);
		m_LastTargetSeenAtPos = pClosestSeenEnemy->GetPos();
		m_LastTarget = pClosestSeenEnemy->GetCid();
		m_FleeingSinceTick = Tick;
	}
	else if(m_BotState == EBotState::Fleeing)
	{
		float FleeingDuration = 0.5f;
		if(Tick >= m_FleeingSinceTick + Server()->TickSpeed() * FleeingDuration)
		{
			SetState(EBotState::Roaming);
		}
	}
}

void CBotPlayer::ScheduleRandomFire()
{
	if(GetClass() != EPlayerClass::Slug)
	{
		return;
	}

	if(m_NextRandomFireTick)
		return;

	float FireInterval = 0.2f + random_float();

	if(GetClass() == EPlayerClass::Slug)
	{
		if(m_RoamingObjection != EObjection::Relax)
		{
			FireInterval /= 2.5f;
		}
	}

	m_NextRandomFireTick = Server()->Tick() + Server()->TickSpeed() * FireInterval;
}

const char *CBotPlayer::DumpBot()
{
	static char aBuf[200];
	switch(m_BotState)
	{
	case EBotState::Roaming:
		str_format(aBuf, sizeof(aBuf), "Roaming | Direction: %d, objection: %s, extra jumps proba: %.2f",
			m_RoamingDirection, toString(m_RoamingObjection), m_JumpExtraProbability);
		break;
	case EBotState::Hunting:
		str_format(aBuf, sizeof(aBuf), "Hunting | Target: %d (%.2f, %.2f)",
			m_LastTarget,
			m_LastTargetSeenAtPos.x / 32,
			m_LastTargetSeenAtPos.y / 32);
		break;
	case EBotState::Fleeing:
		str_format(aBuf, sizeof(aBuf), "Fleeing from target: %d (%.2f, %.2f)",
			m_LastTarget,
			m_LastTargetSeenAtPos.x / 32,
			m_LastTargetSeenAtPos.y / 32);
		break;
	case EBotState::Invalid:
		break;
	}

	return aBuf;
}

void CBotPlayer::SetAiEnabled(bool Enabled)
{
	AiEnabled = Enabled;
}

void CBotPlayer::SetObjectionEnabled(EObjection Objection, bool Enabled)
{
	std::optional<std::size_t> Index = AiBannedObjections.IndexOf(Objection);
	if(Index.has_value())
	{
		if(Enabled)
		{
			AiBannedObjections.RemoveAt(Index.value());
		}
		else
		{
		  // Do nothing
		}
	}
	else
	{
		if(Enabled)
		{
		  // Do nothing
		}
		else
		{
			AiBannedObjections.Add(Objection);
		}
	}
}

void CBotPlayer::ResetEnabledObjections()
{
	AiBannedObjections.Clear();
}

void CBotPlayer::UpdateName()
{
	if((m_MaxLives != InfinityLives))
	{
		if(m_MaxHP && (m_Lives > 0))
		{
			str_format(m_Name, sizeof(m_Name), "Bot%d (%d HP)", GetCid(), m_pCharacter ? m_pCharacter->GetHealthArmorSum() : m_MaxHP);
		}
		else
		{
			str_format(m_Name, sizeof(m_Name), "Bot%d (%d/%d)", GetCid(), m_Lives, m_MaxLives);
		}
	}
	else
	{
		str_format(m_Name, sizeof(m_Name), "Bot%d", GetCid());
	}

	Server()->SetClientName(GetCid(), m_Name);
}

void CBotPlayer::OnCharacterHPChanged()
{
	if(m_MaxHP)
	{
		UpdateName();
	}
}

bool CBotPlayer::IsDebugEnabled(int Verbosity) const
{
	return (Verbosity < g_Config.m_InfBotDebugLevel) && (g_Config.m_InfDebugBot < 0 || g_Config.m_InfDebugBot == GetCid());
}

void CBotPlayer::BotDebugMessage(int VerbosityLevel, const char *fmt, ...) const
{
	if(!IsDebugEnabled(VerbosityLevel))
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

	m_BotUtils.GetDebugSink()->SendFormattedMessage(VerbosityLevel, aBuf);
}

bool CBotPlayer::HasWallInTheDirection(DIRECTION Direction) const
{
	if(!m_pCharacter || (Direction == DIRECTION_NONE))
	{
		return false;
	}

	const vec2 &Pos = m_pCharacter->GetPos();
	static const float OneThirdProximityRadius = m_pCharacter->GetProximityRadius() / 3.0f;
	const int DirectionSign = Direction;

	const int XOffset = DirectionSign * (OneThirdProximityRadius + TileSize * 0.5f);

	return IsSolidTile(Pos + vec2(XOffset, -OneThirdProximityRadius)) || IsSolidTile(Pos + vec2(XOffset, +OneThirdProximityRadius));
}

bool CBotPlayer::HasDangerInTheDirection(DIRECTION Direction) const
{
	if(!m_pCharacter || (Direction == DIRECTION_NONE))
	{
		return false;
	}

	const vec2 &Pos = m_pCharacter->GetPos();
	static const float ProximityRadius = m_pCharacter->GetProximityRadius();
	const int DirectionSign = Direction;

	const int XOffset = DirectionSign * (ProximityRadius / 2 + TileSize * 0.9);

	int DamageIndex1 = GameController()->GetDamageZoneValueAt(Pos + vec2(XOffset, -ProximityRadius / 2));
	int DamageIndex2 = GameController()->GetDamageZoneValueAt(Pos + vec2(XOffset, +ProximityRadius / 2));

	icArray<int, 5> BadIndices = {
		ZONE_DAMAGE_DEATH,
	};

	if(IsHuman())
	{
		BadIndices.Add(ZONE_DAMAGE_INFECTION);
	}

	if(BadIndices.Contains(DamageIndex1) || BadIndices.Contains(DamageIndex2))
		return true;

	return false;
}

bool CBotPlayer::HasDangerBelow() const
{
	const vec2 &Pos = m_pCharacter->GetPos();
	static const float ProximityRadius = m_pCharacter->GetProximityRadius();

	icArray<int, 5> BadIndices = {
		ZONE_DAMAGE_DEATH,
	};

	if(IsHuman())
	{
		BadIndices.Add(ZONE_DAMAGE_INFECTION);
	}

	const float X1 = Pos.x + TileSize / 2;
	const float X2 = Pos.x - TileSize / 2;
	const float Y1 = Pos.y + ProximityRadius / 2 + TileSize * 0.9f;
	for (int i = 0 ; i < 3; ++i)
	{
		int DamageIndex1 = GameController()->GetDamageZoneValueAt(vec2(X1, Y1 + i * TileSize));
		int DamageIndex2 = GameController()->GetDamageZoneValueAt(vec2(X2, Y1 + i * TileSize));

		if(BadIndices.Contains(DamageIndex1) || BadIndices.Contains(DamageIndex2))
		{
			return true;
		}
	}

	return false;
}

EThreatLevel CBotPlayer::GetDangerLevelAhead(vec2 *pThreatPosition, CIcEntity **ppThreatEntity) const
{
	const CIcCharacter *pCharacter = GetCharacter();
	if(!pCharacter || pCharacter->IsBlind())
	{
		return EThreatLevel::Zero;
	}

	const float PredictTime = 0.25f; // Predict 1/4 of the next second

	const vec2 &Pos = pCharacter->GetPos();
	const int DirectionSign = m_RoamingDirection;
	const float Acceleration = c_AirControlAccel * DirectionSign;
	const float MaxHDistance = CBotUtils::GetDistanceForVelocityAccelerationTicks(pCharacter->Core()->m_Vel.x, Acceleration, Server()->TickSpeed() * PredictTime, c_AirControlSpeed);
	const vec2 EndPos = Pos + vec2(MaxHDistance, 0);

	return GetDangerLevelOnLine(Pos, EndPos, pThreatPosition, ppThreatEntity);
}

EThreatLevel CBotPlayer::GetDangerLevelOnLine(const vec2 &From, vec2 To, vec2 *pThreatPosition, CIcEntity **ppThreatEntity) const
{
	if(IsHuman())
	{
		// Bot-human feels no fear
		// TODO: ENTTYPE_SLUG_SLIME
		return EThreatLevel::Zero;
	}

	static const float ProximityRadius = m_pCharacter->GetProximityRadius();
	vec2 ResultPos;

	const std::pair<int, EThreatLevel>
		Threats[] = {
			{CEngineerWall::EntityId, EThreatLevel::Deadly},
			{CScientistMine::EntityId, EThreatLevel::Deadly},
			{CMercenaryBomb::EntityId, EThreatLevel::Dangerous},
			{CBiologistMine::EntityId, EThreatLevel::Dangerous},

			{CScatterGrenade::EntityId, EThreatLevel::Suspicious},
			{CLooperWall::EntityId, EThreatLevel::Suspicious},
		};

	{
		vec2 BeforeCol;
		if(GameServer()->Collision()->IntersectLine(From, To, nullptr, &BeforeCol))
			To = BeforeCol;
	}

	EThreatLevel Result = EThreatLevel::Zero;
	float ResultLen2 = distance2(From, To);
	CIcEntity *pResultEntity = nullptr;

	for(const std::pair<int, EThreatLevel> &ThreatSpec : Threats)
	{
		CEntity *pThreatEntity = GameWorld()->IntersectEntity(From, To, ProximityRadius, &ResultPos, ThreatSpec.first);
		if(!pThreatEntity)
			continue;

		// Incorrect math, distance in square but proximity is not. Do not care for now :eyes:
		float Len2 = distance2(From, ResultPos) - pThreatEntity->GetProximityRadius();
		if(Len2 < ResultLen2)
		{
			ResultLen2 = Len2;
			pResultEntity = static_cast<CIcEntity *>(pThreatEntity);

			if(ThreatSpec.first == CScientistMine::EntityId)
			{
				// Typical mine damage is up to 6 + 6 * 2
				if(m_pCharacter->GetHealthArmorSum() > 18)
				{
					Result = EThreatLevel::Dangerous;
				}
				else
				{
					Result = EThreatLevel::Deadly;
				}
			}
			else
			{
				Result = ThreatSpec.second;
			}
		}
	}

	if(IsDebugEnabled(VERBOSE_TRACE1))
	{
		m_pGameServer->CreateHammerDotEvent(From, Server()->TickSpeed() * 1.0);
		m_pGameServer->CreateHammerDotEvent(To, Server()->TickSpeed() * 2.0);
		if(Result != EThreatLevel::Zero)
		{
			m_pGameServer->CreateHammerDotEvent(ResultPos, Server()->TickSpeed() * 3.0);
		}
	}

	if(pThreatPosition)
	{
		*pThreatPosition = ResultPos;
	}
	if(ppThreatEntity)
	{
		*ppThreatEntity = pResultEntity;
	}

	return Result;
}

int CBotPlayer::GetJumpsNeededToGetOverWall(DIRECTION Direction, int MaxJumps, vec2 *pTargetPosition) const
{
	if(MaxJumps < 0)
		MaxJumps = GetAvailableJumps();

	if(MaxJumps == 0)
	{
		return 0;
	}

	BotDebugMessage(VERBOSE_TRACE1, "Evaluating jump over...");
	int AvailableJumpY = GetAirTilesAbove(Direction, MaxJumps);
	if(AvailableJumpY <= 0)
	{
		return 0;
	}

	const vec2 &Pos = m_pCharacter->GetPos();
	static const float HalfProximityRadius = m_pCharacter->GetProximityRadius() / 2;
	const int DirectionSign = Direction;

	const int XOffset = DirectionSign * (HalfProximityRadius + TileSize * 0.5);
	const float WallPosX = Pos.x + XOffset;

	int NeedJumps = 0;
	for(int i = 1; i <= AvailableJumpY; ++i)
	{
		int CheckPosY = Pos.y - i * TileSize;
		BotDebugMessage(VERBOSE_TRACE2, "Check the wall at %.2f x %.2f: ", WallPosX / TileSizeF, CheckPosY / TileSizeF);

		int Tile = GameServer()->Collision()->GetCollisionAt(WallPosX, CheckPosY);
		static const icArray<int, 2> SolidTiles = {TILE_SOLID, TILE_NOHOOK};
		if(!SolidTiles.Contains(Tile))
		{
			int DamageIndex = GameController()->GetDamageZoneValueAt(vec2(WallPosX, CheckPosY));
			if(DamageIndex == ZONE_DAMAGE_DEATH)
			{
				// Unacceptable tile for a jump
				continue;
			}

			const int PlatformX = WallPosX / TileSize;
			const int PlatformY = CheckPosY / TileSize + 1;
			const int ExtraLength = 2;

			vec2 TargetPos(PlatformX * TileSize + TileSize * 0.5 - (TileSize * 0.5) * DirectionSign, PlatformY * TileSize - HalfProximityRadius - ExtraLength);
			vec2 VectorToTarget = TargetPos - Pos;
			NeedJumps = GetJumpsToReachTarget(VectorToTarget);

			if(NeedJumps <= MaxJumps)
			{
				BotDebugMessage(VERBOSE_TRACE1, "Can! jump over: av: %d, jump at %d", AvailableJumpY, i);

				if(pTargetPosition)
				{
					*pTargetPosition = TargetPos;
				}
			}
			break;
		}
	}

	if(NeedJumps)
	{
		BotDebugMessage(VERBOSE_TRACE1, "Can jump over");
	}
	else
	{
		BotDebugMessage(VERBOSE_TRACE1, "Can't jump over");
	}

	return NeedJumps;
}

int CBotPlayer::GetJumpsNeededToJumpOnPlatform(DIRECTION Direction, int MaxJumps, vec2 *pTargetPosition) const
{
	BotDebugMessage(VERBOSE_TRACE1, "Evaluating jump on a platform...");

	if(MaxJumps < 0)
	{
		MaxJumps = GetAvailableJumps();
	}

	// const float MaxControlSpeed = IsGrounded() ? c_GroundControlSpeed : c_AirControlSpeed;
	// float MaxHSpeed = std::max<float>(MaxControlSpeed, fabs(m_pCharacter->Core()->m_Vel.x));
	const int DirectionSign = Direction;
	const float AirControlAccel = m_NextTuningParams.m_AirControlAccel;
	const float AirControlSpeed = m_NextTuningParams.m_AirControlSpeed;
	const float Acceleration = AirControlAccel * DirectionSign;
	int Ticks = m_BotUtils.GetJumpTicksInAir(MaxJumps, IsGrounded());
	const int MaxTicks = Server()->TickSpeed() * 1.5f;
	if(Ticks > MaxTicks)
		Ticks = MaxTicks;
	const float MaxHDistance = CBotUtils::GetDistanceForVelocityAccelerationTicks(m_pCharacter->Core()->m_Vel.x, Acceleration, Ticks, AirControlSpeed);

	return GetJumpsNeededToJumpOnPlatform(Direction, MaxJumps, pTargetPosition, MaxHDistance);
}

int CBotPlayer::GetJumpsNeededToJumpOnPlatform(DIRECTION Direction, int MaxJumps, vec2 *pTargetPosition, float MaxHDistance) const
{
	if(!Direction)
		return 0;

	static const float HalfProximityRadius = m_pCharacter->GetProximityRadius() / 2;
	const int DirectionSign = Direction;

	const vec2 &Pos = m_pCharacter->GetPos();
	int MaxHTiles = fabs(MaxHDistance / TileSize);
	int MaxTiles = GetMaxTilesForJumps(MaxJumps);

	// float HVelocity = absolute(m_pCharacter->Core()->m_Vel.x);
	int MinHTile = 0; // Dep on velocity

	const float CharHorOffset = DirectionSign * HalfProximityRadius;
	const float CharPosX = Pos.x + CharHorOffset;
	int NeedJumps = 0;

	if(IsDebugEnabled(VERBOSE_TRACE2))
	{
		BotDebugMessage(VERBOSE_TRACE2, "jump max H tiles: %.2f", MaxHDistance);
	}

	const CCollision *pCollision = GameServer()->Collision();
	const int MapWidth = pCollision->GetWidth();
	const int MapHeight = pCollision->GetHeight();
	const int BottomRowIndex = MapWidth * (MapHeight - 1);

	const auto GetMapIndexBelow = [MapWidth, BottomRowIndex](int ReferenceIndex) {
		if (ReferenceIndex > BottomRowIndex)
			return ReferenceIndex;

		return ReferenceIndex + MapWidth;
	};

	int MaxReachableTilesAbove = MaxTiles;
	for(int horTile = MinHTile; horTile < MaxHTiles; horTile++)
	{
		// Min hor tile is 1
		const float AirPosX = CharPosX + horTile * TileSize * DirectionSign;

		int AirTilesAbove = GetAirTilesAboveAtX(MaxTiles, AirPosX);
		if(AirTilesAbove <= 0)
		{
			return 0;
		}

		if(AirTilesAbove < MaxReachableTilesAbove)
			MaxReachableTilesAbove = AirTilesAbove;

		int PrevRefMapIndex = -1;
		// horTile + 1 because we're looking for the next tile after current air
		const float WallPosX = CharPosX + (horTile + 1) * TileSize * DirectionSign;
		for(int i = MaxReachableTilesAbove; i > 0; --i)
		{
			float CheckPosY = Pos.y - i * TileSize;
			int ReferenceMapIndex;
			if(PrevRefMapIndex < 0)
			{
				ReferenceMapIndex = pCollision->GetPureMapIndex(WallPosX, CheckPosY);
			}
			else
			{
				ReferenceMapIndex = PrevRefMapIndex;
				ReferenceMapIndex = CheckPosY < TileSize ? ReferenceMapIndex : GetMapIndexBelow(ReferenceMapIndex);
			}

			PrevRefMapIndex = ReferenceMapIndex;
			int TileIndex = pCollision->GetTileIndex(ReferenceMapIndex);
			bool HasAirThere = !(TileIndex == TILE_SOLID || TileIndex == TILE_NOHOOK);

			if(IsDebugEnabled(VERBOSE_TRACE2))
			{
				BotDebugMessage(VERBOSE_TRACE2, "Check the wall at %.2f x %.2f: %s", WallPosX / TileSize, CheckPosY / TileSize, HasAirThere ? "air" : "solid");
			}

			if(HasAirThere)
			{
				int MapIndexBelow = CheckPosY < TileSize ? ReferenceMapIndex : GetMapIndexBelow(ReferenceMapIndex);
				const int TileIndexBelow = pCollision->GetTileIndex(MapIndexBelow);
				const bool HasSolidBelow = TileIndexBelow == TILE_SOLID || TileIndexBelow == TILE_NOHOOK;

				if(HasSolidBelow)
				{
					if(IsDebugEnabled(VERBOSE_TRACE1))
					{
						BotDebugMessage(VERBOSE_TRACE1, "Found a platform at %.2f x %.2f: ", WallPosX / TileSize, CheckPosY / TileSize + 1);
					}

					const int PlatformX = WallPosX / TileSize;
					const int PlatformY = CheckPosY / TileSize + 1;
					const int ExtraLength = 2;

					vec2 TargetPos(PlatformX * TileSize + TileSize * 0.5 - (TileSize * 0.5) * DirectionSign, PlatformY * TileSize - HalfProximityRadius - ExtraLength);
					vec2 VectorToTarget = TargetPos - Pos;
					int JumpsToReach = GetJumpsToReachTarget(VectorToTarget);
					if(JumpsToReach <= MaxJumps)
					{
						NeedJumps = JumpsToReach;
						if(IsDebugEnabled(VERBOSE_TRACE1))
						{
							BotDebugMessage(VERBOSE_TRACE1, "Can! jump on: av: %d, jump at %d", MaxReachableTilesAbove, i);
						}

						if(pTargetPosition)
						{
							*pTargetPosition = TargetPos;
						}
						break;
					}
				}
			}
		}

		if(NeedJumps)
		{
			break;
		}
	}

	if(NeedJumps)
	{
		BotDebugMessage(VERBOSE_TRACE1, "Can jump on in %d jumps", NeedJumps);
	}
	else
	{
		BotDebugMessage(VERBOSE_TRACE1, "Can't jump on");
	}

	return NeedJumps;
}

int CBotPlayer::GetAirTilesAbove(DIRECTION Direction, int MaxJumps) const
{
	if(!m_pCharacter)
	{
		return 0;
	}

	if(MaxJumps < 0)
	{
		MaxJumps = GetAvailableJumps();
	}

	if(MaxJumps == 0)
	{
		return 0;
	}

	float MaxTiles = GetMaxTilesForJumps(MaxJumps);

	const vec2 &Pos = m_pCharacter->GetPos();
	return m_BotUtils.GetAirTilesAbove(Pos, MaxTiles);
}

float CBotPlayer::GetAirTilesAboveAtX(int MaxTiles, float CheckPosX) const
{
	const vec2 &Pos = m_pCharacter->GetPos();
	const float BaseY = Pos.y;

	return m_BotUtils.GetAirTilesAbove(vec2(CheckPosX, BaseY), MaxTiles);
}

int CBotPlayer::GetMaxJumps() const
{
	if(!m_pCharacter)
	{
		return 0;
	}

	int Result = m_pCharacter->Core()->m_Jumps;
	if(Result > c_JumpsHardLimit)
		Result = c_JumpsHardLimit;

	return Result;
}

void CBotPlayer::SetState(EBotState NewState)
{
	if(m_BotState == NewState)
	{
		return;
	}

	if(NewState != EBotState::Roaming)
	{
		SetPOI(std::nullopt);
	}

	if(NewState == EBotState::Roaming)
	{
		if(IsHuman())
		{
			GameServer()->SendEmoticon(GetCid(), EMOTICON_MUSIC);
		}
		else
		{
			BotDebugMessage(VERBOSE_MAIN, "SwitchState: ROAMING");
		}
	}
	else if(NewState == EBotState::Hunting)
	{
		BotDebugMessage(VERBOSE_MAIN, "SwitchState: HUNTING");

		if(IsHuman())
		{
			GameServer()->SendEmoticon(GetCid(), EMOTICON_HEARTS);
		}
		else
		{
			static const int HuntingEmotes[] = {
				EMOTICON_SPLATTEE,
				EMOTICON_DEVILTEE,
				EMOTICON_ZOMG,
			};
			GameServer()->SendEmoticon(GetCid(), HuntingEmotes[random_int(0, 2)]);
		}
	}
	else if(NewState == EBotState::Fleeing)
	{
		if(IsHuman())
		{
			GameServer()->SendEmoticon(GetCid(), EMOTICON_DROP);
		}
		else
		{
			GameServer()->SendEmoticon(GetCid(), EMOTICON_DOTDOT);
		}
	}

	m_BotState = NewState;
	m_BotStateTick = Server()->Tick();
}

void CBotPlayer::SetRoamingDirection(DIRECTION Direction)
{
	m_RoamingBehaviorTick = Server()->Tick();
	m_RoamingBehaviorChangeTick = 0;
	m_RoamingDirection = Direction;
}

void CBotPlayer::SetObjection(EObjection Objection)
{
	if(m_RoamingObjection == Objection)
		return;

	BotDebugMessage(VERBOSE_MAIN, "Objection: %s", toString(Objection));
	m_RoamingObjection = Objection;
}

bool CBotPlayer::IsBehaviorChangeQueued() const
{
	return m_RoamingBehaviorChangeTick;
}

void CBotPlayer::QueueBehaviorChange()
{
	QueueBehaviorChange(0.125f + random_float());
}

void CBotPlayer::QueueBehaviorChange(float Delay)
{
	const int Tick = Server()->Tick();
	m_RoamingBehaviorChangeTick = Tick + Delay * Server()->TickSpeed();
}

void CBotPlayer::MaybeChangeRoamingBehavior()
{
	const float BehaviorChangeInterval = 45.0;
	const float LastSeenTrackingDuration = 5.0;
	const int Tick = Server()->Tick();

	if(m_RoamingObjection == EObjection::CheckTheLastSeen)
	{
		if(Tick > m_LastSeenTick + Server()->TickSpeed() * LastSeenTrackingDuration)
		{
			SetObjection(m_TargetLastSeenDirObjection);
		}

		return;
	}

	if(m_RoamingBehaviorChangeTick)
	{
		if(Tick >= m_RoamingBehaviorChangeTick)
		{
			ChangeRoamingBehavior();
		}
	}
	else if(Tick > m_RoamingBehaviorTick + Server()->TickSpeed() * BehaviorChangeInterval)
	{
		QueueBehaviorChange();
	}
}

void CBotPlayer::ChangeRoamingBehavior()
{
	BotDebugMessage(VERBOSE_MAIN, "Changing roaming behavior");
	SetRoamingDirection(OppositeDirection(m_RoamingDirection));

	m_DecisionPos = {};

	switch(m_RoamingObjection)
	{
	case EObjection::Relax:
	case EObjection::Jump:
	case EObjection::Lookup:
	case EObjection::CheckTheLastSeen:
	case EObjection::CheckPOI:
		GetNewObjection();
		break;
	case EObjection::CheckTheTop:
	case EObjection::CheckTheMid:
	case EObjection::CheckTheBottom:
		if(random_prob(0.5f))
		{
			SetObjection(EObjection::Lookup);
			GetNewObjection();
		}
		else
		{
			return;
		}
		break;
	case EObjection::Invalid:
		// Invalid
		break;
	}

	ma_IgnorePoints.Clear();

	int Emoticon = -1;
	switch(m_RoamingObjection)
	{
	case EObjection::Relax:
		m_JumpExtraProbability = random_float() - 0.5f;
		Emoticon = EMOTICON_ZZZ;
		break;
	case EObjection::Jump:
		Emoticon = EMOTICON_MUSIC;
		m_JumpExtraProbability = random_float();
		break;
	case EObjection::CheckTheTop:
		m_JumpExtraProbability = 0;
		break;
	case EObjection::CheckTheMid:
		m_JumpExtraProbability = 0;
		break;
	case EObjection::CheckTheBottom:
		m_JumpExtraProbability = -1;
		break;
	case EObjection::CheckPOI:
		m_JumpExtraProbability = 0;
		Emoticon = EMOTICON_QUESTION;
		break;
	case EObjection::Lookup:
	case EObjection::CheckTheLastSeen:
	case EObjection::Invalid:
		m_JumpExtraProbability = 0;
		break;
	}

	if(Emoticon >= 0)
	{
		GameServer()->SendEmoticon(GetCid(), Emoticon);
	}

	BotDebugMessage(VERBOSE_STEPS, "ChangeRoaming| Direction: %d, objection: %s, extra jumps: %.2f",
		m_RoamingDirection, toString(m_RoamingObjection), m_JumpExtraProbability);
}

void CBotPlayer::GetNewObjection()
{
	auto DisabledObjections = AiBannedObjections;
	if(DisabledObjections.Size() == DisabledObjections.Capacity())
	{
		// All objections disabled
		return;
	}

	if(!m_RecentObjections.IsEmpty())
	{
		DisabledObjections.Add(m_RecentObjections.Last());
	}

	EObjection NewObjection = EObjection::Invalid;
	if(m_RoamingObjection == EObjection::Lookup)
	{
		int PlayersAbove = 0;
		int PlayersMid = 0;
		int PlayersBelow = 0;
		// Find if players are above or below

		const vec2 &OwnPos = GetCharacter()->GetPos();

		for(const vec2 &HumanPos : s_HiveMind.GetHumanPositions())
		{
			if(HumanPos.y > OwnPos.y)
			{
				PlayersBelow++;
			}
			else
			{
				bool Reachable = m_BotUtils.IsReachableByGround(OwnPos, HumanPos, GetMaxJumps());

				if(Reachable)
				{
					PlayersMid++;
				}
				else
				{
					PlayersAbove++;
				}
			}
		}

		if(PlayersAbove + PlayersMid + PlayersBelow == 0)
		{
			BotDebugMessage(VERBOSE_STEPS, "Looking up. No players online. Hack the numbers!");
			PlayersAbove = 1;
			PlayersMid = 1;
			PlayersBelow = 1;
		}
		else
		{
			BotDebugMessage(VERBOSE_STEPS, "Looking up. PlayersAbove: %d, PlayersMid: %d, PlayersBelow: %d", PlayersAbove, PlayersMid, PlayersBelow);
		}

		double Probas[3] = {PlayersAbove * 1.0, PlayersMid * 1.0, PlayersBelow * 1.0};
		int Obj = random_distribution(std::begin(Probas), std::end(Probas));
		if(Obj == 0)
		{
			NewObjection = EObjection::CheckTheTop;
		}
		else if(Obj == 1)
		{
			NewObjection = EObjection::CheckTheMid;
		}
		else
		{
			NewObjection = EObjection::CheckTheBottom;
		}
	}
	else
	{
		do
		{
			NewObjection = EObjection(random_int(0, static_cast<int>(EObjection::Count) > -1));
		} while(DisabledObjections.Contains(NewObjection));
	}

	SetObjection(NewObjection);

	if(m_RecentObjections.Size() == m_RecentObjections.Capacity())
	{
		m_RecentObjections.RemoveAt(0);
	}
	m_RecentObjections.Add(m_RoamingObjection);
}

void CBotPlayer::MaybeHookTheTarget(float Distance)
{
	if(!CanHook())
	{
		ResetHooking();
		return;
	}

	const int Tick = Server()->Tick();
	if(aHookyClasses.Contains(GetClass()))
	{
		if(m_pCharacter->Core()->HookedPlayer() == m_LastTarget)
		{
			// Keep hooking
			m_HookUntilTick = Tick + 1;
			return;
		}
	}

	float HookDuration = GameServer()->Tuning()->m_HookDuration;
	float MinimumDelay = 0.5f;
	float RandomExtraDelay = 0.5f;
	if(GameController()->HardMode())
		RandomExtraDelay = 0.2f;

	if(StrongHook())
	{
		MinimumDelay *= 0.75f;
		RandomExtraDelay *= 0.75;
	}
	else if(WeakHook())
	{
		HookDuration *= 0.75f;
		MinimumDelay *= 1.25f;
		RandomExtraDelay *= 1.25f;
	}

	if(aHookyClasses.Contains(GetClass()))
	{
		MinimumDelay *= 0.75f;
		RandomExtraDelay *= 0.75f;
	}

	if(m_pCharacter->Core()->m_HookState == HOOK_GRABBED)
	{
		if(m_pCharacter->Core()->HookedPlayer() != m_LastTarget)
		{
			// Grabbed something else. Release.
			ResetHooking();
			return;
		}
	}

	if(Distance > GetMaxHookDistance() + TileSize * 8)
	{
		m_HookAimingRemainingTicks.reset();
		return;
	}

	const bool AlreadyHooking = m_HookUntilTick >= Tick;
	if(!AlreadyHooking && (Distance < GetMaxHookDistance()))
	{
		if(m_HookAimingRemainingTicks.has_value())
		{
			if(m_HookAimingRemainingTicks.value() <= 0)
			{
				m_HookUntilTick = Tick + Server()->TickSpeed() * HookDuration;
			}
		}
		else
		{
			// Start aiming
			m_HookAimingRemainingTicks = Server()->TickSpeed() * (MinimumDelay + random_float() * RandomExtraDelay);
		}
	}
}

void CBotPlayer::ResetHooking()
{
	m_HookAimingRemainingTicks.reset();
	m_HookUntilTick = -1;
}

bool CBotPlayer::IsMovingInDirection(DIRECTION Direction, float MinVelocity) const
{
	const float VelX = m_pCharacter->Core()->m_Vel.x;
	switch(Direction)
	{
	case DIRECTION_LEFT:
		return VelX < -MinVelocity;
	case DIRECTION_RIGHT:
		return VelX > MinVelocity;
	case DIRECTION_NONE:
		break;
	}

	return false;
}

bool CBotPlayer::IsSolidTile(float X, float Y) const
{
	return GameServer()->Collision()->CheckPoint(X, Y);
}

bool CBotPlayer::IsSolidTile(const vec2 &Point) const
{
	return IsSolidTile(Point.x, Point.y);
}

bool CBotPlayer::IsGrounded() const
{
	return m_CachedGrounded;
}

int CBotPlayer::GetAvailableJumps() const
{
	const CIcCharacter *pCharacter = GetCharacter();
	const int UsedJumps = IsGrounded() ? 0 : pCharacter->Core()->m_JumpedTotal + 1;
	const int AvailableJumps = pCharacter->Core()->m_Jumps - UsedJumps;
	int Result = AvailableJumps > 0 ? AvailableJumps : 0;

	if(Result > c_JumpsHardLimit)
		Result = c_JumpsHardLimit;

	return Result;
}

float CBotPlayer::GetMaxTilesForJumps(int Jumps) const
{
	return m_BotUtils.GetMaxTilesForJumps(Jumps, IsGrounded());
}

int CBotPlayer::GetJumpsToReachTarget(float TileY) const
{
	if(TileY == 0)
		return 0;

	for(int i = 0; i < c_JumpsHardLimit; ++i)
	{
		float MaxTile = GetMaxTilesForJumps(i);
		if(MaxTile >= TileY)
		{
			return i;
		}
	}

	return c_JumpsHardLimit * 2;
}

int CBotPlayer::GetJumpsToReachTarget(const vec2 &VectorToTarget) const
{
	const float TilesUp = -VectorToTarget.y / TileSize;
	const int NeedJumps = GetJumpsToReachTarget(TilesUp);
	return NeedJumps;
}

bool CBotPlayer::MaybeFallDown() const
{
	if(m_RoamingObjection == EObjection::CheckTheLastSeen)
	{
		return m_BotUtils.IsReachableByGround(m_pCharacter->GetPos(), m_LastTargetSeenAtPos, GetMaxJumps());
	}
	if (m_RoamingObjection == EObjection::CheckPOI)
	{
		return m_CachedPOIReachableByGround;
	}

	if(!IsHuman())
	{
		EDecision GoodDecision = s_HiveMind.GetGoodDecision(this);
		// Good decision:
		switch(GoodDecision)
		{
		case EDecision::Jump:
			BotDebugMessage(VERBOSE_STEPS, "Someone jumped previously and it was good. Jump.");
			return false;
		case EDecision::NoJump:
			BotDebugMessage(VERBOSE_STEPS, "Someone did not jump here previously and it was good. Do not jump.");
			return true;
		default:
			break;
		}
	}

	EDecision PreviousDecision = GetPreviousDecision();
	if(PreviousDecision == EDecision::Jump)
	{
		BotDebugMessage(VERBOSE_STEPS, "Jumped previously, do not jump now");
		return true;
	}
	if(PreviousDecision == EDecision::NoJump)
	{
		BotDebugMessage(VERBOSE_STEPS, "Didn't jump previously, jump now");
		return false;
	}

	if(m_RoamingObjection == EObjection::CheckTheBottom)
	{
		return true;
	}

	if(m_RoamingObjection == EObjection::CheckTheTop)
	{
		return false;
	}

	bool Jump = random_prob(0.4f + m_JumpExtraProbability);
	return !Jump;
}

bool CBotPlayer::MaybeJumpOverWall(const vec2 &JumpTargetPosition) const
{
	if(m_RoamingObjection == EObjection::Relax)
	{
		return random_prob(0.6f);
	}

	return true;
}

bool CBotPlayer::MaybeJumpOnPlatform(const vec2 &JumpTargetPosition, bool ForceIgnoreIfChecked)
{
	if(m_RoamingObjection == EObjection::CheckTheLastSeen)
	{
		return m_LastTargetSeenAtPos.y < JumpTargetPosition.y + TileSizeF / 2;
	}

	const vec2 &Pos = m_pCharacter->GetPos();
	STilePosition ShortPos = JumpPosToShortPos(JumpTargetPosition, Pos);
	for(auto &CheckPoint : ma_IgnorePoints)
	{
		if(CheckPoint.TilePos == ShortPos)
		{
			return false;
		}
	}

	bool HasPosition = false;
	for(int i = 0; i < ma_CheckPoints.Size(); ++i)
	{
		const auto &CheckPoint = ma_CheckPoints.At(i);
		if(CheckPoint.TilePos == ShortPos)
		{
			HasPosition = true;
			break;
		}
	}

	const bool Checked = HasPosition || (!IsHuman() && s_HiveMind.IsPositionChecked(ShortPos));
	if(Checked && ForceIgnoreIfChecked)
		return false;

	constexpr float ChanceToIgnoreAlreadyCheckedPosition = 0.75f;
	constexpr float ChanceToCheckUncheckedPosition = 0.15f;

	if(Checked)
	{
		if(random_prob(ChanceToIgnoreAlreadyCheckedPosition))
		{
			return false;
		}
	}

	if(!IsHuman())
	{
		DIRECTION CheckDirection = JumpTargetPosition.x > Pos.x ? DIRECTION_RIGHT : DIRECTION_LEFT;
		EDecision GoodDecision = s_HiveMind.GetGoodDecision(this, CheckDirection);
		if(GoodDecision != EDecision::Invalid)
		{
			// Definitely go!
			if(random_prob(0.9f))
			{
				GameServer()->SendEmoticon(GetCid(), EMOTICON_EXCLAMATION);

				// Good decision:
				switch(GoodDecision)
				{
				case EDecision::Jump:
					BotDebugMessage(VERBOSE_STEPS, "Someone jumped previously and it was good. Jump.");
					return true;
				case EDecision::NoJump:
					BotDebugMessage(VERBOSE_STEPS, "Someone did not jump here previously and it was good. Do not jump.");
					return false;
				default:
					break;
				}
			}
		}
	}

	EDecision PreviousDecision = GetPreviousDecision();
	if(PreviousDecision == EDecision::Jump)
	{
		BotDebugMessage(VERBOSE_STEPS, "Jumped previously, do not jump now");
		return false;
	}
	if(PreviousDecision == EDecision::NoJump)
	{
		BotDebugMessage(VERBOSE_STEPS, "Didn't jump previously, jump now");
		return true;
	}

	if(m_RoamingObjection == EObjection::CheckTheBottom)
	{
		if(!Checked)
		{
			if(random_prob(ChanceToCheckUncheckedPosition))
			{
				BotDebugMessage(VERBOSE_STEPS, "Jump by a chance (check the bottom)");
				return true;
			}
		}

		BotDebugMessage(VERBOSE_STEPS, "Do not jump (check the bottom)");
		return false;
	}

	if(m_RoamingObjection == EObjection::CheckTheMid)
	{
		if(!Checked)
		{
			if(random_prob(ChanceToCheckUncheckedPosition))
			{
				BotDebugMessage(VERBOSE_STEPS, "Jump by a chance (check the mid)");
				return true;
			}
		}

		BotDebugMessage(VERBOSE_STEPS, "Do not jump (check the mid)");
		return false;
	}

	if(m_RoamingObjection == EObjection::CheckTheTop)
	{
		BotDebugMessage(VERBOSE_STEPS, "Do jump (check the top)");
		return true;
	}

	return random_prob(0.2 + m_JumpExtraProbability);
}

bool CBotPlayer::MaybeRandomJumpUp() const
{
	return random_prob(m_JumpExtraProbability);
}

int CBotPlayer::GetJumpsToAvoidDanger(vec2 *pTargetPosition) const
{
	int MaxJumps = GetAvailableJumps();
	if(HasDangerBelow())
	{
		int NeedJumps = 0;
		bool CheckAnotherDir = true;
		float MaxHDistance = TileSizeF * 3;
		NeedJumps = GetJumpsNeededToJumpOnPlatform(m_RoamingDirection, MaxJumps, pTargetPosition, MaxHDistance);
		if(NeedJumps == 0 || NeedJumps > MaxJumps)
		{
			DIRECTION OppositeDir = OppositeDirection(m_RoamingDirection);
			NeedJumps = GetJumpsNeededToJumpOnPlatform(OppositeDir, MaxJumps, pTargetPosition, MaxHDistance);
		}

		bool Doable = NeedJumps > 0 && NeedJumps <= MaxJumps;
		BotDebugMessage(VERBOSE_STEPS, "Has danger tiles below, %s avoid", Doable ? "can" : "can't");

		return NeedJumps;
	}

	if(!IsThreatAware())
	{
		return 0;
	}

	constexpr int MaxJumpsToAvoid = 1;
	if (MaxJumps > MaxJumpsToAvoid)
	{
		MaxJumps = MaxJumpsToAvoid;
	}

	CIcEntity *pThreatEntity = nullptr;
	vec2 ThreatIntersectPos;
	const EThreatLevel ThreatLevel = GetDangerLevelAhead(&ThreatIntersectPos, &pThreatEntity);
	if(!pThreatEntity || ThreatLevel < CareAboutThreatLevel())
	{
		return 0;
	}

	int DirectionSign = m_RoamingDirection;

	const float VelX = m_pCharacter->Core()->m_Vel.x;
	const float VelY = IsGrounded() ? 0 : m_pCharacter->Core()->m_Vel.y;
	const vec2 Pos = m_pCharacter->GetPos();
	const vec2 ThreatPos = pThreatEntity->GetPos();
	const float ThreatRadius = pThreatEntity->GetProximityRadius();

	const float MaxTiles = (IsGrounded() ? m_BotUtils.GetGroundJumpTiles() : m_BotUtils.GetAirJumpTiles()) + 1;
	const float AirTilesAbovePos = m_BotUtils.GetAirTilesAbove(Pos, MaxTiles);
	std::optional<float> FirstAirAboveThreat = m_BotUtils.GetFirstAirAbovePosition(ThreatPos, MaxTiles);
	if(!FirstAirAboveThreat.has_value())
	{
		return 0;
	}

	const float AirControlAccel = m_NextTuningParams.m_AirControlAccel;
	const float AirControlSpeed = m_NextTuningParams.m_AirControlSpeed;
	const float Acceleration = AirControlAccel * DirectionSign;

	const int ApproxTicks = m_BotUtils.GetJumpTicksInAir(MaxJumps, IsGrounded(), AirTilesAbovePos * TileSize, VelY);
	const float PredictedXDistance = m_BotUtils.GetDistanceForVelocityAccelerationTicks(VelX, Acceleration, ApproxTicks, AirControlSpeed);
	const float PredictedXDistanceAbs = std::abs(PredictedXDistance);
	const float ReachablePosX = Pos.x + PredictedXDistance;

	// Fixing up the DirectionSign (we need this because the initial VelX can exceed 'acceleration * ticks')
	DirectionSign = PredictedXDistance > 0 ? 1 : -1;

	float Distance2 = distance2(ThreatPos, vec2(ReachablePosX, Pos.y));
	if(Distance2 < ThreatRadius * ThreatRadius)
	{
		// Can't avoid the danger
		return 0;
	}

	float Distance = 0;
	while (Distance < PredictedXDistanceAbs)
	{
		// There should be at least the same tiles above AirTilesAbovePos
		const float AirTilesAboveX = m_BotUtils.GetAirTilesAbove(Pos + vec2(Distance * DirectionSign, 0), MaxTiles);
		if(AirTilesAboveX < AirTilesAbovePos)
		{
			// Do not jump: going to hit the head otherwise
			return 0;
		}
		Distance += TileSizeF;
	}

	*pTargetPosition = vec2(ReachablePosX, FirstAirAboveThreat.value());

	return MaxJumps;
}

void CBotPlayer::PushDecision(EDecision Decision, std::optional<DIRECTION> OptDirection)
{
	DIRECTION Direction = OptDirection.has_value() ? OptDirection.value() : m_RoamingDirection;
	const vec2 &Pos = GetCharacter()->GetPos();
	const STilePosition Position = STilePosition::fromPosXY(Pos.x, Pos.y);

	const auto SameContextLambda = [Position, Direction](const SBotDecision &BotDecision) -> bool {
		if(BotDecision.Position != Position)
			return false;
		if(BotDecision.Direction != Direction)
			return false;

		return true;
	};
	SBotDecision *pSameContext = std::find_if(m_RecentDecisions.begin(), m_RecentDecisions.end(), SameContextLambda);

	if(pSameContext != m_RecentDecisions.end())
	{
		BotDebugMessage(VERBOSE_STEPS, "Overwrite decision at %d %d", Position.X, Position.Y);
		m_RecentDecisions.erase(pSameContext);
	}

	if(m_RecentDecisions.Size() == m_RecentDecisions.Capacity())
	{
		m_RecentDecisions.RemoveAt(0);
	}

	int Tick = Server()->Tick();

	SBotDecision BotDecision;
	BotDecision.Tick = Tick;
	BotDecision.Position = Position;
	BotDecision.Direction = Direction;
	BotDecision.Objection = m_RoamingObjection;
	BotDecision.Decision = Decision;
	m_RecentDecisions.Add(BotDecision);

	BotDebugMessage(VERBOSE_STEPS, "Add decision %s at %d %d", toString(Decision), Position.X, Position.Y);
}

EDecision CBotPlayer::GetPreviousDecision() const
{
	const vec2 &Pos = GetCharacter()->GetPos();
	const STilePosition Position = STilePosition::fromPos(Pos);

	for(const SBotDecision &BotDecision : m_RecentDecisions)
	{
		if(BotDecision.Position != Position)
			continue;

		if(BotDecision.Direction != m_RoamingDirection)
			continue;

		if(BotDecision.Objection != m_RoamingObjection)
			continue;

		return BotDecision.Decision;
	}

	return EDecision::Invalid;
}

void CBotPlayer::PushCheckedPosition(const STilePosition &ShortPos)
{
	if(!g_Config.m_InfBotCheckPos)
	{
		return;
	}
	const int Tick = Server()->Tick();
	ma_CheckPoints.Add({ShortPos, Tick});
	if(!IsHuman())
	{
		s_HiveMind.PushCheckedPosition(ShortPos, Tick);
	}
}

void CBotPlayer::PushIgnoredPosition(const vec2 &Pos)
{
	PushIgnoredPosition(STilePosition::fromPos(Pos));
}

void CBotPlayer::PushIgnoredPosition(const STilePosition &ShortPos)
{
	if(ma_IgnorePoints.Size() == ma_IgnorePoints.Capacity())
		ma_IgnorePoints.RemoveAt(0);

	ma_IgnorePoints.Add({ShortPos, Server()->Tick()});
}

CGameWorld *CBotPlayer::GameWorld() const
{
	return GameServer()->GameWorld();
}

void CBotPlayer::UpdateCharacterState()
{
	const int T = Server()->Tick();
	if(m_StateUpdateTick == T)
		return;

	m_StateUpdateTick = T;

	const CIcCharacter *pCharacter = GetCharacter();
	m_CachedGrounded = pCharacter && pCharacter->IsGrounded();

	m_WantedJumps = 0;
	m_AirJumps = 0;

	if(!ma_RecentFailedAttackTicks.IsEmpty())
	{
		const int TrackAttacksForTicks = Server()->TickSpeed() * 3.0f;
		if(T > ma_RecentFailedAttackTicks.First() + TrackAttacksForTicks)
		{
			ma_RecentFailedAttackTicks.RemoveAt(0);
		}
	}

	UpdatePOIState();
}

void CBotPlayer::UpdatePOIState()
{
	if(m_POIPos.has_value() && m_pCharacter)
	{
		constexpr int MaxSteps = 100;
		m_CachedPOIReachableByGround = m_BotUtils.IsReachableByGround(m_pCharacter->GetPos(), m_POIPos.value(), GetMaxJumps(), MaxSteps);
	}
	else
	{
		m_CachedPOIReachableByGround = false;
	}
}

CBotPlayer::DIRECTION CBotPlayer::DoLandingManeuves() const
{
	const vec2 &Pos = GetCharacter()->GetPos();
	const float VelX = m_pCharacter->Core()->m_Vel.x;
	const vec2 VectorToTarget = m_JumpTargetPosition - Pos;
	float AbsXToJumpTarget = fabs(VectorToTarget.x);
	float AbsYToJumpTarget = fabs(VectorToTarget.y);

	const float ProximityRadius = m_pCharacter->GetProximityRadius();
	const bool GoingDown = VectorToTarget.y > 0;
	const float MaxOffset = GoingDown ? 2 : TileSize + ProximityRadius * 1.75f;
	if(AbsYToJumpTarget > MaxOffset)
	{
		const DIRECTION DirectionFromJumpToTarget = m_JumpTargetPosition.x >= m_JumpFromPosition.x ? DIRECTION_RIGHT : DIRECTION_LEFT;
		const float WantedXPos = GoingDown ? m_JumpTargetPosition.x : (m_JumpTargetPosition.x - TileSize * 0.5f * DirectionFromJumpToTarget);
		float SecondWantedXPos = WantedXPos;

		if(GoingDown)
		{
			vec2 CurrentTargetTile = m_JumpTargetPosition;
			float CurrentY = Pos.y;
			constexpr int MaxHorizontalTiles = 10;
			for(int i = 0; i < MaxHorizontalTiles; ++i)
			{
				float TryX = SecondWantedXPos + TileSize * DirectionFromJumpToTarget;
				if(m_BotUtils.GetCollision()->IsSolid(vec2(TryX, CurrentY)))
				{
					// Allow only one tile jump for now
					CurrentY -= TileSize;
					if(m_BotUtils.GetCollision()->IsSolid(vec2(TryX, CurrentY)))
					{
						break;
					}
				}

				vec2 NextTile(TryX, Pos.y);
				constexpr int MaxFault = 8;
				std::optional<float> Gnd = m_BotUtils.GetSolidBelow(NextTile, MaxFault);
				if(!Gnd.has_value())
				{
					break;
				}

				float GroundValue = Gnd.value() - ProximityRadius / 2;

				if(m_BotUtils.IsReachableByGround(CurrentTargetTile, vec2(NextTile.x, GroundValue), 1))
				{
					CurrentTargetTile = vec2(NextTile.x, GroundValue);
					SecondWantedXPos = NextTile.x;
				}
				else
				{
					// GameController()->GameServer()->CreateLoveEvent(vec2(NextTile.x, Gnd));
					break;
				}
			}
		}

		float MaxWantedX = std::max<float>(WantedXPos, SecondWantedXPos);
		float MinWantedX = std::min<float>(WantedXPos, SecondWantedXPos);

		if(!GoingDown)
		{
			const float MaxDiff = 1; // (TileSize - ProximityRadius) * 0.375f;
			if(DirectionFromJumpToTarget == DIRECTION_RIGHT)
			{
				MinWantedX -= MaxDiff;
			}
			else
			{
				MaxWantedX += MaxDiff;
			}
		}

		m_BotUtils.GetDebugSink()->HighlightLineSegment(vec2(MinWantedX, Pos.y), vec2(MinWantedX, m_JumpTargetPosition.y));
		if(MinWantedX != MaxWantedX)
		{
			m_BotUtils.GetDebugSink()->HighlightLineSegment(vec2(MaxWantedX, Pos.y), vec2(MaxWantedX, m_JumpTargetPosition.y));
		}

		if(AbsXToJumpTarget > TileSize * 4)
		{
			// Rought math
			if(Pos.x > MaxWantedX)
			{
				return DIRECTION_LEFT;
			}
			else if(Pos.x < MinWantedX)
			{
				return DIRECTION_RIGHT;
			}
			else
			{
				if(GoingDown)
				{
					return DirectionFromJumpToTarget;
				}
				else
				{
					return DIRECTION_NONE;
				}
			}
		}
		else
		{
			const float AirControlAccel = m_NextTuningParams.m_AirControlAccel;
			const float AirControlSpeed = m_NextTuningParams.m_AirControlSpeed;
			int ActualDirection = VelX > 0 ? 1 : -1;
			const float Acceleration = AirControlAccel * ActualDirection;

			int MaxTicks = 500;
			const int ApproxTicks = m_BotUtils.GetTicksToMoveDistance(VelX, AirControlAccel, VectorToTarget.x, MaxTicks, AirControlSpeed);
			const float PredictedXDistance = m_BotUtils.GetDistanceForVelocityAccelerationTicks(VelX, Acceleration, ApproxTicks, AirControlSpeed);

			if(Pos.x + PredictedXDistance > MaxWantedX)
			{
				return DIRECTION_LEFT;
			}
			else if(Pos.x + PredictedXDistance < MinWantedX)
			{
				return DIRECTION_RIGHT;
			}
			else
			{
				if(GoingDown)
				{
					return DirectionFromJumpToTarget;
				}
				else
				{
					return DIRECTION_NONE;
				}
			}
		}
	}

	return m_RoamingDirection;
}

bool CBotPlayer::CanHook() const
{
	if(IsHuman())
		return false;

	if(GetClass() == EPlayerClass::Tank)
		return false;

	if(m_aTweaks.Contains(EBotTweak::NoHook))
		return false;

	int Tick = Server()->Tick();
	constexpr float NoHookInterval = 2.0f;
	if(Tick < m_RespawnTick + Server()->TickSpeed() * NoHookInterval)
		return false;

	return true;
}

bool CBotPlayer::WeakHook() const
{
	return m_aTweaks.Contains(EBotTweak::WeakHook);
}

bool CBotPlayer::StrongHook() const
{
	return m_aTweaks.Contains(EBotTweak::StrongHook);
}

bool CBotPlayer::IsThreatAware() const
{
	return m_aTweaks.Contains(EBotTweak::ThreatAware);
}

bool CBotPlayer::CanFlee() const
{
	return m_aTweaks.Contains(EBotTweak::CanFlee);
}

float CBotPlayer::GetMaxHookDistance() const
{
	if(!CanHook())
		return 0;

	float Distance = GameServer()->Tuning()->m_HookLength;

	if(!aHookyClasses.Contains(GetClass()))
		Distance *= 0.9f;

	if(WeakHook())
		Distance *= 0.9f;

	return Distance;
}

EThreatLevel CBotPlayer::CareAboutThreatLevel() const
{
	if(GetClass() == EPlayerClass::Witch)
	{
		return EThreatLevel::Suspicious;
	}
	if(GetClass() == EPlayerClass::Spitter)
	{
		return EThreatLevel::Suspicious;
	}

	return EThreatLevel::Dangerous;
}

float CBotPlayer::GetLookupRadius() const
{
	if(GetClass() == EPlayerClass::Spitter)
		return 800.0f;

	return 600.0f;
}

float CBotPlayer::GetLookupOffset() const
{
	return 300.0f;
}

STilePosition CBotPlayer::JumpPosToShortPos(const vec2 &JumpTarget, const vec2 &JumpFromPosition) const
{
	if(JumpTarget.x > JumpFromPosition.x)
	{
		return STilePosition::fromPosXY(JumpTarget.x + 16, JumpTarget.y);
	}
	else
	{
		return STilePosition::fromPosXY(JumpTarget.x - 16, JumpTarget.y);
	}
}

void CBotPlayer::SetJumpTargetPosition(const vec2 &JumpTarget, const vec2 &JumpFromPosition)
{
	// JumpTarget should be the edge of the tile (x % 32 == 0)
	m_JumpTargetPosition = JumpTarget;
	m_JumpFromPosition = JumpFromPosition;

	m_BotUtils.GetDebugSink()->HighlightPosition(JumpTarget);
}

void CBotPlayer::SetPOI(std::optional<vec2> newPOI)
{
	if(m_POIPos == newPOI)
		return;

	m_POIPos = newPOI;
	UpdatePOIState();
}
