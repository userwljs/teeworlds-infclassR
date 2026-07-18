/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_GAMECORE_H
#define GAME_GAMECORE_H

#include <base/math.h>
#include <base/system.h>
#include <base/vmath.h>

#include <set>

#include <engine/shared/protocol.h>
#include <game/generated/protocol.h>
#include <math.h>

#include "mapitems.h"

class CCollision;
class CTeamsCore;

class CTuneParam
{
	int m_Value;

public:
	void Set(int v) { m_Value = v; }
	int Get() const { return m_Value; }
	CTuneParam &operator=(int v)
	{
		m_Value = (int)(v * 100.0f);
		return *this;
	}
	CTuneParam &operator=(float v)
	{
		m_Value = (int)(v * 100.0f);
		return *this;
	}
	operator float() const { return m_Value / 100.0f; }
};

class CTuningParams
{
	static const char *ms_apNames[];

public:
	CTuningParams()
	{
		const float TicksPerSecond = 50.0f;
#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description) m_##Name.Set((int)(Value * 100.0f));
#include "tuning.h"
#undef MACRO_TUNING_PARAM
	}

	bool operator==(const CTuningParams &TuningParams) const
	{
#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description) \
	if(m_##Name != TuningParams.m_##Name) \
		return false;
#include "tuning.h"
#undef MACRO_TUNING_PARAM
		return true;
	}

#define MACRO_TUNING_PARAM(Name, ScriptName, Value, Description) CTuneParam m_##Name;
#include "tuning.h"
#undef MACRO_TUNING_PARAM

	static int Num()
	{
		return sizeof(CTuningParams) / sizeof(int);
	}
	bool Set(int Index, float Value);
	bool Set(const char *pName, float Value);
	bool Get(int Index, float *pValue) const;
	bool Get(const char *pName, float *pValue) const;
	static const char *Name(int Index) { return ms_apNames[Index]; }
};

// Do not use these function unless for legacy code!
void StrToInts(int *pInts, size_t NumInts, const char *pStr);
bool IntsToStr(const int *pInts, size_t NumInts, char *pStr, size_t StrSize);

inline vec2 CalcPos(vec2 Pos, vec2 Velocity, float Curvature, float Speed, float Time)
{
	vec2 n;
	Time *= Speed;
	n.x = Pos.x + Velocity.x * Time;
	n.y = Pos.y + Velocity.y * Time + Curvature / 10000 * (Time * Time);
	return n;
}

template<typename T>
inline T SaturatedAdd(T Min, T Max, T Current, T Modifier)
{
	if(Modifier < 0)
	{
		if(Current < Min)
			return Current;
		Current += Modifier;
		if(Current < Min)
			Current = Min;
		return Current;
	}
	else
	{
		if(Current > Max)
			return Current;
		Current += Modifier;
		if(Current > Max)
			Current = Max;
		return Current;
	}
}

float VelocityRamp(float Value, float Start, float Range, float Curvature);

// hooking stuff
enum
{
	HOOK_RETRACTED = -1,
	HOOK_IDLE = 0,
	HOOK_RETRACT_START = 1,
	HOOK_RETRACT_END = 3,
	HOOK_FLYING,
	HOOK_GRABBED,

	COREEVENT_GROUND_JUMP = 0x01,
	COREEVENT_AIR_JUMP = 0x02,
	COREEVENT_HOOK_LAUNCH = 0x04,
	COREEVENT_HOOK_ATTACH_PLAYER = 0x08,
	COREEVENT_HOOK_ATTACH_GROUND = 0x10,
	COREEVENT_HOOK_HIT_NOHOOK = 0x20,
	COREEVENT_HOOK_RETRACT = 0x40,
	// COREEVENT_HOOK_TELE=0x80,
};

// show others values - do not change them
enum
{
	SHOW_OTHERS_NOT_SET = -1, // show others value before it is set
	SHOW_OTHERS_OFF = 0, // show no other players in solo or other teams
	SHOW_OTHERS_ON = 1, // show all other players in solo and other teams
	SHOW_OTHERS_ONLY_TEAM = 2 // show players that are in solo and are in the same team
};

struct SSwitchers
{
	bool m_aStatus[MAX_CLIENTS];
	bool m_Initial;
	int m_aEndTick[MAX_CLIENTS];
	int m_aType[MAX_CLIENTS];
	int m_aLastUpdateTick[MAX_CLIENTS];
};

class CWorldCore
{
public:
	CWorldCore()
	{
		mem_zero(m_apCharacters, sizeof(m_apCharacters));
	}

