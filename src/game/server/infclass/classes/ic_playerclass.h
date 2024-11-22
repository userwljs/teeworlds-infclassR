#ifndef GAME_SERVER_INFCLASS_CLASSES_PLAYER_CLASS_H
#define GAME_SERVER_INFCLASS_CLASSES_PLAYER_CLASS_H

#include <base/tl/ic_array.h>
#include <base/vmath.h>
#include <game/server/entity.h>
#include <game/server/skininfo.h>

class CConfig;
class CGameContext;
class CGameWorld;
class CIcCharacter;
class CIcGameController;
class CIcPlayer;
class IServer;

struct SpawnContext;
struct SDamageContext;
struct DeathContext;
struct WeaponFireContext;
struct CAmmoParams;

enum class EUpgradeType;
using PlayerUpgradesArray = icArray<EUpgradeType, 5>;

enum class EDamageType;
enum class IC_PICKUP_TYPE;

class CIcPlayerClass
{
public:
	CIcPlayerClass(CIcPlayer *pPlayer);
	virtual ~CIcPlayerClass() = default;

	void SetCharacter(CIcCharacter *character);

	virtual bool IsHuman() const = 0;
	bool IsZombie() const;

	virtual SkinGetter GetSkinGetter() const = 0;
	virtual void SetupSkinContext(CSkinContext *pOutput, bool ForSameTeam) const = 0;

	virtual void ResetNormalEmote();
	void SetNormalEmote(int Emote);
	int GetDefaultEmote() const;
	virtual CAmmoParams GetAmmoParams(int Weapon) const;
	virtual int GetJumps() const;

	virtual bool CanDie() const;
	virtual bool CanBeHit() const;
	virtual bool CanBeUnfreezed() const;
	virtual PlayerUpgradesArray GetUpgrade(int Level) const;
	PlayerUpgradesArray GetNextUpgrade() const;

	float GetHammerProjOffset() const;
	float GetHammerRange() const;
	virtual float GetGhoulPercent() const;

	// Temp stuff
	void UpdateSkin();
	EPlayerClass GetPlayerClass() const;
	virtual void OnPlayerClassChanged();

	virtual void PrepareToDie(DeathContext *Context);

	bool IsHealingDisabled() const;
	void DisableHealing(float Duration, int From, EDamageType DamageType);

	// Events
	virtual void OnPlayerSnap(int SnappingClient, int InfClassVersion);

	virtual void OnCharacterPreCoreTick();
	virtual void OnCharacterTick();
	virtual void OnCharacterTickPaused();
	virtual void OnCharacterPostCoreTick();
	virtual void OnCharacterTickDeferred();
	virtual void OnCharacterSnap(int SnappingClient);
	virtual void OnCharacterSpawned(const SpawnContext &Context);
	virtual void OnCharacterDeath(EDamageType DamageType);
	virtual void OnCharacterDamage(SDamageContext *pContext);

	virtual void OnKilledCharacter(CIcCharacter *pVictim, const DeathContext &Context);

	virtual void OnHookAttachedPlayer();

	virtual void HandleNinja() {};

	virtual void OnWeaponFired(WeaponFireContext *pFireContext);

	virtual void OnHammerFired(WeaponFireContext *pFireContext);
	virtual void OnGunFired(WeaponFireContext *pFireContext);
	virtual void OnShotgunFired(WeaponFireContext *pFireContext);
	virtual void OnGrenadeFired(WeaponFireContext *pFireContext);
	virtual void OnLaserFired(WeaponFireContext *pFireContext);
	virtual void OnNinjaFired(WeaponFireContext *pFireContext);

	virtual void OnSlimeEffect(int Owner, int Damage, float DamageInterval) = 0;
	virtual void OnFloatingPointCollected(int Points);

	virtual void GiveUpgrades(const PlayerUpgradesArray &NewUpgrades){};

	CGameContext *GameContext() const;
	CGameContext *GameServer() const;
	CGameWorld *GameWorld() const;
	CIcGameController *GameController() const;
	CConfig *Config();
	const CConfig *Config() const;
	IServer *Server() const;
	CIcPlayer *GetPlayer();
	const CIcPlayer *GetPlayer() const;
	int GetCid() const;
	vec2 GetPos() const;
	vec2 GetDirection() const;
	vec2 GetProjectileStartPos(float Offset) const;
	float GetProximityRadius() const;

	int GetUpgradeLevel() const;
	void ResetUpgradeLevel();

	bool HasUpgrade(EUpgradeType Upgrade) const { return m_Upgrades.Contains(Upgrade); }

protected:
	virtual void GiveClassAttributes();
	virtual void DestroyChildEntities();
	virtual void BroadcastWeaponState() const;

	void CreateHammerHit(const vec2 &ProjStartPos, const CIcCharacter *pTarget);

	CIcPlayer *m_pPlayer = nullptr;
	CIcCharacter *m_pCharacter = nullptr;
	int m_NormalEmote;

	int m_UpgradeLevel = 0;
	PlayerUpgradesArray m_Upgrades;
	int m_ControlPointEffectAppliedTick = 0;
	int m_HealingDisabledTicks = 0;
};

#endif // GAME_SERVER_INFCLASS_CLASSES_PLAYER_CLASS_H
