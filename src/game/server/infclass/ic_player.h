#ifndef GAME_SERVER_INFCLASS_PLAYER_H
#define GAME_SERVER_INFCLASS_PLAYER_H

#include <base/tl/ic_array.h>

#include <game/gamecore.h>
#include <game/server/player.h>
#include <game/server/skininfo.h>

class CGameContext;
class CIcCharacter;
class CIcGameController;
class CIcPlayerClass;
struct SDamageContext;
struct SpawnContext;

enum class EDamageType;

enum class INFECTION_TYPE
{
	NO,
	REGULAR,
	RESTORE_INF_CLASS,
};

enum class INFECTION_CAUSE
{
	GAME,
	PLAYER,
};

enum class EPlayerScoreMode
{
	Class,
	Time,
	Clan,

	Count,
};

class CIcPlayer : public CPlayer
{
	MACRO_ALLOC_POOL_ID()

public:
	CIcPlayer(CIcGameController *pGameController, int UniqueClientId, int ClientId, int Team);
	~CIcPlayer() override;

	static CIcPlayer *GetInstance(CPlayer *pPlayer);
	static const CIcPlayer *GetInstance(const CPlayer *pPlayer);

	CIcGameController *GameController() const;

	void TryRespawn() override;

	int GetScore(int SnappingClient) const override;

	void Tick() override;
	void PostTick() override;
	void Snap(int SnappingClient) override;
	void SnapClientInfo(int SnappingClient, int SnappingClientMappedId) override;
	int GetDefaultEmote() const override;
	CWeakSkinInfo GetSkinInfo(int SnappingClient) const;

	bool GetAntiPingEnabled() const;
	void SetAntiPingEnabled(bool Enabled);

	void SetInfectionTimestamp(int Timestamp);
	int GetInfectionTimestamp() const;

	EPlayerClass GetPreferredClass() const { return m_PreferredClass; }
	void SetPreferredClass(EPlayerClass Class);
	void SetPreviouslyPickedClass(EPlayerClass Class);

	void HandleInfection();
	void KillCharacter(int Weapon = WEAPON_GAME) override;

	CIcCharacter *GetCharacter();
	const CIcCharacter *GetCharacter() const;
	CIcPlayerClass *GetCharacterClass() { return m_pInfcPlayerClass; }
	const CIcPlayerClass *GetCharacterClass() const { return m_pInfcPlayerClass; }
	void SetCharacterClass(CIcPlayerClass *pClass);

	void SetClass(EPlayerClass NewClass);
	void UpdateSkin();

	INFECTION_TYPE InfectionType() const { return m_InfectionType; }
	INFECTION_CAUSE InfectionCause() const { return m_InfectionCause; }
	void StartInfection(int InfectiousPlayerCid = -1, INFECTION_TYPE InfectionType = INFECTION_TYPE::REGULAR);
	bool IsInfectionStarted() const;

	int MapMenu() const { return (m_Team != TEAM_SPECTATORS) ? m_MapMenu : 0; }
	void OpenMapMenu(int Menu);
	void CloseMapMenu();
	bool MapMenuClickable();

	void SetHookProtection(bool Value, bool Automatic = true);
	bool HookProtectionEnabled() const { return m_HookProtection; }

	EPlayerScoreMode GetScoreMode() const;
	void SetScoreMode(EPlayerScoreMode Mode);

	int DieTick() const { return m_DieTick; }

	bool SpecialCameraIsActive() const;
	void ResetSpecialCamera();
	int GetSpecialCameraTargetCid() const;
	void SetSpecialCameraTargetCid(int ClientId, float Duration);

	int GetSpectatingCid() const;

	float GetGhoulPercent() const;
	void IncreaseGhoulLevel(int Diff);
	int GetGhoulLevel() const { return m_GhoulLevel; }

	void SetRandomClassChoosen();
	bool RandomClassChoosen() const;

	EPlayerClass GetPreviousInfectedClass() const;
	EPlayerClass GetPreviousHumanClass() const;
	EPlayerClass GetPreviouslyPickedClass() const;

	void AddSavedPosition(const vec2 Position);
	bool LoadSavedPosition(vec2 *pOutput) const;

	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const { return CPlayer::Server(); }

	void ResetRoundData();
	int GetInfectionTick() const { return m_InfectionTick; }

	void OnKill();
	void OnDeath();
	void OnAssist();

