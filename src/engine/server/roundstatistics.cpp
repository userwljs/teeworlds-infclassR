#include "roundstatistics.h"

#include <base/system.h>

int CRoundStatistics::CPlayerStats::OnScoreEvent(EScoreEvent EventType, EPlayerClass Class, std::optional<int> Param)
{
	if(Class == EPlayerClass::Invalid)
		return 0;

	int Points = 0;
	switch(EventType)
	{
	case EScoreEvent::HUMAN_SURVIVE:
		Points = 50;
		break;
	case EScoreEvent::HUMAN_SUICIDE:
		Points = -10;
		break;
	case EScoreEvent::INFECTION:
		Points = 30;
		break;
	case EScoreEvent::KILL_INFECTED:
		Points = 10;
		break;
	case EScoreEvent::KILL_TARGET:
		Points = 20;
		break;
	case EScoreEvent::KILL_WITCH:
		Points = 50;
		break;
	case EScoreEvent::KILL_UNDEAD:
		Points = 50;
		break;
	case EScoreEvent::DESTROY_TURRET:
		Points = 10;
		break;
	case EScoreEvent::HELP_FREEZE:
		Points = 10;
		break;
	case EScoreEvent::HELP_HOOK_BARRIER:
		Points = 10;
		break;
	case EScoreEvent::HELP_HOOK_INFECTION:
		Points = 10;
		break;
	case EScoreEvent::HUMAN_HEALING:
		Points = 10;
		break;
	case EScoreEvent::HERO_FLAG:
		Points = 10;
		break;
	case EScoreEvent::BONUS:
		Points = 50;
		break;
	case EScoreEvent::MEDIC_REVIVE:
		Points = 50;
		break;
	}

	m_Score += Points;

	std::size_t ClassIndex = static_cast<std::size_t>(Class);
	if(ClassIndex > 0 && ClassIndex < NB_PLAYERCLASS)
	{
		m_ClassScore[ClassIndex] += Points;
	}

	return Points;
}

void CRoundStatistics::ResetPlayer(int ClientId)
{
	if(ClientId >= 0 && ClientId < MAX_CLIENTS)
		m_aPlayers[ClientId].Reset();
}

void CRoundStatistics::OnScoreEvent(int ClientId, EScoreEvent EventType, EPlayerClass Class, const char* Name, IConsole* console, std::optional<int> Param1)
{
	if(ClientId >= 0 && ClientId < MAX_CLIENTS) {
		int Score = m_aPlayers[ClientId].OnScoreEvent(EventType, Class, Param1);

		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "score player='%s' amount='%d'",
			Name,
			Score);
		console->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}
	
}

void CRoundStatistics::SetPlayerAsWinner(int ClientId)
{
	if(ClientId >= 0 && ClientId < MAX_CLIENTS)
		m_aPlayers[ClientId].m_Won = true;
}

CRoundStatistics::CPlayerStats *CRoundStatistics::PlayerStatistics(int ClientId)
{
	if(ClientId >= 0 && ClientId < MAX_CLIENTS)
		return &m_aPlayers[ClientId];
	else return nullptr;
}


int CRoundStatistics::PlayerScore(int ClientId)
{
	if(ClientId >= 0 && ClientId < MAX_CLIENTS)
		return m_aPlayers[ClientId].m_Score/10;
	else return 0;
}

void CRoundStatistics::SetPlayerScore(int ClientId, int Score)
{
	if(ClientId >= 0 && ClientId < MAX_CLIENTS)
	{
		m_aPlayers[ClientId].m_Score = Score * 10;
	}
}

int CRoundStatistics::NumWinners() const
{
	int NumWinner = 0;
	for(int i=0; i<MAX_CLIENTS; i++)
	{
		if(m_aPlayers[i].m_Won)
			NumWinner++;
	}
	return NumWinner;
}

void CRoundStatistics::UpdatePlayer(int ClientId, bool IsSpectator)
{
	if(ClientId >= 0 && ClientId < MAX_CLIENTS)
		m_aPlayers[ClientId].m_WasSpectator = IsSpectator || m_aPlayers[ClientId].m_WasSpectator;
}
	
void CRoundStatistics::UpdateNumberOfPlayers(int Num)
{
	if(m_NumPlayersMin > Num)
		m_NumPlayersMin = Num;
	
	if(m_NumPlayersMax < Num)
		m_NumPlayersMax = Num;
	
	if(Num > 1)
		m_PlayedTicks++;
}
	
bool CRoundStatistics::IsValidePlayer(int ClientId)
{
	if(ClientId >= 0 && ClientId < MAX_CLIENTS)
		return true;
	else
		return false;
}
