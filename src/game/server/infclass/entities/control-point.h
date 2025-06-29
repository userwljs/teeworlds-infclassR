#ifndef GAME_SERVER_ENTITIES_CONTROL_POINT_H
#define GAME_SERVER_ENTITIES_CONTROL_POINT_H

#include "ic_placed_object.h"

class CIcPlayer;

class CControlPoint : public CPlacedObject
{
public:
	static int EntityId;

	CControlPoint(CGameContext *pGameContext, const vec2 &Pos);
	~CControlPoint() override;

	void Snap(int SnappingClient) override;
	void Tick() override;
	void TickPaused() override;

	bool IsReady() const;
	bool IsTaken() const;
	bool IsBlocked() const;
	bool IsInfected() const;

	int GetReadyTick();
	void SetNextEffectTime(float Seconds);

protected:
	static constexpr int NUM_HINT = 12;
	static constexpr int NUM_SIDE = 12;
	static constexpr int NUM_IDS = NUM_HINT + NUM_SIDE;

	enum class PortalType
	{
		Disconnected,
		In,
		Out,
	};

	void StartParallelsVisualEffect();
	void StartMeridiansVisualEffect();
	void MoveParallelsParticles();
	void MoveMeridiansParticles();
	float CalcSpeedMultiplier();
	void PrepareAntipingParticles(vec2 *Output);

	PortalType GetPortalType() const;

	void TakeControlPoint(CIcPlayer *pPlayer);
	void SetBlocked(bool Blocked);
	int GetInfectedCID();
	float VisualRadius() const;
	void SetReady(bool Ready);

	void OnOwnershipChanged();

protected:
	// visual
	const float m_ParticleStartSpeed = 1.1f;
	const float m_ParticleAcceleration = 1.01f;
	int m_ParticleStopTickTime; // when X time is left stop creating particles - close animation

	int m_Ids[NUM_IDS];
	vec2 m_ParticlePos[NUM_IDS];
	vec2 m_ParticleVec[NUM_HINT];
	float m_Radius = 0;
	float m_Angle = 0;
	float m_SpeedMultiplier = 0;

	int m_OwnershipChangedTick = 0;
	int m_NextEffectTick = 0;
	bool m_Blocked = false;
	bool m_Infected = false;
	int m_InfectedPlayerId = -1;
	bool m_Ready = false;
};

#endif
