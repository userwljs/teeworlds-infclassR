#include "bot-player.h"

#include <base/tl/ic_enum.h>
#include <base/tl/ic_fifo.h>
#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/server/infclass/bot_utils.h>
#include <game/server/infclass/classes/ic_playerclass.h>
#include <game/server/infclass/entities/biologist-mine.h>
#include <game/server/infclass/entities/engineer-wall.h>
#include <game/server/infclass/entities/ic_character.h>
#include <game/server/infclass/entities/ic_entity.h>
#include <game/server/infclass/entities/looper-wall.h>
#include <game/server/infclass/entities/merc-bomb.h>
#include <game/server/infclass/entities/scatter-grenade.h>
#include <game/server/infclass/entities/scientist-mine.h>
#include <game/server/infclass/ic_gamecontroller.h>

static constexpr int InfinityLives = -1;
static icFifoArray<STilePosition, 320> sa_CheckedPos;

static icFifoArray<SBotDecision, 8> sa_GoodDecisions;

class CHiveMind
{
public:
	void Reset();
	void UpdateTick(int Tick);

	bool TryAttack(int ClientID);
	bool TryHook(int ClientID);

protected:
	static constexpr int MaxAttacksInTimespan = 2;
	static constexpr float Timespan = 0.75f; // in seconds

	struct HiveVictim
	{
		int ClientID = -1;
		icArray <int, MaxAttacksInTimespan> aAttacks;
		int Hooks = 0;
	};

	HiveVictim *GetVictim(int ClientID);

	int m_Tick = 0;
	icArray<HiveVictim, MAX_CLIENTS> m_aVictims;
};

void CHiveMind::Reset()
{
	m_Tick = -1;
	m_aVictims.Clear();
}

void CHiveMind::UpdateTick(int Tick)
{
	if(Tick == m_Tick)
		return;

	m_Tick = Tick;
	for(int i = m_aVictims.Size() - 1; i >= 0; --i)
	{
		HiveVictim &Victim = m_aVictims[i];

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

		if(Victim.aAttacks.IsEmpty())
		{
			m_aVictims.RemoveAt(i);
			continue;
		}

		Victim.Hooks = 0;
	}
}

bool CHiveMind::TryAttack(int ClientID)
{
	HiveVictim *pVictim = GetVictim(ClientID);
	auto aAttacks = pVictim->aAttacks;
	if(aAttacks.Size() == aAttacks.Capacity())
		return false;

	pVictim->aAttacks.Add(m_Tick);
	return true;
}

bool CHiveMind::TryHook(int ClientID)
{
	HiveVictim *pVictim = GetVictim(ClientID);

	if(pVictim->Hooks >= g_Config.m_InfMaxHiveHooks)
	{
		return false;
	}

	pVictim->Hooks++;
	return true;
}

CHiveMind::HiveVictim *CHiveMind::GetVictim(int ClientID)
{
	for(HiveVictim &Victim : m_aVictims)
	{
		if(Victim.ClientID == ClientID)
		{
			return &Victim;
		}
	}

	m_aVictims.Add({});
	m_aVictims.Last().ClientID = ClientID;

	return &m_aVictims.Last();
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
	"invalid",
};

const char *toString(EObjection Objection)
{
	return toStringImpl(Objection, gs_aObjectionNames);
}

static const char *gs_aBotStateNames[] = {
	"roaming",
	"hunting",
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
	sa_CheckedPos.Clear();
	sa_GoodDecisions.Clear();
	s_HiveMind.Reset();
}

