#include "bot-player.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/server/infclass/classes/ic_playerclass.h>
#include <game/server/infclass/entities/ic_character.h>
#include <game/server/infclass/entities/ic_entity.h>
#include <game/server/infclass/ic_gamecontroller.h>

static constexpr int InfinityLives = -1;

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

void CBaseBotPlayer::SetRespawnInterval(float Interval)
{
	m_RespawnInterval = Interval;
}

MACRO_ALLOC_POOL_ID_IMPL(CBotPlayer, MAX_CLIENTS)

CBotPlayer::CBotPlayer(CIcGameController *pGameController, int UniqueClientId, int ClientID, int Team) :
	CBaseBotPlayer(pGameController, UniqueClientId, ClientID, Team)
{
	m_IsInGame = true;
	m_IsReady = true;
}