	CTuningParams m_Tuning;
	class CCharacterCore *m_apCharacters[MAX_CLIENTS];
};

class CCharacterCore
{
public:
	struct CParams : public CTuningParams
	{
		const CTuningParams *m_pTuningParams;
		int m_HookMode;
		int m_HookGrabTime;

		CParams(const CTuningParams *pTuningParams)
		{
			m_pTuningParams = pTuningParams;
			m_HookMode = 0;
			m_HookGrabTime = SERVER_TICK_SPEED + SERVER_TICK_SPEED / 5;
		}
	};
	CWorldCore *m_pWorld;
	const CCollision *m_pCollision;

	static constexpr float PhysicalSize() { return 28.0f; };
	static constexpr vec2 PhysicalSizeVec2() { return vec2(28.0f, 28.0f); };
	vec2 m_Pos;
	vec2 m_Vel;

	vec2 m_HookPos;
	vec2 m_HookDir;
	int m_HookTick;
	int m_HookState;
	std::set<int> m_AttachedPlayers;
	int HookedPlayer() const { return m_HookedPlayer; }
	void SetHookedPlayer(int HookedPlayer);

	bool m_HookProtected;
	bool m_Infected;
	bool m_InLove;
	// InfClassR
	int m_PassengerNumber = 0;
	static const float PassengerYOffset;
	CCharacterCore *m_Passenger;
	bool m_IsPassenger;
	bool m_ProbablyStucked;

	bool HasPassenger() const { return m_Passenger; }

	int m_Jumped;
	// m_JumpedTotal counts the jumps performed in the air
	int m_JumpedTotal;
	int m_Jumps;

	int m_Direction;
	int m_Angle;
	CNetObj_PlayerInput m_Input;

	int m_TriggeredEvents;

	void Init(CWorldCore *pWorld, const CCollision *pCollision, CTeamsCore *pTeams = nullptr);
	void Reset();
	bool IsGrounded() const;
	void TickDeferred(const CParams *pParams);
	void Tick(bool UseInput, const CParams *pParams);
	void Move(const CParams *pParams);

	void Read(const CNetObj_CharacterCore *pObjCore);
	void Write(CNetObj_CharacterCore *pObjCore) const;
	void Quantize();

	// DDRace

	int m_Id;

	// DDNet Character
	void SetTeamsCore(CTeamsCore *pTeams);
	bool m_Solo;
	bool m_CollisionDisabled;
	bool m_Super;
	bool m_EndlessJump;
	int m_FreezeStart;

private:
	CTeamsCore *m_pTeams;
	int m_MoveRestrictions;
	int m_HookedPlayer;

	// InfClass
public:
	bool m_ReflectingProjectiles{};
	bool IsRecursePassenger(CCharacterCore *pMaybePassenger) const;
	void TryBecomePassenger(CCharacterCore *pTaxi);
	void SetPassenger(CCharacterCore *pPassenger);

protected:
	void UpdateTaxiPassengers();
};

// input count
struct CInputCount
{
	int m_Presses;
	int m_Releases;
};

inline CInputCount CountInput(int Prev, int Cur)
{
	CInputCount c = {0, 0};
	Prev &= INPUT_STATE_MASK;
	Cur &= INPUT_STATE_MASK;
	int i = Prev;

	while(i != Cur)
	{
		i = (i + 1) & INPUT_STATE_MASK;
		if(i & 1)
			c.m_Presses++;
		else
			c.m_Releases++;
	}

	return c;
}

#endif
