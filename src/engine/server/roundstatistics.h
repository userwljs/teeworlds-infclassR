#ifndef ENGINE_SERVER_ROUND_STATISTICS_H
#define ENGINE_SERVER_ROUND_STATISTICS_H

#include <engine/console.h>
#include <engine/shared/protocol.h>

#include <game/infclass/ic_classes.h>

enum class EScoreEvent
{
	HUMAN_SURVIVE,
	HUMAN_SUICIDE,
	HUMAN_HEALING,
	INFECTION,
	KILL_INFECTED,
	KILL_WITCH,
	KILL_UNDEAD,
	KILL_TARGET,
	DESTROY_TURRET,
	HELP_FREEZE,
	HELP_HOOK_BARRIER,
	HELP_HOOK_INFECTION,
	HERO_FLAG,
	BONUS,
	MEDIC_REVIVE,
};

class CRoundStatistics
{
public:
	class CPlayerStats
	{
	public:
		int m_Score{};
		int m_ClassScore[NB_PLAYERCLASS]{};

		bool m_WasSpectator{};
		bool m_Won{};

	public:
		CPlayerStats() = default;

		void Reset() { *this = CPlayerStats{}; }
		int OnScoreEvent(EScoreEvent EventType, EPlayerClass Class, std::optional<int> Param = std::nullopt);
	};

public:
	CPlayerStats m_aPlayers[MAX_CLIENTS];
	int m_NumPlayersMin{};
	int m_NumPlayersMax{};
	int m_PlayedTicks{};
	
public:
	CRoundStatistics() = default;
	void Reset() { *this = CRoundStatistics{}; }
	void ResetPlayer(int ClientId);
	void OnScoreEvent(int ClientId, EScoreEvent EventType, EPlayerClass Class, const char *pName, IConsole *console,
		std::optional<int> Param1 = std::nullopt);
	void SetPlayerAsWinner(int ClientId);
	
	CRoundStatistics::CPlayerStats* PlayerStatistics(int ClientId);
	int PlayerScore(int ClientId);
	void SetPlayerScore(int ClientId, int Score);
	
	int NumWinners() const;
	
	void UpdatePlayer(int ClientId, bool IsSpectator);
	void UpdateNumberOfPlayers(int Num);
	
	bool IsValidePlayer(int ClientId);
};

#endif
