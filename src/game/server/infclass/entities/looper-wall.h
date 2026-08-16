#ifndef GAME_SERVER_ENTITIES_LOOPER_WALL_H
#define GAME_SERVER_ENTITIES_LOOPER_WALL_H

#include <optional>

#include <game/server/infclass/entities/ic_placed_object.h>

class CLooperWall : public CPlacedObject
{
public:
	static int EntityId;

	static constexpr int NUM_PARTICLES = 18;

public:
	CLooperWall(CGameContext *pGameContext, vec2 Pos, int Owner);
	~CLooperWall() override;

	void Tick() override;
	void Snap(int SnappingClient) override;

private:
	void OnHitInfected(CIcCharacter *pCharacter);

	void PrepareSnapData();

	std::optional<int> m_Ids[2]{};
	std::optional<int> m_EndPointIds[2]{};
	std::optional<int> m_ParticleIds[NUM_PARTICLES]{};
	int m_SnapStartTick{};
};

#endif
