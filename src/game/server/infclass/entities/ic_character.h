#ifndef GAME_SERVER_INFCLASS_ENTITIES_CHARACTER_H
#define GAME_SERVER_INFCLASS_ENTITIES_CHARACTER_H

#include <game/infclass/ic_classes.h>
#include <game/server/entities/character.h>
#include <game/server/entities_filter.h>

#include <base/tl/ic_array.h>

class CGameContext;
class CIcGameController;
class CIcPlayer;
class CIcPlayerClass;
class CWhiteHole;

enum class EDamageType;
enum class EInfclassWeapon;
enum class EWeaponClass;
enum class TAKEDAMAGEMODE;

enum FREEZEREASON
{
	FREEZEREASON_FLASH = 0,
	FREEZEREASON_UNDEAD = 1,
	FREEZEREASON_INFECTION = 2,
};

struct SDamageContext;
struct DeathContext;

struct CHelperInfo
{
	int m_Cid = -1;
	int m_Tick = 0;
};

struct CDamagePoint
{
	EDamageType DamageType;
	int From = 0;
	int Amount = 0;
	int Tick = 0;
};

struct EnforcerInfo
{
	int m_Cid;
	int m_Tick;
	EDamageType m_DamageType;
};

struct WeaponFireContext
{
	int Weapon = 0;
	EInfclassWeapon InfClassWeapon{};
	bool FireAccepted = false;
	int AmmoAvailable = 0;
	int AmmoConsumed = 0;
	bool NoAmmo = false;
	float ReloadInterval = 0;
};

struct SpawnContext
{
	enum SPAWN_TYPE
	{
		MapSpawn,
		WitchSpawn,
		Count,
		Invalid = Count
	};

	vec2 SpawnPos = vec2(0, 0);
	SPAWN_TYPE SpawnType = MapSpawn;
};

const char *toString(SpawnContext::SPAWN_TYPE SpawnType);

using ClientsArray = icArray<int, MAX_CLIENTS>;

class CIcCharacter : public CCharacter
{
	MACRO_ALLOC_POOL_ID()
public:
	CIcCharacter(CIcGameController *pGameController, CNetObj_PlayerInput LastInput);
	~CIcCharacter() override;

	static const CIcCharacter *GetInstance(const CCharacter *pCharacter);
	static CIcCharacter *GetInstance(CCharacter *pCharacter);

	static CharacterFilter GetInfectedFilter();
	static CharacterFilter GetHumansFilter();
	static CharacterFilter GetExceptCharacterFilter(int ClientId);
	static CharacterFilter GetExceptCharactersFilter(const icArray<const CIcCharacter *, 10> &aCharacters);
	static CharacterFilter GetFilterAllOff(CharacterFilter Filter1, CharacterFilter Filter2);

	void OnCharacterSpawned(const SpawnContext &Context);
	void OnCharacterInInfectionZone();
	void OnCharacterOutOfInfectionZone();
	void OnCharacterInDamageZone(float Damage, float DamageInterval = 1.0f);

	void Destroy() override;
	void TickBeforeWorld();
	void Tick() override;
	void TickDeferred() override;
	void TickPaused() override;
	void Snap(int SnappingClient) override;
	void SpecialSnapForClient(int SnappingClient, bool *pDoSnap);

	void HandleNinja() override;
	void HandleNinjaMove(float NinjaVelocity);
	void HandleWeaponSwitch() override;

	void FireWeapon() override;

	bool TakeDamage(const vec2 &Force, float Dmg, int From, EDamageType DamageType, float *pDamagePointsLeft = nullptr);

	bool Heal(int HitPoints, std::optional<int> FromCid = {});
	bool GiveHealth(int HitPoints, std::optional<int> FromCid = {});
	bool GiveArmor(int HitPoints, std::optional<int> FromCid = {});

	void SetJumpsLimit(int Limit);

	EPlayerClass GetPlayerClass() const;

	int GetDropLevel() const { return m_DropLevel; }
	void SetDropLevel(int Level);

