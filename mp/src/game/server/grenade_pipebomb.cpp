//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "basegrenade_shared.h"
#include "grenade_pipebomb.h"
#include "Sprite.h"
#include "SpriteTrail.h"
#include "soundent.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define pipebomb_GRENADE_BLIP_FREQUENCY			1.0f
#define pipebomb_GRENADE_BLIP_FAST_FREQUENCY	0.3f

#define pipebomb_GRENADE_GRACE_TIME_AFTER_PICKUP 1.5f
#define pipebomb_GRENADE_WARN_TIME 1.5f

const float GRENADE_COEFFICIENT_OF_RESTITUTION = 0.2f;

ConVar sk_plr_dmg_pipebombgrenade("sk_plr_dmg_pipebombgrenade", "0");
ConVar sk_npc_dmg_pipebombgrenade("sk_npc_dmg_pipebombgrenade", "0");
ConVar sk_pipebombgrenade_radius("sk_pipebombgrenade_radius", "0");

#define GRENADE_MODEL "models/Weapons/w_grenade.mdl"

class CGrenadepipebomb : public CBaseGrenade
{
	DECLARE_CLASS(CGrenadepipebomb, CBaseGrenade);

#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

	~CGrenadepipebomb(void);

public:
	void	Spawn(void);
	void	OnRestore(void);
	void	Precache(void);
	bool	CreateVPhysics(void);
	void	CreateEffects(void);
	void	SetTimer(float detonateDelay, float warnDelay);
	void	SetVelocity(const Vector &velocity, const AngularImpulse &angVelocity);
	int		OnTakeDamage(const CTakeDamageInfo &inputInfo);
	void	BlipSound() { EmitSound("Grenade.Blip"); }
	void	DelayThink();
	void	VPhysicsUpdate(IPhysicsObject *pPhysics);
	void	OnPhysGunPickup(CBasePlayer *pPhysGunUser, PhysGunPickup_t reason);
	void	SetCombineSpawned(bool combineSpawned) { m_combineSpawned = combineSpawned; }
	bool	IsCombineSpawned(void) const { return m_combineSpawned; }
	void	SetPunted(bool punt) { m_punted = punt; }
	bool	WasPunted(void) const { return m_punted; }

	// this function only used in episodic.
#if defined(HL2_EPISODIC) && 0 // FIXME: HandleInteraction() is no longer called now that base grenade derives from CBaseAnimating
	bool	HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt);
#endif 

	void	InputSetTimer(inputdata_t &inputdata);

protected:
	CHandle<CSprite>		m_pMainGlow;
	CHandle<CSpriteTrail>	m_pGlowTrail;

	float	m_flNextBlipTime;
	bool	m_inSolid;
	bool	m_combineSpawned;
	bool	m_punted;
};

LINK_ENTITY_TO_CLASS(npc_grenade_pipebomb, CGrenadepipebomb);

BEGIN_DATADESC(CGrenadepipebomb)

// Fields
DEFINE_FIELD(m_pMainGlow, FIELD_EHANDLE),
DEFINE_FIELD(m_pGlowTrail, FIELD_EHANDLE),
DEFINE_FIELD(m_flNextBlipTime, FIELD_TIME),
DEFINE_FIELD(m_inSolid, FIELD_BOOLEAN),
DEFINE_FIELD(m_combineSpawned, FIELD_BOOLEAN),
DEFINE_FIELD(m_punted, FIELD_BOOLEAN),

// Function Pointers
DEFINE_THINKFUNC(DelayThink),

// Inputs
DEFINE_INPUTFUNC(FIELD_FLOAT, "SetTimer", InputSetTimer),

END_DATADESC()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CGrenadepipebomb::~CGrenadepipebomb(void)
{
}

