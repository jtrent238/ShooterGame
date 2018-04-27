//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Projectile shot from the AR2 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef	WeaponGren_H
#define	WeaponGren_H

#include "basegrenade_shared.h"
#include "weapon_hl2mpbase_machinegun.h"

#ifdef CLIENT_DLL
#define CWeaponGren C_WeaponGren
#endif

class CWeaponGren : public CHL2MPMachineGun
{
public:
	DECLARE_CLASS(CWeaponGren, CHL2MPMachineGun);

	CWeaponGren();

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	void	ItemPostFrame(void);
	void	Precache(void);

	void	SecondaryAttack(void);
	void	DelayedAttack(void);

	const char *GetTracerType(void) { return "AR2Tracer"; }

	void	AddViewKick(void);

	int		GetMinBurst(void) { return 2; }
	int		GetMaxBurst(void) { return 5; }
	float	GetFireRate(void) { return 0.1f; }

	bool	CanHolster(void);
	bool	Reload(void);

	Activity	GetPrimaryAttackActivity(void);

	void	DoImpactEffect(trace_t &tr, int nDamageType);

	virtual bool Deploy(void);


	virtual const Vector& GetBulletSpread(void)
	{
		static Vector cone;

		cone = VECTOR_CONE_3DEGREES;

		return cone;
	}

	const WeaponProficiencyInfo_t *GetProficiencyValues();

private:
	CWeaponGren(const CWeaponGren &);

protected:

	float					m_flDelayedFire;
	bool					m_bShotDelayed;
	int						m_nVentPose;

#ifndef CLIENT_DLL
	DECLARE_ACTTABLE();
#endif
};


#endif	//WeaponGren_H
