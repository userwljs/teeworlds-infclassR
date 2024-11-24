#include "snap_filter.h"

#include "engine/server.h"
#include "engine/shared/config.h"
#include "game/server/gamecontext.h"
#include "game/server/infclass/entities/infccharacter.h"
#include "game/server/infclass/entities/infcentity.h"
#include "game/server/player.h"

bool SnapFiltersPassed(const CInfCEntity *pEntity, int SnappingClient, FilterFlags Flags)
{
	const int Owner = pEntity->GetOwner();
	if(Owner < 0 || SnappingClient == Owner)
		return true;

	if(SnappingClient == SERVER_DEMO_CLIENT)
		return Flags & EFilterFlag::Demo;

	const CPlayer *pSnappingPlayer = pEntity->GameServer()->GetPlayer(SnappingClient);
	if (!pSnappingPlayer)
	{
		// Assert?
		return false;
	}
	if(pSnappingPlayer->GetTeam() == TEAM_SPECTATORS)
	{
		if ((Flags & EFilterFlag::Restricted) && g_Config.m_SvStrictSpectateMode)
		{
			return false;
		}

		if(Flags & EFilterFlag::AnySpec)
		{
			return true;
		}
		if(Flags & EFilterFlag::Follower)
		{
			if(pSnappingPlayer->m_SpectatorId == Owner)
			{
				return true;
			}
		}
		if(Flags & EFilterFlag::FreeSpec)
		{
			if(pSnappingPlayer->m_SpectatorId == -1)
			{
				return true;
			}
		}

		// Return false for spectators following someone else
		return false;
	}

	if(Flags & EFilterFlag::SameTeam)
	{
		const CInfClassCharacter *pOwner = pEntity->GetOwnerCharacter();
		if(pOwner && (pOwner->IsInfected() == pSnappingPlayer->IsInfected()))
		{
			return true;
		}
	}

	return false;
}