void CBotPlayer::SetBotUtils(CBotUtils *pUtils)
{
	m_pUtils = pUtils;
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

	s_HiveMind.UpdateTick(Server()->Tick());

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
		if(m_JumpTargetingTicks > 0)
		{
			m_JumpTargetingTicks--;
		}

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

		if(m_pCharacter)
		{
			if(m_pCharacter->IsAlive())
			{
				UpdateTarget();
			}
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
	m_LastJumpTick++;
	m_LastSeenTick++;
	m_LastFireTick++;
	if(m_NextRandomFireTick)
		m_NextRandomFireTick++;
	m_HookUntilTick++;
	m_DelayHookUntilTick++;

	for(auto &IgnoredPosition : ma_IgnorePoints)
	{
		IgnoredPosition.Tick++;
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

	// Lookup for humans
	ClientsArray Targets;

	const vec2 &Pos = pCharacter->GetPos();
	if(!pCharacter->IsBlind())
	{
		const float LookupRadius = 600;
		const float LookupOffset = 300;
		const vec2 LookupFromPos = Pos + pCharacter->GetDirection() * LookupOffset;
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			const CIcCharacter *pChar = GameController()->GetCharacter(i);
			if(pChar && pChar->IsHuman() && pChar->IsAlive())
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
		for(int CandidateId : Targets)
		{
			const CIcCharacter *pChar = GameController()->GetCharacter(CandidateId);
			vec2 Out;
			vec2 Before;

			if(GameServer()->Collision()->IntersectLine(Pos, pChar->GetPos(), &Out, &Before))
			{
				continue;
			}

			TargetId = CandidateId;
			break;
		}
	}

	if(TargetId < 0)
	{
		if(m_BotState != EBotState::Roaming)
		{
			SetState(EBotState::Roaming);
			const CIcCharacter *pExTarget = GameController()->GetCharacter(m_LastTarget);
			EObjection SameDirObjection = EObjection::Invalid;
			{
				int SolidBelowTheTarget = m_pUtils->GetSolidBelow(m_LastTargetSeenAtPos);
				int SolidBelowTheBot = m_pUtils->GetSolidBelow(Pos);

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
			}
			else
			{
				SetObjection(SameDirObjection);
			}
		}
		return;
	}

	if(IsInfected() && (m_BotState == EBotState::Roaming))
	{
		int Tick = Server()->Tick();

		int MaxCheckpoints = std::max<int>(m_RecentDecisions.Size(), 10);
		for(int i = m_RecentDecisions.Size() - MaxCheckpoints; i < m_RecentDecisions.Size(); ++i)
		{
			sa_GoodDecisions.Add(m_RecentDecisions.At(i));
		}
		sa_GoodDecisions.Last().Tick = Tick;
	}

	m_LastTarget = TargetId;
	m_LastTargetSeenAtPos = GameController()->GetCharacter(TargetId)->GetPos();
	m_LastSeenTick = Server()->Tick();

	SetState(EBotState::Hunting);
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

	if(m_BotState == EBotState::Roaming)
	{
		UpdateControlsRoaming(&NewInput);
	}
	else if(m_BotState == EBotState::Hunting)
	{
		UpdateControlsHunting(&NewInput);
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

	if(NewInput.m_Fire)
	{
		m_LastFireTick = Tick;
	}

	ScheduleRandomFire();

	m_pCharacter->OnPredictedInput(&NewInput);
	OnDirectInput(&NewInput);
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
	const vec2 &Pos = GetCharacter()->GetPos();
	const float Radius = GetCharacter()->GetProximityRadius();

	const bool CanJump = GetCharacter()->CanJump();
	int MaxJumps = GetAvailableJumps();

	bool WantToJump = false;
	int TileX = Pos.x / TileSize;
	int TileY = Pos.y / TileSize;

	const bool HasWallInRoamingDirection = HasWallInTheDirection(m_RoamingDirection);
	bool HasDangerInRoamingHorizontalDirection = false;

	if(!HasWallInRoamingDirection)
	{
		HasDangerInRoamingHorizontalDirection = HasDangerInTheDirection(m_RoamingDirection);
	}

	int KeepMoving = 1;
	const int DirectionSign = m_RoamingDirection;
	const float VelX = m_pCharacter->Core()->m_Vel.x;

	if(IsGrounded())
	{
		m_WantedJumps = 0;
		m_AirJumps = 0;

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
			if(m_DecisionTileX != TileX)
			{
				vec2 JumpTarget;
				const int WantJumps = GetJumpsNeededToJumpOnPlatform(m_RoamingDirection, MaxJumps, &JumpTarget);
				if(WantJumps)
				{
					if(MaybeJumpOn(JumpTarget))
					{
						SetJumpTargetPosition(JumpTarget, Pos);
						BotDebugMessage(VERBOSE_STEPS, "Decided to 'jump on' to %.2fx%.2f", m_JumpTargetPosition.x / TileSizeF, m_JumpTargetPosition.y / TileSizeF);
						WantToJump = true;
						m_WantedJumps = WantJumps;

						PushCheckedPosition(m_JumpTargetPosition);
					}
					else
					{
						PushIgnoredPosition(JumpTarget);
						BotDebugMessage(VERBOSE_STEPS, "Ignore 'jump on' to %.2fx%.2f", m_JumpTargetPosition.x / TileSizeF, m_JumpTargetPosition.y / TileSizeF);
					}

					PushDecision(WantToJump ? EDecision::Jump : EDecision::NoJump);
				}
				else if(!IsSolidTile(Pos.x + (2 + Radius / 2) * m_RoamingDirection, Pos.y + Radius + 5))
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
				}

				if(!WantToJump)
				{
					if(m_RoamingObjection == EObjection::Jump)
					{
						int TilesAbove = GetAirTilesAbove(m_RoamingDirection, MaxJumps);
						if(TilesAbove > m_pUtils->GetGroundJumpTiles() && random_prob(m_JumpExtraProbability))
						{
							BotDebugMessage(VERBOSE_STEPS, "Jump just because!");
							WantToJump = true;
							m_JumpFromPosition = Pos;
							m_WantedJumps = std::max(MaxJumps, 3);
							m_JumpTargetPosition.x = std::numeric_limits<float>::quiet_NaN();
						}
					}
				}

				m_DecisionTileX = TileX;
			}
		}

		if(!WantToJump)
		{
			vec2 JumpTarget;
			int JumpsToAvoidDanger = GetJumpsToAvoidDanger(&JumpTarget);
			if(JumpsToAvoidDanger)
			{
				WantToJump = true;

				m_WantedJumps = JumpsToAvoidDanger;
				SetJumpTargetPosition(JumpTarget, Pos);
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
				pInput->m_Direction = KeepMoving = -1;
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
			if((m_WantedJumps <= 0) && (m_DecisionTileX != TileX) && (m_pCharacter->Core()->m_Vel.y > -3))
			{
				BotDebugMessage(VERBOSE_TRACE1, "Considering jump from the air");
				int MaybeWantJumps = 0;
				if(HasWallInRoamingDirection)
				{
					if((MaybeWantJumps = GetJumpsNeededToGetOverWall(m_RoamingDirection, MaxJumps, &JumpTarget)))
					{
						if(m_pCharacter->Core()->m_Vel.y < 0)
						{
							// We're moving up
							if(MaybeJumpOverWall(JumpTarget))
							{
								SetJumpTargetPosition(JumpTarget, Pos);
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
				else if((MaybeWantJumps = GetJumpsNeededToJumpOnPlatform(m_RoamingDirection, MaxJumps, &JumpTarget)))
				{
					if(MaybeJumpOn(JumpTarget))
					{
						SetJumpTargetPosition(JumpTarget, Pos);
						BotDebugMessage(VERBOSE_STEPS, "Jump on from the air");
						m_WantedJumps = MaybeWantJumps;

						PushCheckedPosition(m_JumpTargetPosition);
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

				m_DecisionTileX = TileX;
			}

			if(m_WantedJumps > 0)
			{
				WantToJump = true;
				++m_AirJumps;
			}

			if(!WantToJump)
			{
				int JumpsToAvoidDanger = GetJumpsToAvoidDanger(&m_JumpTargetPosition);
				if(JumpsToAvoidDanger)
				{
					WantToJump = true;

					m_WantedJumps = JumpsToAvoidDanger;
					m_JumpFromPosition = Pos;
				}
			}
		}
	}

	const vec2 VectorToTarget = m_JumpTargetPosition - Pos;
	float AbsXToJumpTarget = fabs(VectorToTarget.x);
	float AbsYToJumpTarget = fabs(VectorToTarget.y);


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
		const float ProximityRadius = m_pCharacter->GetProximityRadius();
		const bool GoingDown = VectorToTarget.y > 0;
		const float MaxOffset = GoingDown ? 2 : TileSize + ProximityRadius * 1.75f;
		if(AbsYToJumpTarget > MaxOffset)
		{
			const int DirectionFromJumpToTarget = m_JumpTargetPosition.x >= m_JumpFromPosition.x ? DIRECTION_RIGHT : DIRECTION_LEFT;
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
					if(m_pUtils->GetCollision()->IsSolid(vec2(TryX, CurrentY)))
					{
						// Allow only one tile jump for now
						CurrentY -= TileSize;
						if(m_pUtils->GetCollision()->IsSolid(vec2(TryX, CurrentY)))
						{
							break;
						}
					}

					vec2 NextTile(TryX, Pos.y);
					constexpr int MaxFault = 5;
					float Gnd = m_pUtils->GetSolidBelow(NextTile, MaxFault);
					if(Gnd >= Pos.y + TileSize * MaxFault)
					{
						break;
					}
					else
					{
						Gnd -= ProximityRadius / 2;

						if(m_pUtils->IsReachableByGround(CurrentTargetTile, vec2(NextTile.x, Gnd), 1))
						{
							CurrentTargetTile = vec2(NextTile.x, Gnd);
							m_pUtils->GetDebugSink()->HighlightLineSegment(Pos, CurrentTargetTile);
							SecondWantedXPos = NextTile.x;
						}
						else
						{
							// GameController()->GameServer()->CreateLoveEvent(vec2(NextTile.x, Gnd));
							break;
						}
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

			if(AbsXToJumpTarget > TileSize * 4)
			{
				// Rought math
				if(Pos.x > MaxWantedX)
				{
					pInput->m_Direction = DIRECTION_LEFT;
				}
				else if(Pos.x < MinWantedX)
				{
					pInput->m_Direction = DIRECTION_RIGHT;
				}
				else
				{
					if(GoingDown)
					{
						pInput->m_Direction = DirectionFromJumpToTarget;
					}
					else
					{
						pInput->m_Direction = DIRECTION_NONE;
					}
				}
			}
			else
			{
				const float AirControlAccel = m_NextTuningParams.m_AirControlAccel;
				const float AirControlSpeed = m_NextTuningParams.m_AirControlSpeed;
				const float Acceleration = AirControlAccel * DirectionSign;

				const int ApproxTicks = AbsXToJumpTarget / VelX;
				const float PredictedXDistance = m_pUtils->GetDistanceForVelocityAccelerationTicks(VelX, Acceleration, ApproxTicks, AirControlSpeed);

				if(Pos.x + PredictedXDistance > MaxWantedX)
				{
					pInput->m_Direction = DIRECTION_LEFT;
				}
				else if(Pos.x + PredictedXDistance < MinWantedX)
				{
					pInput->m_Direction = DIRECTION_RIGHT;
				}
				else
				{
					if(GoingDown)
					{
						pInput->m_Direction = DirectionFromJumpToTarget;
					}
					else
					{
						pInput->m_Direction = DIRECTION_NONE;
					}
				}

				if(!GoingDown && pInput->m_Direction)
				{
					if(ApproxTicks > Server()->TickSpeed() * 0.75f)
					{
						pInput->m_Direction = DIRECTION_NONE;
					}
				}
			}
		}
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
			if(dYTiles < m_pUtils->GetAirJumpTiles())
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

		if(Jump)
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
	const vec2 &Pos = GetCharacter()->GetPos();
	int TileX = Pos.x / TileSize;

	const float Gravity = m_NextTuningParams.m_Gravity;
	const vec2 VectorToTarget = m_LastTargetSeenAtPos - Pos;
	const float AbsXToTarget = fabs(VectorToTarget.x);
	const bool FallingDown = m_pCharacter->Core()->m_Vel.y > -Gravity;
	const int AvailableJumps = GetAvailableJumps();

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
				m_WantedJumps = std::min<int>(AvailableJumps, NeedJumps);
				SetJumpTargetPosition(m_LastTargetSeenAtPos, Pos);
			}
		}

		WantToJump = m_WantedJumps > 0;
	}
	else if(Pos.y < m_LastTargetSeenAtPos.y)
	{
		WantGoDown = true;
	}

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

	if((AbsXToTarget < TileSize * 3) && WantGoDown)
	{
		int YDiff = (m_LastTargetSeenAtPos.y - Pos.y) / TileSizeF;
		float AirTilesAboveTarget = m_pUtils->GetAirTilesAbove(m_LastTargetSeenAtPos, YDiff + 1);
		if(YDiff > AirTilesAboveTarget)
		{
			// float GroundLevelBelow = m_pUtils->GetSolidBelow(Pos, YDiff);
			Direction = OppositeDirection(Direction);
		}
	}

	if(Direction != DIRECTION_NONE)
	{
		bool HasWall = HasWallInTheDirection(Direction);

		BotDebugMessage(VERBOSE_TRACE1, HasWall ? "HasWall" : "HasNoWall");

		if(WantToJump && HasWall)
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

		if(!WantToJump)
		{
			bool HasDangerInRoamingHorizontalDirection = false;

			HasDangerInRoamingHorizontalDirection = HasDangerInTheDirection(m_RoamingDirection);
			if(HasDangerInRoamingHorizontalDirection)
			{
				WantToJump = true;
			}
		}
	}

	const float Distance = distance(Pos, m_LastTargetSeenAtPos);
	const float GroundControlSpeed = GameServer()->Tuning()->m_GroundControlSpeed;

	bool ConsiderHookingOut = false;

	if(CanHook() && !WeakHook() && (Distance < GetMaxHookDistance() - TileSize * 1))
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
			if(dYTiles < m_pUtils->GetAirJumpTiles())
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

		if(Jump)
		{
			pInput->m_Jump = 1;
			m_LastJumpTick = Tick;
			m_WantedJumps--;
		}
	}

	SetRoamingDirection(Direction);

	BotDebugMessage(VERBOSE_TRACE1, WantToJump ? "WantToJump: yes" : "WantToJump: no");

	pInput->m_Direction = m_RoamingDirection;
	pInput->m_TargetX = m_LastTargetSeenAtPos.x - Pos.x;
	pInput->m_TargetY = m_LastTargetSeenAtPos.y - Pos.y;

	float FirePerSecond = 0.3f;
	// TODO: This should be the target proximity radius
	const float HitDistance = GetCharacterClass()->GetHammerProjOffset() + GetCharacterClass()->GetHammerRange() + m_pCharacter->GetProximityRadius();

	if(Distance < HitDistance)
	{
		if(GetCharacter()->GetReloadTimer() <= 0)
		{
			pInput->m_Fire = true;
		}
	}
	else if(Distance < HitDistance * 2)
	{
		FirePerSecond += random_float() * 0.5f;

		if(Tick > m_LastFireTick + Server()->TickSpeed() * FirePerSecond)
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

		if(Tick > m_LastFireTick + Server()->TickSpeed() * FirePerSecond)
		{
			pInput->m_Fire = true;
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
		pInput->m_Hook = 1;
	}

	if(pInput->m_Fire)
	{
		pInput->m_Fire = s_HiveMind.TryAttack(m_LastTarget);
	}
	if(pInput->m_Hook)
	{
		pInput->m_Hook = s_HiveMind.TryHook(m_LastTarget);
	}

	if(m_NextRandomFireTick && Tick > m_NextRandomFireTick)
	{
		pInput->m_Fire = true;
		m_NextRandomFireTick = 0;
	}
}

void CBotPlayer::UpdateHumanBotControls()
{
	m_pCharacter->SetWeapon(WEAPON_HAMMER);
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

	m_NextRandomFireTick = m_LastFireTick + Server()->TickSpeed() * FireInterval;
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

	m_pUtils->GetDebugSink()->SendFormattedMessage(VerbosityLevel, aBuf);
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

	const icArray<int, 5> BadIndices = {
		ZONE_DAMAGE_DEATH,
	};

	if(BadIndices.Contains(DamageIndex1) || BadIndices.Contains(DamageIndex2))
		return true;

	return false;
}

bool CBotPlayer::HasDangerBelow() const
{
	const vec2 &Pos = m_pCharacter->GetPos();
	static const float ProximityRadius = m_pCharacter->GetProximityRadius();

	int DamageIndex1 = GameController()->GetDamageZoneValueAt(Pos + vec2(TileSize / 2, +ProximityRadius / 2 + TileSize * 0.9f));
	int DamageIndex2 = GameController()->GetDamageZoneValueAt(Pos + vec2(-TileSize / 2, +ProximityRadius / 2 + TileSize * 0.9f));

	const icArray<int, 5> BadIndices = {
		ZONE_DAMAGE_DEATH,
	};

	if(BadIndices.Contains(DamageIndex1) || BadIndices.Contains(DamageIndex2))
		return true;

	return false;
}

EThreatLevel CBotPlayer::GetDangerLevelAhead(vec2 *pThreatPosition, CIcEntity **ppThreatEntity) const
{
	if(!m_pCharacter)
	{
		return EThreatLevel::Zero;
	}

	const float PredictTime = 0.25f; // Predict 1/4 of the next second

	const vec2 &Pos = m_pCharacter->GetPos();
	const int DirectionSign = m_RoamingDirection;
	const float Acceleration = c_AirControlAccel * DirectionSign;
	const float MaxHDistance = CBotUtils::GetDistanceForVelocityAccelerationTicks(m_pCharacter->Core()->m_Vel.x, Acceleration, Server()->TickSpeed() * PredictTime, c_AirControlSpeed);
	const vec2 EndPos = Pos + vec2(MaxHDistance, 0);

	return GetDangerLevelOnLine(Pos, EndPos, pThreatPosition, ppThreatEntity);
}

EThreatLevel CBotPlayer::GetDangerLevelOnLine(const vec2 &From, vec2 To, vec2 *pThreatPosition, CIcEntity **ppThreatEntity) const
{
	if(IsHuman())
	{
		// Bot-human feels no fear
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
				// Typical mine damage is up to 18
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

	const vec2 &Pos = m_pCharacter->GetPos();
	static const float HalfProximityRadius = m_pCharacter->GetProximityRadius() / 2;
	const int DirectionSign = Direction;

	// float HVelocity = absolute(m_pCharacter->Core()->m_Vel.x);
	int MinHTile = 0; // Dep on velocity

	// const float MaxControlSpeed = IsGrounded() ? c_GroundControlSpeed : c_AirControlSpeed;
	// float MaxHSpeed = std::max<float>(MaxControlSpeed, fabs(m_pCharacter->Core()->m_Vel.x));
	const float AirControlAccel = m_NextTuningParams.m_AirControlAccel;
	const float AirControlSpeed = m_NextTuningParams.m_AirControlSpeed;
	const float Acceleration = AirControlAccel * DirectionSign;
	int Ticks = m_pUtils->GetJumpTicksInAir(MaxJumps, IsGrounded());
	const int MaxTicks = Server()->TickSpeed() * 1.5f;
	if(Ticks > MaxTicks)
		Ticks = MaxTicks;
	const float MaxHDistance = CBotUtils::GetDistanceForVelocityAccelerationTicks(m_pCharacter->Core()->m_Vel.x, Acceleration, Ticks, AirControlSpeed);
	int MaxHTiles = fabs(MaxHDistance / TileSize);
	int MaxTiles = GetMaxTilesForJumps(MaxJumps);

	const float CharHorOffset = DirectionSign * HalfProximityRadius;
	const float CharPosX = Pos.x + CharHorOffset;
	int NeedJumps = 0;

	if(IsDebugEnabled(VERBOSE_TRACE2))
	{
		BotDebugMessage(VERBOSE_TRACE2, "jump max H tiles: %.2f", MaxHDistance);
	}

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

		// horTile + 1 because we're looking for the next tile after current air
		const float WallPosX = CharPosX + (horTile + 1) * TileSize * DirectionSign;
		for(int i = MaxReachableTilesAbove; i > 0; --i)
		{
			float CheckPosY = Pos.y - i * TileSize;
			bool HasAirThere = !IsSolidTile(WallPosX, CheckPosY);

			if(IsDebugEnabled(VERBOSE_TRACE2))
			{
				BotDebugMessage(VERBOSE_TRACE2, "Check the wall at %.2f x %.2f: %s", WallPosX / TileSize, CheckPosY / TileSize, HasAirThere ? "air" : "solid");
			}

			if(HasAirThere)
			{
				bool HasSolidBelow = IsSolidTile(WallPosX, CheckPosY + TileSize);

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
	return m_pUtils->GetAirTilesAbove(Pos, MaxTiles);
}

float CBotPlayer::GetAirTilesAboveAtX(int MaxTiles, float CheckPosX) const
{
	const vec2 &Pos = m_pCharacter->GetPos();
	const float BaseY = Pos.y;

	return m_pUtils->GetAirTilesAbove(vec2(CheckPosX, BaseY), MaxTiles);
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

	if(NewState == EBotState::Roaming)
	{
		BotDebugMessage(VERBOSE_MAIN, "SwitchState: ROAMING");
	}
	else if(NewState == EBotState::Hunting)
	{
		BotDebugMessage(VERBOSE_MAIN, "SwitchState: HUNTING");

		static const int HuntingEmotes[] = {
			EMOTICON_SPLATTEE,
			EMOTICON_DEVILTEE,
			EMOTICON_ZOMG,
		};
		GameServer()->SendEmoticon(GetCid(), HuntingEmotes[random_int(0, 2)]);
	}

	if(IsHuman())
	{
		GameServer()->SendEmoticon(GetCid(), EMOTICON_HEARTS);
	}

	m_BotState = NewState;
}

void CBotPlayer::SetRoamingDirection(DIRECTION Direction)
{
	m_RoamingBehaviorTick = Server()->Tick();
	m_RoamingDirection = Direction;
}

void CBotPlayer::SetObjection(EObjection Objection)
{
	if(m_RoamingObjection == Objection)
		return;

	BotDebugMessage(VERBOSE_MAIN, "Objection: %s", toString(Objection));
	m_RoamingObjection = Objection;
}

void CBotPlayer::MaybeChangeRoamingBehavior()
{
	const float BehaviorChangeInterval = 30.0;
	const float LastSeenTrackingDuration = 5.0;
	if(m_RoamingObjection == EObjection::CheckTheLastSeen)
	{
		if(Server()->Tick() > m_LastSeenTick + Server()->TickSpeed() * LastSeenTrackingDuration)
		{
			SetObjection(m_TargetLastSeenDirObjection);
		}

		return;
	}
	if(Server()->Tick() > m_RoamingBehaviorTick + Server()->TickSpeed() * BehaviorChangeInterval)
	{
		if(random_prob(0.6f))
		{
			// Dirty delay
			m_RoamingBehaviorTick += Server()->TickSpeed() * (0.125f + random_float());
		}
		else
		{
			ChangeRoamingBehavior();
		}
	}
}

void CBotPlayer::ChangeRoamingBehavior()
{
	SetRoamingDirection(OppositeDirection(m_RoamingDirection));

	m_DecisionTileX = -1;
	m_DecisionTileY = -1;

	switch(m_RoamingObjection)
	{
	case EObjection::Relax:
	case EObjection::Jump:
	case EObjection::Lookup:
	case EObjection::CheckTheLastSeen:
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
	case EObjection::Lookup:
	case EObjection::CheckTheLastSeen:
	case EObjection::Invalid:
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

		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			const CIcPlayer *pPlayer = GameController()->GetPlayer(i);
			if(!pPlayer || !pPlayer->GetCharacter() || !pPlayer->GetCharacter()->IsAlive())
				continue;

			// Different team?
			if(pPlayer->IsHuman() != IsHuman())
			{
				vec2 TargetPos = pPlayer->GetCharacter()->GetPos();
				if(TargetPos.y > OwnPos.y)
				{
					PlayersBelow++;
				}
				else
				{
					bool Reachable = m_pUtils->IsReachableByGround(OwnPos, TargetPos, GetMaxJumps());

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
		}

		BotDebugMessage(VERBOSE_STEPS, "Looking up. PlayersAbove: %d, PlayersMid: %d, PlayersBelow: %d", PlayersAbove, PlayersMid, PlayersBelow);

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
		m_HookUntilTick = -1;
		return;
	}

	float HookDuration = GameServer()->Tuning()->m_HookDuration;
	float BaseDelay = 0.3f;
	float ExtraDelay = 0.75f;

	if(StrongHook())
	{
		BaseDelay *= 0.75f;
		ExtraDelay *= 0.75;
	}
	else if(WeakHook())
	{
		HookDuration *= 0.75f;
		BaseDelay *= 1.5f;
		ExtraDelay *= 1.5f;
	}

	if(aHookyClasses.Contains(GetClass()))
	{
		BaseDelay *= 0.66f;
		ExtraDelay *= 0.66f;

		if(m_pCharacter->Core()->HookedPlayer() == m_LastTarget)
		{
			// Keep hooking
			const int Tick = Server()->Tick();
			m_HookUntilTick = Tick + 1;
			// And delay the next hook
			m_DelayHookUntilTick = Tick + Server()->TickSpeed() * (BaseDelay + ExtraDelay);
			return;
		}
	}

	const int Tick = Server()->Tick();
	if(m_pCharacter->Core()->m_HookState == HOOK_GRABBED)
	{
		if(m_pCharacter->Core()->HookedPlayer() != m_LastTarget)
		{
			// Grabbed something else. Release.
			m_HookUntilTick = -1;
			m_DelayHookUntilTick = Tick + Server()->TickSpeed() * 0.25f;
		}
	}

	if(Distance < GetMaxHookDistance())
	{
		if(m_HookUntilTick <= Tick)
		{
			// If we're not hooking...
			if(m_DelayHookUntilTick < Tick)
			{
				// ... and we don't have a delay set
				// then delay
				m_DelayHookUntilTick = Tick + Server()->TickSpeed() * (BaseDelay + random_float() * ExtraDelay);
			}
			else if(m_DelayHookUntilTick == Tick)
			{
				// ... and we delayed the hook until this tick
				// then hook
				m_HookUntilTick = Tick + Server()->TickSpeed() * HookDuration;
			}
		}
	}
	else
	{
		m_DelayHookUntilTick = -1;
	}
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
	return m_pUtils->GetMaxTilesForJumps(Jumps, IsGrounded());
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
		return m_pUtils->IsReachableByGround(m_pCharacter->GetPos(), m_LastTargetSeenAtPos, GetMaxJumps());
	}

	{
		EDecision GoodDecision = GetGoodDecision();
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

bool CBotPlayer::MaybeJumpOn(const vec2 &JumpTargetPosition)
{
	if(m_RoamingObjection == EObjection::CheckTheLastSeen)
	{
		return m_LastTargetSeenAtPos.y < JumpTargetPosition.y + TileSizeF / 2;
	}

	STilePosition ShortPos = STilePosition::fromPos(JumpTargetPosition);
	for(auto &CheckPoint : ma_IgnorePoints)
	{
		if(CheckPoint.TilePos == ShortPos)
		{
			return false;
		}
	}

	bool HasPosition = false;
	for(const auto &CheckPoint : ma_CheckPoints)
	{
		if(CheckPoint.TilePos == ShortPos)
		{
			HasPosition = true;
			break;
		}
	}

	const bool Checked = HasPosition || sa_CheckedPos.Contains(ShortPos);
	constexpr float ChanceToIgnoreAlreadyCheckedPosition = 0.75f;
	constexpr float ChanceToCheckUncheckedPosition = 0.15f;

	if(Checked)
	{
		if(random_prob(ChanceToIgnoreAlreadyCheckedPosition))
		{
			return false;
		}
	}

	{
		EDecision GoodDecision = GetGoodDecision();
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
	if(HasDangerBelow())
	{
		if(HasWallInTheDirection(m_RoamingDirection))
		{
			int MaxJumps = GetAvailableJumps();
			return GetJumpsNeededToGetOverWall(m_RoamingDirection, MaxJumps, pTargetPosition);
		}

		if(pTargetPosition)
		{
			pTargetPosition->x = std::numeric_limits<float>::quiet_NaN();
		}

		return 1;
	}

	CIcEntity *pThreatEntity = nullptr;
	vec2 ThreatIntersectPos;
	const EThreatLevel ThreatLevel = GetDangerLevelAhead(&ThreatIntersectPos, &pThreatEntity);
	if(!pThreatEntity || ThreatLevel < EThreatLevel::Dangerous)
	{
		return 0;
	}

	const vec2 Pos = m_pCharacter->GetPos();
	const vec2 ThreatPos = pThreatEntity->GetPos();

	float MaxTiles = (IsGrounded() ? m_pUtils->GetGroundJumpTiles() : m_pUtils->GetAirJumpTiles()) + 1;
	float TopLineY = Pos.y - MaxTiles * TileSize + 5;
	std::optional<float> MaybeTheFirstSolidAbove = m_pUtils->GetFirstSolidAbovePosition(ThreatPos, MaxTiles);
	if(MaybeTheFirstSolidAbove.has_value() && MaybeTheFirstSolidAbove.value() > TopLineY)
	{
		TopLineY = MaybeTheFirstSolidAbove.value();
	}
	bool ExpectSolid = IsSolidTile(ThreatPos.x, TopLineY);

	// There should be no sky higher than that

	float CurrentX = ThreatIntersectPos.x; // + DirectionSign * TileSize
	float CheckToX = Pos.x;
	bool MakesSenseToJump = true;
	while(true)
	{
		bool Solid = IsSolidTile(CurrentX, TopLineY);

		if(!ExpectSolid)
		{
			float GroundY = m_pUtils->GetSolidBelow(vec2(CurrentX, TopLineY), MaxTiles + 1);
			if(GroundY < Pos.y)
			{
			}
		}
		// Check air at (CheckFromX, FirstSolidAbove + TileSize)
		if(!Solid)
		{
			MakesSenseToJump = false;
			break;
		}

		if(ThreatIntersectPos.x > Pos.x)
		{
		}
	}

	return MakesSenseToJump ? 1 : 0;
}

void CBotPlayer::PushDecision(EDecision Decision)
{
	const vec2 &Pos = GetCharacter()->GetPos();
	const STilePosition Position = STilePosition::fromPosXY(Pos.x, Pos.y);
	int Direction = m_RoamingDirection;

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

EDecision CBotPlayer::GetGoodDecision() const
{
	const vec2 &Pos = GetCharacter()->GetPos();
	const STilePosition Position = STilePosition::fromPos(Pos);
	for(int i = 0; i < sa_GoodDecisions.Size(); ++i)
	{
		const SBotDecision &BotDecision = sa_GoodDecisions.At(i);

		if(BotDecision.Position != Position)
			continue;

		if(BotDecision.Direction != m_RoamingDirection)
			continue;

		return BotDecision.Decision;
	}

	return EDecision::Invalid;
}

void CBotPlayer::PushCheckedPosition(const vec2 &Pos)
{
	if(ma_CheckPoints.Size() == ma_CheckPoints.Capacity())
		ma_CheckPoints.RemoveAt(0);

	STilePosition ShortPos = STilePosition::fromPos(Pos);
	ma_CheckPoints.Add({ShortPos, Server()->Tick()});
	sa_CheckedPos.Add(ShortPos);
}

void CBotPlayer::PushIgnoredPosition(const vec2 &Pos)
{
	if(ma_IgnorePoints.Size() == ma_IgnorePoints.Capacity())
		ma_IgnorePoints.RemoveAt(0);

	STilePosition ShortPos = STilePosition::fromPos(Pos);
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

void CBotPlayer::SetJumpTargetPosition(const vec2 &JumpTarget, const vec2 &JumpFromPosition)
{
	// JumpTarget should be the edge of the tile (x % 32 == 0)
	m_JumpTargetPosition = JumpTarget;
	m_JumpFromPosition = JumpFromPosition;

	m_pUtils->GetDebugSink()->HighlightPosition(JumpTarget);
}
