#ifndef ENGINE_SERVER_ROUND_STATISTICS_H
#define ENGINE_SERVER_ROUND_STATISTICS_H

#include <engine/console.h>
#include <engine/shared/protocol.h>

#include <game/infclass/classes.h>

enum
{
	SCOREEVENT_HUMAN_SURVIVE=0,
	SCOREEVENT_HUMAN_SUICIDE,
	SCOREEVENT_HUMAN_HEALING,
	SCOREEVENT_INFECTION,
	SCOREEVENT_KILL_INFECTED,
	SCOREEVENT_KILL_WITCH,
	SCOREEVENT_KILL_UNDEAD,
	SCOREEVENT_KILL_TARGET,
	SCOREEVENT_DESTROY_TURRET,
	SCOREEVENT_HELP_FREEZE,
	SCOREEVENT_HELP_HOOK_BARRIER,
	SCOREEVENT_HELP_HOOK_INFECTION,
	SCOREEVENT_HERO_FLAG,
	SCOREEVENT_BONUS,
	SCOREEVENT_MEDIC_REVIVE,
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
		int OnScoreEvent(int EventType, EPlayerClass Class);
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
	void OnScoreEvent(int ClientId, int EventType, EPlayerClass Class, const char* Name, IConsole* console);
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