void CGrenadepipebomb::Spawn(void)
{
	Precache();

	SetModel(GRENADE_MODEL);

	if (GetOwnerEntity() && GetOwnerEntity()->IsPlayer())
	{
		m_flDamage = sk_plr_dmg_pipebombgrenade.GetFloat();
		m_DmgRadius = sk_pipebombgrenade_radius.GetFloat();
	}
	else
	{
		m_flDamage = sk_npc_dmg_pipebombgrenade.GetFloat();
		m_DmgRadius = sk_pipebombgrenade_radius.GetFloat();
	}

	m_takedamage = DAMAGE_YES;
	m_iHealth = 1;

	SetSize(-Vector(4, 4, 4), Vector(4, 4, 4));
	SetCollisionGroup(COLLISION_GROUP_WEAPON);
	CreateVPhysics();

	BlipSound();
	m_flNextBlipTime = gpGlobals->curtime + pipebomb_GRENADE_BLIP_FREQUENCY;

	AddSolidFlags(FSOLID_NOT_STANDABLE);

	m_combineSpawned = false;
	m_punted = false;

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadepipebomb::OnRestore(void)
{
	// If we were primed and ready to detonate, put FX on us.
	if (m_flDetonateTime > 0)
		CreateEffects();

	BaseClass::OnRestore();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGrenadepipebomb::CreateEffects(void)
{
	// Start up the eye glow
	m_pMainGlow = CSprite::SpriteCreate("sprites/redglow1.vmt", GetLocalOrigin(), false);

	int	nAttachment = LookupAttachment("fuse");

	if (m_pMainGlow != NULL)
	{
		m_pMainGlow->FollowEntity(this);
		m_pMainGlow->SetAttachment(this, nAttachment);
		m_pMainGlow->SetTransparency(kRenderGlow, 255, 255, 255, 200, kRenderFxNoDissipation);
		m_pMainGlow->SetScale(0.2f);
		m_pMainGlow->SetGlowProxySize(4.0f);
	}

	// Start up the eye trail
	m_pGlowTrail = CSpriteTrail::SpriteTrailCreate("sprites/bluelaser1.vmt", GetLocalOrigin(), false);

	if (m_pGlowTrail != NULL)
	{
		m_pGlowTrail->FollowEntity(this);
		m_pGlowTrail->SetAttachment(this, nAttachment);
		m_pGlowTrail->SetTransparency(kRenderTransAdd, 255, 0, 0, 255, kRenderFxNone);
		m_pGlowTrail->SetStartWidth(8.0f);
		m_pGlowTrail->SetEndWidth(1.0f);
		m_pGlowTrail->SetLifeTime(0.5f);
	}
}

bool CGrenadepipebomb::CreateVPhysics()
{
	// Create the object in the physics system
	VPhysicsInitNormal(SOLID_BBOX, 0, false);
	return true;
}

// this will hit only things that are in newCollisionGroup, but NOT in collisionGroupAlreadyChecked
class CTraceFilterCollisionGroupDelta : public CTraceFilterEntitiesOnly
{
public:
	// It does have a base, but we'll never network anything below here..
	DECLARE_CLASS_NOBASE(CTraceFilterCollisionGroupDelta);

	CTraceFilterCollisionGroupDelta(const IHandleEntity *passentity, int collisionGroupAlreadyChecked, int newCollisionGroup)
		: m_pPassEnt(passentity), m_collisionGroupAlreadyChecked(collisionGroupAlreadyChecked), m_newCollisionGroup(newCollisionGroup)
	{
	}

	virtual bool ShouldHitEntity(IHandleEntity *pHandleEntity, int contentsMask)
	{
		if (!PassServerEntityFilter(pHandleEntity, m_pPassEnt))
			return false;
		CBaseEntity *pEntity = EntityFromEntityHandle(pHandleEntity);

		if (pEntity)
		{
			if (g_pGameRules->ShouldCollide(m_collisionGroupAlreadyChecked, pEntity->GetCollisionGroup()))
				return false;
			if (g_pGameRules->ShouldCollide(m_newCollisionGroup, pEntity->GetCollisionGroup()))
				return true;
		}

		return false;
	}

protected:
	const IHandleEntity *m_pPassEnt;
	int		m_collisionGroupAlreadyChecked;
	int		m_newCollisionGroup;
};

void CGrenadepipebomb::VPhysicsUpdate(IPhysicsObject *pPhysics)
{
	BaseClass::VPhysicsUpdate(pPhysics);
	Vector vel;
	AngularImpulse angVel;
	pPhysics->GetVelocity(&vel, &angVel);

	Vector start = GetAbsOrigin();
	// find all entities that my collision group wouldn't hit, but COLLISION_GROUP_NONE would and bounce off of them as a ray cast
	CTraceFilterCollisionGroupDelta filter(this, GetCollisionGroup(), COLLISION_GROUP_NONE);
	trace_t tr;

	// UNDONE: Hull won't work with hitboxes - hits outer hull.  But the whole point of this test is to hit hitboxes.
#if 0
	UTIL_TraceHull(start, start + vel * gpGlobals->frametime, CollisionProp()->OBBMins(), CollisionProp()->OBBMaxs(), CONTENTS_HITBOX | CONTENTS_MONSTER | CONTENTS_SOLID, &filter, &tr);
#else
	UTIL_TraceLine(start, start + vel * gpGlobals->frametime, CONTENTS_HITBOX | CONTENTS_MONSTER | CONTENTS_SOLID, &filter, &tr);
#endif
	if (tr.startsolid)
	{
		if (!m_inSolid)
		{
			// UNDONE: Do a better contact solution that uses relative velocity?
			vel *= -GRENADE_COEFFICIENT_OF_RESTITUTION; // bounce backwards
			pPhysics->SetVelocity(&vel, NULL);
		}
		m_inSolid = true;
		return;
	}
	m_inSolid = false;
	if (tr.DidHit())
	{
		Vector dir = vel;
		VectorNormalize(dir);
		// send a tiny amount of damage so the character will react to getting bonked
		CTakeDamageInfo info(this, GetThrower(), pPhysics->GetMass() * vel, GetAbsOrigin(), 0.1f, DMG_CRUSH);
		tr.m_pEnt->TakeDamage(info);

		// reflect velocity around normal
		vel = -2.0f * tr.plane.normal * DotProduct(vel, tr.plane.normal) + vel;

		// absorb 80% in impact
		vel *= GRENADE_COEFFICIENT_OF_RESTITUTION;
		angVel *= -0.5f;
		pPhysics->SetVelocity(&vel, &angVel);
	}
}


void CGrenadepipebomb::Precache(void)
{
	PrecacheModel(GRENADE_MODEL);

	PrecacheScriptSound("Grenade.Blip");

	PrecacheModel("sprites/redglow1.vmt");
	PrecacheModel("sprites/bluelaser1.vmt");

	BaseClass::Precache();
}

void CGrenadepipebomb::SetTimer(float detonateDelay, float warnDelay)
{
	m_flDetonateTime = gpGlobals->curtime + detonateDelay;
	m_flWarnAITime = gpGlobals->curtime + warnDelay;
	SetThink(&CGrenadepipebomb::DelayThink);
	SetNextThink(gpGlobals->curtime);

	CreateEffects();
}

void CGrenadepipebomb::OnPhysGunPickup(CBasePlayer *pPhysGunUser, PhysGunPickup_t reason)
{
	SetThrower(pPhysGunUser);

#ifdef HL2MP
	SetTimer(pipebomb_GRENADE_GRACE_TIME_AFTER_PICKUP, pipebomb_GRENADE_GRACE_TIME_AFTER_PICKUP / 2);

	BlipSound();
	m_flNextBlipTime = gpGlobals->curtime + pipebomb_GRENADE_BLIP_FAST_FREQUENCY;
	m_bHasWarnedAI = true;
#else
	if (IsX360())
	{
		// Give 'em a couple of seconds to aim and throw. 
		SetTimer(2.0f, 1.0f);
		BlipSound();
		m_flNextBlipTime = gpGlobals->curtime + pipebomb_GRENADE_BLIP_FAST_FREQUENCY;
	}
#endif

#ifdef HL2_EPISODIC
	SetPunted(true);
#endif

	BaseClass::OnPhysGunPickup(pPhysGunUser, reason);
}

void CGrenadepipebomb::DelayThink()
{
	if (gpGlobals->curtime > m_flDetonateTime)
	{
		Detonate();
		return;
	}

	if (!m_bHasWarnedAI && gpGlobals->curtime >= m_flWarnAITime)
	{
#if !defined( CLIENT_DLL )
		CSoundEnt::InsertSound(SOUND_DANGER, GetAbsOrigin(), 400, 1.5, this);
#endif
		m_bHasWarnedAI = true;
	}

	if (gpGlobals->curtime > m_flNextBlipTime)
	{
		BlipSound();

		if (m_bHasWarnedAI)
		{
			m_flNextBlipTime = gpGlobals->curtime + pipebomb_GRENADE_BLIP_FAST_FREQUENCY;
		}
		else
		{
			m_flNextBlipTime = gpGlobals->curtime + pipebomb_GRENADE_BLIP_FREQUENCY;
		}
	}

	SetNextThink(gpGlobals->curtime + 0.1);
}

void CGrenadepipebomb::SetVelocity(const Vector &velocity, const AngularImpulse &angVelocity)
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();
	if (pPhysicsObject)
	{
		pPhysicsObject->AddVelocity(&velocity, &angVelocity);
	}
}

int CGrenadepipebomb::OnTakeDamage(const CTakeDamageInfo &inputInfo)
{
	// Manually apply vphysics because BaseCombatCharacter takedamage doesn't call back to CBaseEntity OnTakeDamage
	VPhysicsTakeDamage(inputInfo);

	// Grenades only suffer blast damage and burn damage.
	if (!(inputInfo.GetDamageType() & (DMG_BLAST | DMG_BURN)))
		return 0;

	return BaseClass::OnTakeDamage(inputInfo);
}

#if defined(HL2_EPISODIC) && 0 // FIXME: HandleInteraction() is no longer called now that base grenade derives from CBaseAnimating
extern int	g_interactionBarnacleVictimGrab; ///< usually declared in ai_interactions.h but no reason to haul all of that in here.
extern int g_interactionBarnacleVictimBite;
extern int g_interactionBarnacleVictimReleased;
bool CGrenadepipebomb::HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt)
{
	// allow pipebombnades to be grabbed by barnacles. 
	if (interactionType == g_interactionBarnacleVictimGrab)
	{
		// give the grenade another five seconds seconds so the player can have the satisfaction of blowing up the barnacle with it
		float timer = m_flDetonateTime - gpGlobals->curtime + 5.0f;
		SetTimer(timer, timer - pipebomb_GRENADE_WARN_TIME);

		return true;
	}
	else if (interactionType == g_interactionBarnacleVictimBite)
	{
		// detonate the grenade immediately 
		SetTimer(0, 0);
		return true;
	}
	else if (interactionType == g_interactionBarnacleVictimReleased)
	{
		// take the five seconds back off the timer.
		float timer = MAX(m_flDetonateTime - gpGlobals->curtime - 5.0f, 0.0f);
		SetTimer(timer, timer - pipebomb_GRENADE_WARN_TIME);
		return true;
	}
	else
	{
		return BaseClass::HandleInteraction(interactionType, data, sourceEnt);
	}
}
#endif