	void OpenClassChooser();
	void HandleMapMenu();
	void HandleMapMenuClicked();
	void HandleWeaponsRegen();
	void HandleIndirectKillerCleanup();

	void Die(int Killer, int Weapon) override;
	void Die(int Killer, EDamageType DamageType);
	void Die(DeathContext *pContext);

	CAmmoParams GetAmmoParams(EWeapon Weapon) const override;

	void SetLastWeapon(int Weapon);
	bool HasWeapon(int Weapon) const;
	bool HasWeapon(EWeaponClass WeaponClass) const;

	CIcPlayer *GetPlayer();

	const CIcPlayerClass *GetClass() const { return m_pClass; }
	CIcPlayerClass *GetClass() { return m_pClass; }
	void SetClass(CIcPlayerClass *pClass);

	CInputCount CountFireInput() const;
	bool FireJustPressed() const;

	int GetAttackTick() const { return m_AttackTick; }
	int GetLastNoAmmoSoundTick() const { return m_LastNoAmmoSound; }
	int GetActiveWeapon() const { return m_ActiveWeapon; }
	int GetReloadTimer() const { return m_ReloadTimer; }
	void SetReloadTimer(int Ticks);
	void SetReloadDuration(float Seconds);

	void SetAntiFire();
	void SetAntiFireDuration(float Seconds);

	vec2 GetHookPos() const;
	int GetHookedPlayer() const;
	void SetHookedPlayer(int ClientId);

	vec2 Velocity() const;
	float Speed() const;

	CIcGameController *GameController() const { return m_pGameController; }
	CGameContext *GameContext() const;

	bool CanDie() const;

	bool CanJump() const;

	int GetInAirTick() const { return m_InAirTick; }

	bool IsVisibleForPlayer(int ClientId) const;
	bool IsInvisible() const;
	bool HasGrantedInvisibility() const;
	bool IsSolo() const;
	bool IsInvincible() const; // Invincible here means "ignores all damage"
	void SetInvincible(int Invincible);
	bool HasHallucination() const;
	void Freeze(float Time, int Player, FREEZEREASON Reason);
	bool IsFrozen() const;
	void Unfreeze();
	void TryUnfreeze(int UnfreezerCid = -1);
	FREEZEREASON GetFreezeReason() const { return m_FreezeReason; }
	int GetFreezer() const;
	int FreezeStartTick() const;

	bool IsBlind() const { return m_BlindnessTicks > 0; }

	void ResetBlindness();
	void MakeBlind(float Duration, std::optional<int> FromCid = {});

	float WebHookLength() const;

	void GiveRandomClassSelectionBonus();
	void MakeVisible();
	void MakeInvisible();
	void GrantInvisibility(float Duration);
	void SetSoloForDuration(float Duration);
	void GrantSpawnProtection(float Duration);

	bool PositionIsLocked() const;
	void LockPosition();
	void UnlockPosition();

	void CancelLoveEffect();

	bool IsSleeping() const { return (m_SleepTicks > 0) || (m_DeepSleepTicks > 0); }
	void PutToSleep(float Duration, std::optional<int> FromCid = {});
	void PutToDeepSleep(float Duration, std::optional<int> FromCid = {});
	void CancelSleep(std::optional<int> ByCid = {});
	void CancelDeepSleep(std::optional<int> ByCid = {});

	bool IsInSlowMotion() const;
	float SlowMotionEffect(float Duration, std::optional<int> FromCid = {});
	void CancelSlowMotion();

	bool IsPoisoned() const;
	void Poison(int Count, int From, EDamageType DamageType, float Interval = 1.0f);
	void ResetPoisonEffect();

	void ResetMovementsInput();
	void ResetHookInput();

	int GetCursorId() const { return m_CursorId; }
	int GetFlagId() const { return m_FlagId; }
	int GetHeartId() const { return m_HeartId; }

	bool IsInfected() const;
	bool IsHuman() const;

	void AddHelper(int HelperCid, float Time);
	void ResetHelpers();

