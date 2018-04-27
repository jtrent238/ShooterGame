//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GRENADE_pipebomb_H
#define GRENADE_pipebomb_H
#pragma once

class CBaseGrenade;
struct edict_t;

CBaseGrenade *pipebombgrenade_Create(const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner, float timer, bool combineSpawned);
bool	pipebombgrenade_WasPunted(const CBaseEntity *pEntity);
bool	pipebombgrenade_WasCreatedByCombine(const CBaseEntity *pEntity);

#endif // GRENADE_pipebomb_H
