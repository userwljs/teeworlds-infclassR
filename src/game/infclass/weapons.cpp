#include "weapons.h"

EWeaponClass GetWeaponClassById(EInfclassWeapon Weapon)
{
	switch(Weapon)
	{
	case EInfclassWeapon::HAMMER:
	case EInfclassWeapon::JAWS:
	case EInfclassWeapon::SLIME:
	case EInfclassWeapon::INFECTED_HAMMER:
		return EWeaponClass::HAMMER;
	case EInfclassWeapon::BOOMER_EXPLOSION:
		return EWeaponClass::SELF_EXPLOSION;
	case EInfclassWeapon::SOLDIER_GRENADE:
	case EInfclassWeapon::HERO_GRENADE:
	case EInfclassWeapon::LOOPER_GRENADE:
		return EWeaponClass::GRENADE;
	case EInfclassWeapon::LASER:
	case EInfclassWeapon::SNIPER_RIFLE:
	case EInfclassWeapon::EXPLOSIVE_LASER:
	case EInfclassWeapon::HERO_LASER:
	case EInfclassWeapon::LOOPER_LASER:
		return EWeaponClass::LASER;
	default:
		break;
	}

	return EWeaponClass::UNKNOWN;
}
