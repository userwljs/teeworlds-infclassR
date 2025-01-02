#pragma once

#include <game/server/infclass/bot-player.h>

#include <optional>

class CIcGameController;

class CHiveMind
{
public:
	CIcGameController *GameController() { return m_pGameController; }

	void Reset();
	void ResetDecisions();
	void UpdateTick(CIcGameController *pGameController, int Tick);
	void UpdateGroups();

	void ReportKilled(CBotPlayer *pPlayer);
	void ReportTargetFound(const CBotPlayer *pPlayer, const vec2 &TargetPos);

	bool TryJump(CBotPlayer *pPlayer, float SpeedRate);
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
	const icArray<vec2, MAX_CLIENTS> &GetInfectedPositions()
	{
		return m_aInfectedPositions;
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

	CIcGameController *m_pGameController{};
	int m_Tick = 0;
	HiveVictim m_aVictims[MAX_CLIENTS];
	icArray<vec2, 10> m_aPOIs;
	int m_HumansTick = 0;
	int m_PrevJumpTick = 0;

	struct GroupItem
	{
		const CIcCharacter *pCharacter{};
		icArray<const CIcCharacter*, MAX_CLIENTS> aNearbies;
	};

	icArray<GroupItem, MAX_CLIENTS> m_Groups;

	icArray<vec2, MAX_CLIENTS> m_aHumanPositions;
	icArray<vec2, MAX_CLIENTS> m_aInfectedPositions;
	icArray<int, MAX_CLIENTS> m_aInfectedBots;
	icArray<int, MAX_CLIENTS> m_aBotsProcessingQueue;
	icFifoArray<STilePosition, 320> m_aCheckedPos;
	icFifoArray<STilePosition, 32> m_aUncheckedPos;
	icFifoArray<SBotDecision, 8> m_aGoodDecisions;
};