void CGrenadepipebomb::InputSetTimer(inputdata_t &inputdata)
{
	SetTimer(inputdata.value.Float(), inputdata.value.Float() - pipebomb_GRENADE_WARN_TIME);
}

CBaseGrenade *pipebombgrenade_Create(const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, float timer, bool combineSpawned)
{
	// Don't set the owner here, or the player can't interact with grenades he's thrown
	CGrenadepipebomb *pGrenade = (CGrenadepipebomb *)CBaseEntity::Create("npc_grenade_pipebomb", position, angles, pOwner);

	pGrenade->SetTimer(timer, timer - pipebomb_GRENADE_WARN_TIME);
	pGrenade->SetVelocity(velocity, angVelocity);
	pGrenade->SetThrower(ToBaseCombatCharacter(pOwner));
	pGrenade->m_takedamage = DAMAGE_EVENTS_ONLY;
	pGrenade->SetCombineSpawned(combineSpawned);

	return pGrenade;
}

bool pipebombgrenade_WasPunted(const CBaseEntity *pEntity)
{
	const CGrenadepipebomb *ppipebomb = dynamic_cast<const CGrenadepipebomb *>(pEntity);
	if (ppipebomb)
	{
		return ppipebomb->WasPunted();
	}

	return false;
}

bool pipebombgrenade_WasCreatedByCombine(const CBaseEntity *pEntity)
{
	const CGrenadepipebomb *ppipebomb = dynamic_cast<const CGrenadepipebomb *>(pEntity);
	if (ppipebomb)
	{
		return ppipebomb->IsCombineSpawned();
	}

	return false;
}