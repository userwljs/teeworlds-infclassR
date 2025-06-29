/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_IC_ENTITIES_PICKUP_H
#define GAME_SERVER_IC_ENTITIES_PICKUP_H

#include "ic_entity.h"

#include <base/tl/ic_array.h>

const int PickupPhysSize = 20;

enum class EICPickupType
{
	Invalid,
	Health,
	Armor,
	ClassUpgrade,
};

enum class EUpgradeType;
using PlayerUpgradesArray = icArray<EUpgradeType, 5>;

class CIcPickup : public CIcEntity
{
public:
	static int EntityId;

	CIcPickup(CGameContext *pGameContext, EICPickupType Type, vec2 Pos, int Owner = -1);

	void Reset() final;
	void Tick() override;
	void TickPaused() override;
	void Snap(int SnappingClient) override;

	void SnapAsFlag();

	void Spawn(float Delay = 0);
	void SetRespawnInterval(float Seconds);
	void SetUpgrades(const PlayerUpgradesArray &Upgrades);

private:
	void UpdateNetworkTypes();

	EICPickupType m_Type = EICPickupType::Invalid;
	int m_SpawnTick = 0;
	float m_SpawnInterval = -1;
	int m_NetworkType = 0;
	int m_NetworkSubtype{};
	PlayerUpgradesArray m_Upgrades;
};

#endif
