/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "teams.h"
#include "player.h"

#include <game/race_state.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontroller.h>
#include <game/server/player_race_data.h>

CGameTeams::CGameTeams(CGameContext *pGameContext) :
	m_pGameContext(pGameContext)
{
	Reset();
}

void CGameTeams::Reset()
{
	m_Core.Reset();
}

void CGameTeams::OnCharacterStart(int ClientId)
{
	int Tick = Server()->Tick();
	CCharacter *pStartingChar = GetCharacter(ClientId);
	if(!pStartingChar)
		return;
	{
		pStartingChar->SetRaceState(ERaceState::STARTED);
		pStartingChar->SetRaceStartTick(Tick);
	}
}

void CGameTeams::OnCharacterFinish(int ClientId)
{
	CPlayer *pPlayer = GetPlayer(ClientId);
	if(pPlayer && pPlayer->IsPlaying())
	{
		const CCharacter *pCharacter = pPlayer->GetCharacter();
		if (pCharacter->RaceState() != ERaceState::STARTED)
			return;

		int TimeTicks = Server()->Tick() - pCharacter->GetRaceStartTick();
		if(TimeTicks <= 0)
			return;
		char aTimestamp[TIMESTAMP_STR_LENGTH];
		str_timestamp_format(aTimestamp, sizeof(aTimestamp), FORMAT_SPACE); // 2019-04-02 19:41:58

		OnFinish(pPlayer, TimeTicks, aTimestamp);
	}
}

void CGameTeams::OnCharacterSpawn(int ClientId)
{
}

void CGameTeams::OnCharacterDeath(int ClientId, int Weapon)
{
}

const char *CGameTeams::SetCharacterTeam(int ClientId, int Team)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return "Invalid client ID";
	if(Team < 0 || Team >= MAX_CLIENTS + 1)
		return "Invalid team number";
	if(m_Core.Team(ClientId) == Team)
		return "You are in this team already";
	if(!GetCharacter(ClientId))
		return "Your character is not valid";

	SetForceCharacterTeam(ClientId, Team);
	return nullptr;
}

CClientMask CGameTeams::TeamMask(int Team, int ExceptId, int Asker)
{
	if(Team == TEAM_SUPER)
	{
		if(ExceptId == -1)
			return CClientMask().set();
		return CClientMask().set().reset(ExceptId);
	}

	CClientMask Mask;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(i == ExceptId)
			continue; // Explicitly excluded
		if(!GetPlayer(i))
			continue; // Player doesn't exist

		if(!(GetPlayer(i)->GetTeam() == TEAM_SPECTATORS || GetPlayer(i)->IsPaused()))
		{ // Not spectator
			if(i != Asker)
			{ // Actions of other players
				if(!GetCharacter(i))
					continue; // Player is currently dead
				if(GetPlayer(i)->m_ShowOthers == SHOW_OTHERS_ONLY_TEAM)
				{
					if(m_Core.Team(i) != Team && m_Core.Team(i) != TEAM_SUPER)
						continue; // In different teams
				}
				else if(GetPlayer(i)->m_ShowOthers == SHOW_OTHERS_OFF)
				{
					if(m_Core.GetSolo(Asker))
						continue; // When in solo part don't show others
					if(m_Core.GetSolo(i))
						continue; // When in solo part don't show others
					if(m_Core.Team(i) != Team && m_Core.Team(i) != TEAM_SUPER)
						continue; // In different teams
				}
			} // See everything of yourself
		}
		else if(GetPlayer(i)->m_SpectatorId != SPEC_FREEVIEW)
		{ // Spectating specific player
			if(GetPlayer(i)->m_SpectatorId != Asker)
			{ // Actions of other players
				if(!GetCharacter(GetPlayer(i)->m_SpectatorId))
					continue; // Player is currently dead
				if(GetPlayer(i)->m_ShowOthers == SHOW_OTHERS_ONLY_TEAM)
				{
					if(m_Core.Team(GetPlayer(i)->m_SpectatorId) != Team && m_Core.Team(GetPlayer(i)->m_SpectatorId) != TEAM_SUPER)
						continue; // In different teams
				}
				else if(GetPlayer(i)->m_ShowOthers == SHOW_OTHERS_OFF)
				{
					if(m_Core.GetSolo(Asker))
						continue; // When in solo part don't show others
					if(m_Core.GetSolo(GetPlayer(i)->m_SpectatorId))
						continue; // When in solo part don't show others
					if(m_Core.Team(GetPlayer(i)->m_SpectatorId) != Team && m_Core.Team(GetPlayer(i)->m_SpectatorId) != TEAM_SUPER)
						continue; // In different teams
				}
			} // See everything of player you're spectating
		}
		else
		{ // Freeview
			if(GetPlayer(i)->m_SpecTeam)
			{ // Show only players in own team when spectating
				if(m_Core.Team(i) != Team && m_Core.Team(i) != TEAM_SUPER)
					continue; // in different teams
			}
		}

		Mask.set(i);
	}
	return Mask;
}

