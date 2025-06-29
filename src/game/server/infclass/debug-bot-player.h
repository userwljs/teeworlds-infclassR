#ifndef INFCLASS_DEBUG_PLAYER_H
#define INFCLASS_DEBUG_PLAYER_H

#include "bot-player.h"

class CDebugPlayer : public CBotPlayer
{
	MACRO_ALLOC_POOL_ID()
public:
	CDebugPlayer(CIcGameController *pGameController, int UniqueClientId, int ClientID, int Team);

	void Snap(int SnappingClient) override;

	void TryRespawn() override;
	bool IsBot() const override { return false; }
	void Tick() override;

	void OnCharacterSpawned(const SpawnContext &Context) override;
	void OnCharacterDamage(const SDamageContext &Context) override;

	void UpdateName() override;
};

#endif // INFCLASS_DEBUG_PLAYER_H
