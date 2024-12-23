/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_TURRET_H
#define GAME_SERVER_ENTITIES_TURRET_H

#include <game/server/infclass/entities/ic_placed_object.h>

class CTurret : public CPlacedObject
{
public:
	static int EntityId;

	enum Type
	{
		LASER,
		PLASMA,
	};

	CTurret(CGameContext *pGameContext, vec2 Pos, int Owner, Type Type);
	~CTurret() override;

	bool IsDestructable() const { return m_Destructable; }
	void SetDestructable(bool Destructable);

	void Tick() override;
	void Snap(int SnappingClient) override;
	float HitRadius() const { return 4.0f; }

	void Hit(CIcCharacter *pCharacter);
	void Die(CIcCharacter *pKiller);

	float GetReloadDuration() const { return m_ReloadDuration; }
	void SetReloadDuration(float Seconds);

	int GetDamage() const;
	void SetDamage(int Damage);

protected:
	void AttackTargets();
	void Reload();

private:
	int m_StartTick;
	int m_Radius;
	Type m_Type;
	int m_SnapLaserType = -1;
	const float m_RadiusGrowthRate = 4.0f;
	float m_ReloadDuration{};
	int m_WarmUpCounter;
	int m_ReloadCounter;
	int m_ammunition;
	std::optional<int> m_Damage;
	bool m_foundTarget;
	bool m_Destructable{};

	int m_Ids[8];
};

#endif