	int GetMaxHP() const { return m_MaxHP; }
	void SetMaxHP(int MaxHP);
	void ApplyMaxHP();

	int GetKills() const { return m_Kills; }
	int GetDeaths() const { return m_Deaths; }

	virtual void OnCharacterHPChanged() {};
	virtual void OnCharacterDamage(const SDamageContext &DamageContext) {};

public:
	int m_MapMenuItem = -1;

protected:
	virtual void OnCharacterSpawned(const SpawnContext &Context);
	const char *GetClan(int SnappingClient = -1) const override;
	void HandleAutoRespawn() override;
	void UpdateSpectatorPos();
	void UpdateSpecialCamera();

	void SendClassIntro();

	CSkinContext m_SameTeamSkinContext;
	CSkinContext m_DiffTeamSkinContext;
	SkinGetter m_SkinGetter;

	CIcGameController *m_pGameController = nullptr;
	CIcPlayerClass *m_pInfcPlayerClass = nullptr;
	int m_HumanTime{};
	int m_InfectionTick;
	bool m_HookProtection{};
	bool m_HookProtectionAutomatic{};

	bool m_aKnownClasses[NB_PLAYERCLASS]{};

	EPlayerClass m_PreferredClass;
	bool m_AntiPing = false;

	int m_MaxHP = 0;

	int m_Kills = 0;
	int m_Deaths = 0;
	int m_Assists = 0;
	int m_Score = 0;

	int m_RandomClassRoundId = 0;
	int m_GameInfectionTimestamp = 0;

	INFECTION_TYPE m_InfectionType = INFECTION_TYPE::NO;
	INFECTION_CAUSE m_InfectionCause = INFECTION_CAUSE::GAME;
	int m_InfectiousPlayerCid = -1;

	int m_SelfKillAttemptTick = -1;

	std::optional<int> m_FollowTargetId;
	std::optional<int> m_SpecialCameraTicks;

	int m_MapMenu = 0;
	int m_MapMenuTick = -1;

	EPlayerScoreMode m_ScoreMode = EPlayerScoreMode::Class;

	int m_GhoulLevel = 0;
	int m_GhoulLevelTick = 0;

	EPlayerClass m_PickedClass = EPlayerClass::Invalid;
	icArray<EPlayerClass, 5> m_PreviousClasses;
	icArray<vec2, 1> m_SavedPositions;
};

inline const CIcPlayer *CIcPlayer::GetInstance(const CPlayer *pPlayer)
{
	return static_cast<const CIcPlayer *>(pPlayer);
}

inline CIcPlayer *CIcPlayer::GetInstance(CPlayer *pPlayer)
{
	return static_cast<CIcPlayer *>(pPlayer);
}

enum
{
	PLAYERITER_ALL = 0x0,

	PLAYERITER_COND_READY = 0x1,
	PLAYERITER_COND_SPEC = 0x2,
	PLAYERITER_COND_NOSPEC = 0x4,

	PLAYERITER_INGAME = PLAYERITER_COND_READY | PLAYERITER_COND_NOSPEC,
	PLAYERITER_SPECTATORS = PLAYERITER_COND_READY | PLAYERITER_COND_SPEC,
};

template<int FLAGS>
class CIcPlayerIterator
{
public:
	CIcPlayerIterator(CPlayer **ppPlayers) :
		m_ppPlayers(ppPlayers)
	{
		Reset();
	}

	bool Next()
	{
		for(m_ClientId = m_ClientId + 1; m_ClientId < MAX_CLIENTS; m_ClientId++)
		{
			CPlayer *pPlayer = Player();

			if(!pPlayer)
				continue;
			if((FLAGS & PLAYERITER_COND_READY) && (!pPlayer->m_IsInGame || !pPlayer->m_IsReady))
				continue;
			if((FLAGS & PLAYERITER_COND_NOSPEC) && (pPlayer->GetTeam() == TEAM_SPECTATORS))
				continue;
			if((FLAGS & PLAYERITER_COND_SPEC) && (pPlayer->GetTeam() != TEAM_SPECTATORS))
				continue;

			return true;
		}

		return false;
	}

	void Reset() { m_ClientId = -1; }
	CIcPlayer *Player() { return static_cast<CIcPlayer *>(m_ppPlayers[m_ClientId]); }
	int ClientId() { return m_ClientId; }

private:
	CPlayer **m_ppPlayers;
	int m_ClientId;
};

#endif // GAME_SERVER_INFCLASS_PLAYER_H
