#include "debug-bot-player.h"

MACRO_ALLOC_POOL_ID_IMPL(CDebugPlayer, MAX_CLIENTS)

CDebugPlayer::CDebugPlayer(CIcGameController *pGameController, int UniqueClientId, int ClientID, int Team) :
	CBotPlayer(pGameController, UniqueClientId, ClientID, Team)
{
	m_IsInGame = false;
	m_IsReady = false;
}

void CDebugPlayer::Snap(int SnappingClient)
{
	CIcPlayer::Snap(SnappingClient);
}

void CDebugPlayer::TryRespawn()
{
	CIcPlayer::TryRespawn();
}

void CDebugPlayer::Tick()
{
	CIcPlayer::Tick();
}

void CDebugPlayer::OnCharacterSpawned(const SpawnContext &Context)
{
	CIcPlayer::OnCharacterSpawned(Context);
}

void CDebugPlayer::UpdateName()
{
	// Do nothing
}
