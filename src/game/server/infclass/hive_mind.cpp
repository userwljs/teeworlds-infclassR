#include "hive_mind.h"

#include <engine/shared/config.h>

#include <game/server/infclass/entities/control-point.h>
#include <game/server/infclass/entities/ic_character.h>
#include <game/server/infclass/ic_gamecontroller.h>

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
	ResetDecisions();
}

void CHiveMind::ResetDecisions()
{
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
		const CIcPlayer *pPlayer = pGameController->GetPlayer(i);
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