int CGameTeams::Count(int Team) const
{
	if(Team == TEAM_SUPER)
		return -1;

	int Count = 0;

	for(int i = 0; i < MAX_CLIENTS; ++i)
		if(m_Core.Team(i) == Team)
			Count++;

	return Count;
}

void CGameTeams::SetForceCharacterTeam(int ClientId, int Team)
{
	m_Core.Team(ClientId, Team);
}

void CGameTeams::OnFinish(CPlayer *pPlayer, int TimeTicks, const char *pTimestamp)
{
	if(!pPlayer || !pPlayer->IsPlaying())
		return;

	float Time = TimeTicks / (float)Server()->TickSpeed();

	const int ClientId = pPlayer->GetCid();
	CPlayerRaceData Data;
	CPlayerRaceData *pData = GameServer()->m_pController->GetPlayerRaceData(ClientId);
	if(pData == nullptr)
		pData = &Data;

	char aBuf[128];
	// Note that the "finished in" message is parsed by the client
	str_format(aBuf, sizeof(aBuf),
		"%s finished in: %d minute(s) %5.2f second(s)",
		Server()->ClientName(ClientId), (int)Time / 60,
		Time - ((int)Time / 60 * 60));

	GameServer()->SendChatTarget(-1, aBuf);

	float Diff = absolute(Time - pData->m_BestTime);

	if(Time - pData->m_BestTime < 0)
	{
		// new record \o/
		pData->m_RecordStopTick = Server()->Tick() + Server()->TickSpeed();
		pData->m_RecordFinishTime = Time;

		if(Diff >= 60)
			str_format(aBuf, sizeof(aBuf), "New record: %d minute(s) %5.2f second(s) better.",
				(int)Diff / 60, Diff - ((int)Diff / 60 * 60));
		else
			str_format(aBuf, sizeof(aBuf), "New record: %5.2f second(s) better.",
				Diff);
		GameServer()->SendChatTarget(-1, aBuf);
	}
	else if(pData->m_BestTime != 0) // tee has already finished?
	{
		Server()->StopRecord(ClientId);

		if(Diff <= 0.005f)
		{
			GameServer()->SendChatTarget(ClientId, "You finished with your best time.");
		}
		else
		{
			if(Diff >= 60)
				str_format(aBuf, sizeof(aBuf), "%d minute(s) %5.2f second(s) worse, better luck next time.",
					(int)Diff / 60, Diff - ((int)Diff / 60 * 60));
			else
				str_format(aBuf, sizeof(aBuf),
					"%5.2f second(s) worse, better luck next time.",
					Diff);
			GameServer()->SendChatTarget(ClientId, aBuf); // this is private, sent only to the tee
		}
	}
	else
	{
		pData->m_RecordStopTick = Server()->Tick() + Server()->TickSpeed();
		pData->m_RecordFinishTime = Time;
	}

	GameServer()->SendFinish(ClientId, Time, pData->m_BestTime);

	bool NeedToSendNewPersonalRecord = false;
	if(!pData->m_BestTime || Time < pData->m_BestTime)
	{
		// update the score
		// pData->Set(Time, GetCurrentTimeCp(pPlayer));
		NeedToSendNewPersonalRecord = true;
	}

	bool NeedToSendNewServerRecord = false;
	// update server best time
	const float ServerBestTime = GameServer()->m_pController->ServerBestRaceTime();
	if((ServerBestTime == 0) || (Time < ServerBestTime))
	{
		GameServer()->m_pController->SetServerBestRaceTime(Time);
		NeedToSendNewServerRecord = true;
	}

	CCharacter *pCharacter = pPlayer->GetCharacter();
	pCharacter->SetRaceState(ERaceState::FINISHED);
	if(NeedToSendNewServerRecord)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetClientVersion() >= VERSION_DDRACE)
			{
				GameServer()->SendRecord(i);
			}
		}
	}
	if(!NeedToSendNewServerRecord && NeedToSendNewPersonalRecord && pPlayer->GetClientVersion() >= VERSION_DDRACE)
	{
		GameServer()->SendRecord(ClientId);
	}

	// Confetti
	m_pGameContext->CreateFinishEffect(pCharacter->m_Pos);
}