	void GetDeathContext(const SDamageContext &DamageContext, DeathContext *pContext) const;

	void UpdateLastHookers(const ClientsArray &Hookers, int HookerTick);

	void UpdateLastEnforcer(int ClientId, float Force, EDamageType DamageType, int Tick);

	void RemoveReferencesToCid(int ClientId);

	void SaturateVelocity(vec2 Force, float MaxSpeed);
	bool IsPassenger() const;
	bool HasPassenger() const;
	CIcCharacter *GetPassenger() const;
	CIcCharacter *GetTaxi() const;
	// Driver is the last Taxi in a chain
	CIcCharacter *GetTaxiDriver() const;
	void SetPassenger(CIcCharacter *pPassenger);
	void TryBecomePassenger(CIcCharacter *pTargetDriver);
	int GetInfZoneTick();

	bool HasSuperWeaponIndicator() const;
	void SetSuperWeaponIndicatorEnabled(bool Enabled);

	EInfclassWeapon GetInfWeaponId(int WID = -1) const;

	using CCharacter::GameWorld;
	using CCharacter::Server;

	CGameWorld *GameWorld() const;
	const IServer *Server() const;

	const auto &GetTakenDamageDetails() const { return m_TakenDamageDetails; }

protected:
	void PreCoreTick() override;
	void PostCoreTick() override;

	void PrepareSnapContext(CCharacterSnapContext &SnapContext) const override;

	void ClassSpawnAttributes();
	void DestroyChildEntities();

	void FreeChildSnapIds();

	void UpdateCoreSolo();
	void UpdateTuningParam();

	void ResetClassObject();
	void HandleDamage(int From, int Damage, EDamageType DamageType);

	void OnTotalHealthChanged(int Difference) override;
	void PrepareToDie(DeathContext *pContext);

protected:
	CIcGameController *m_pGameController = nullptr;
	CIcPlayerClass *m_pClass = nullptr;

	CNetObj_PlayerInput m_InputBackup;

	int m_FlagId;
	int m_HeartId;
	int m_CursorId;
	int m_DropLevel = 0;

	CHelperInfo m_LastHelper;
	ClientsArray m_LastHookers;
	int m_LastHookerTick = -1;

	int m_BlindnessTicks = 0;
	std::optional<int> m_LastBlinder;

	int m_ProtectionTick = 0;

	int m_InfZoneTick = 0;
	int m_DamageZoneTick;
	float m_DamageZoneDealtDamage = 0;

	icArray<EnforcerInfo, 4> m_EnforcersInfo;

	int m_DamageTaken = 0;
	icArray<CDamagePoint, 4> m_TakenDamageDetails;
	bool m_PositionLocked = false;

	int m_SleepTicks = 0;
	int m_DeepSleepTicks = 0;
	std::optional<int> m_PutToSleepBy;
	std::optional<int> m_PutToDeepSleepBy;

	bool m_HasIndicator{};
	bool m_IsFrozen = false;
	int m_FrozenTime;
	int m_LastFreezer = -1;
	FREEZEREASON m_FreezeReason;

	int m_SlowMotionTick;
	std::optional<int> m_SlowEffectApplicant;

	int m_Poison = 0;
	float m_PoisonEffectInterval{};
	int m_PoisonTick = 0;
	int m_PoisonFrom = 0;
	EDamageType m_PoisonDamageType;

	bool m_IsInvisible = false;
	int m_AntiFireTime = 0;
	int m_GrantedInvisibilityUntilTick = 0;
	std::optional<int> m_SoloUntilTick;
	int m_Invincible = 0;

	int m_HealTick = 0;
};

inline const CIcCharacter *CIcCharacter::GetInstance(const CCharacter *pCharacter)
{
	return static_cast<const CIcCharacter *>(pCharacter);
}

inline CIcCharacter *CIcCharacter::GetInstance(CCharacter *pCharacter)
{
	return static_cast<CIcCharacter *>(pCharacter);
}

#endif // GAME_SERVER_INFCLASS_ENTITIES_CHARACTER_H
