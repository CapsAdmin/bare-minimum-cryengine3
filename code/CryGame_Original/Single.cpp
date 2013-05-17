/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 11:9:2005   15:00 : Created by Marcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Single.h"
#include "Item.h"
#include "Weapon.h"
#include "Projectile.h"
#include "Actor.h"
#include "Player.h"
#include "Game.h"
#include "GameCVars.h"
#include "WeaponSystem.h"
#include <IEntitySystem.h>
#include "ISound.h"
#include <IVehicleSystem.h>
#include <IMaterialEffects.h>
#include "GameRules.h"
#include <Cry_GeoDistance.h>

#include "IronSight.h"

#include "IRenderer.h"
#include "IRenderAuxGeom.h"

#include "WeaponSharedParams.h"

#pragma warning(disable: 4244)

struct DebugShoot
{
	Vec3 pos;
	Vec3 hit;
	Vec3 dir;
};

//std::vector<DebugShoot> g_shoots;

//---------------------------------------------------------------------------
// TODO remove when aiming/fire direction is working
// debugging aiming dir
struct DBG_shoot
{
	Vec3	src;
	Vec3	dst;
};

const	int	DGB_ShotCounter(3);
int	DGB_curIdx(-1);
int	DGB_curLimit(-1);
DBG_shoot DGB_shots[DGB_ShotCounter];
// remove over
//---------------------------------------------------------------------------



//------------------------------------------------------------------------
CSingle::CSingle()
	: m_pWeapon(0),
	  m_projectileId(0),
	  m_enabled(true),
	  m_mflTimer(0.0f),
	  m_suTimer(0.0f),
	  m_speed_scale(1.0f),
	  m_zoomtimeout(0.0f),
	  m_bLocked(false),
	  m_fStareTime(0.0f),
	  m_suId(0),
	  m_sulightId(0),
	  m_lockedTarget(0),
	  m_recoil(0.0f),
	  m_recoilMultiplier(1.0f),
	  m_recoil_dir_idx(0),
	  m_recoil_dir(0, 0),
	  m_recoil_offset(0, 0),
	  m_spread(0),
	  m_spinUpTime(0),
	  m_firstShot(true),
	  m_next_shot(0),
	  m_next_shot_dt(0),
	  m_emptyclip(false),
	  m_reloading(false),
	  m_firing(false),
	  m_fired(false),
	  m_heatEffectId(0),
	  m_heatSoundId(INVALID_SOUNDID),
	  m_barrelId(0),
	  m_autoaimTimeOut(AUTOAIM_TIME_OUT),
	  m_bLocking(false),
	  m_autoFireTimer(-1.0f),
	  m_autoAimHelperTimer(-1.0f),
	  m_reloadCancelled(false),
	  m_reloadStartFrame(0),
	  m_reloadPending(false),
	  m_lastModeStrength(false),
	  m_cocking(false),
	  m_smokeEffectId(0),
	  m_nextHeatTime(0.0f),
	  m_saved_next_shot(0.0f),
	  m_useCustomParams(false),
	  m_firePending(false)
{
	m_mflightId[0] = 0;
	m_mflightId[1] = 0;
	m_soundVariationParam = floor_tpl(Random(1.1f, 3.9f));		//1.0, 2.0f or 3.0f
	m_targetSpot.Set(0.0f, 0.0f, 0.0f);
}

//------------------------------------------------------------------------
CSingle::~CSingle()
{
	ClearTracerCache();
	m_fireParams = 0;
}

//------------------------------------------------------------------------
void CSingle::Init(IWeapon *pWeapon, const IItemParamsNode *params, uint32 id)
{
	m_pWeapon = static_cast<CWeapon *>(pWeapon);
	m_fmIdx = id;
	InitSharedParams();
	CacheSharedParamsPtr();

	if (params)
	{
		ResetParams(params);
	}

	CacheTracer();
}

//----------------------------------------------------------------------
void CSingle::PostInit()
{
	CacheAmmoGeometry();
}

//----------------------------------------------------------------------
void CSingle::InitSharedParams()
{
	CWeaponSharedParams *pWSP = m_pWeapon->GetWeaponSharedParams();
	assert(pWSP);
	m_fireParams	= pWSP->GetFireSharedParams("SingleData", m_fmIdx);
}

//-----------------------------------------------------------------------
void CSingle::CacheSharedParamsPtr()
{
	m_pShared			= static_cast<CSingleSharedData *>(m_fireParams.get());
}

//-----------------------------------------------------------
void CSingle::ModifyParams(bool modify, bool modified /* = false */)
{
	CWeaponSharedParams *pWSP = m_pWeapon->GetWeaponSharedParams();
	assert(pWSP);
	const char *dataType = m_fireParams->GetDataType();

	//Require it's own data, separated from shared "pool"
	if(modify)
	{
		if(!m_useCustomParams)
		{
			m_fireParams = 0;
			m_fireParams	= pWSP->CreateFireParams(dataType);
			assert(m_fireParams.get());
			CacheSharedParamsPtr();
			m_useCustomParams = true;
		}

		m_fireParams->SetValid(false);
	}
	else
	{
		m_fireParams->SetValid(true);

		if(m_useCustomParams && !modified)
		{
			//No modifications, release custom ones and use shared ones
			m_fireParams	= 0;
			m_fireParams	= pWSP->GetFireSharedParams(dataType, m_fmIdx);
			CacheSharedParamsPtr();
			m_useCustomParams = false;
		}
	}
}

//------------------------------------------------------------------------
void CSingle::Update(float frameTime, uint32 frameId)
{
	FUNCTION_PROFILER( GetISystem(), PROFILE_GAME );
	bool keepUpdating = false;
	CActor *pActor = m_pWeapon->GetOwnerActor();

	if (m_firePending)
	{
		StartFire();

		if (!m_firePending)
		{
			StopFire();
		}
	}

	if (m_pShared->fireparams.autoaim && m_pWeapon->IsSelected() && pActor && pActor->IsClient())
	{
		//For the LAW only use "cruise-mode" while you are using the zoom...
		if(!m_pShared->fireparams.autoaim_zoom || (m_pShared->fireparams.autoaim_zoom && m_pWeapon->IsZoomed()))
		{
			UpdateAutoAim(frameTime);
		}
		else if(m_pShared->fireparams.autoaim_zoom && !m_pWeapon->IsZoomed() && (m_bLocked || m_bLocking) )
		{
			Unlock();
		}

		keepUpdating = true;
	}

	if (m_zoomtimeout > 0.0f && m_pShared->fireparams.autozoom)
	{
		m_zoomtimeout -= frameTime;

		if (m_zoomtimeout < 0.0f)
		{
			m_zoomtimeout = 0.0f;
			CScreenEffects *pSE = pActor ? pActor->GetScreenEffects() : NULL;

			if (pSE)
			{
				// Only start a zoom out if we're zooming in and not already zooming out
				if (pSE->HasJobs(CScreenEffects::eSFX_GID_ZoomIn) && !pSE->HasJobs(CScreenEffects::eSFX_GID_ZoomOut))
				{
					pSE->ResetBlendGroup(CScreenEffects::eSFX_GID_ZoomIn, false);
					IBlendType   *blend = CBlendType<CLinearBlend>::Create(CLinearBlend(1.0f));
					IBlendedEffect *zoomOutEffect	= CBlendedEffect<CFOVEffect>::Create(CFOVEffect(pActor->GetEntityId(), 1.0f));
					pSE->StartBlend(zoomOutEffect, blend, 1.0f / .1f, CScreenEffects::eSFX_GID_ZoomOut);
				}
			}
		}

		keepUpdating = true;
	}

	if (m_spinUpTime > 0.0f)
	{
		m_spinUpTime -= frameTime;

		if (m_spinUpTime <= 0.0f)
		{
			m_spinUpTime = 0.0f;
			Shoot(true);
		}

		keepUpdating = true;
	}
	else
	{
		if (m_next_shot > 0.0f)
		{
			m_next_shot -= frameTime;

			if (m_next_shot <= 0.0f)
			{
				m_next_shot = 0.0f;
			}

			keepUpdating = true;
		}
	}

	if (IsFiring())
	{
		if(m_pShared->fireparams.auto_fire && m_autoFireTimer > 0.0f)
		{
			m_autoFireTimer -= frameTime;

			if(m_autoFireTimer <= 0.0f)
			{
				SetAutoFireTimer(m_autoFireTimerLength);
				AutoFire();
			}

			keepUpdating = true;
		}
	}

	if (IsReadyToFire())
	{
		m_pWeapon->OnReadyToFire();
	}

	// update muzzle flash light
	if (m_mflTimer > 0.0f)
	{
		m_mflTimer -= frameTime;

		if (m_mflTimer <= 0.0f)
		{
			m_mflTimer = 0.0f;

			if (m_mflightId[0])
			{
				m_pWeapon->EnableLight(false, m_mflightId[0]);
			}

			if (m_mflightId[1])
			{
				m_pWeapon->EnableLight(false, m_mflightId[1]);
			}
		}

		keepUpdating = true;
	}

	// update spinup effect
	if (m_suTimer > 0.0f)
	{
		m_suTimer -= frameTime;

		if (m_suTimer <= 0.0f)
		{
			m_suTimer = 0.0f;

			if (m_suId)
			{
				SpinUpEffect(false);
			}
		}

		keepUpdating = true;
	}

	UpdateRecoil(frameTime);
	UpdateHeat(frameTime);
	m_fired = false;

	if (g_pGameCVars->aim_assistCrosshairDebug && m_pShared->fireparams.crosshair_assist_range > 0.f && m_pWeapon->GetOwnerActor() && m_pWeapon->GetOwnerActor()->IsClient())
	{
		// debug only
		bool bHit(false);
		ray_hit rayhit;
		rayhit.pCollider = 0;
		Vec3 hit = GetProbableHit(WEAPON_HIT_RANGE, &bHit, &rayhit);
		Vec3 pos = GetFiringPos(hit);
		Vec3 dir = GetFiringDir(hit, pos);
		CrosshairAssistAiming(pos, dir, &rayhit);
		keepUpdating = true;
	}

	if (keepUpdating)
	{
		m_pWeapon->RequireUpdate(eIUS_FireMode);
	}

	//---------------------------------------------------------------------------
	// TODO remove when aiming/fire direction is working
	// debugging aiming dir
	static ICVar *pAimDebug = gEnv->pConsole->GetCVar("g_aimdebug");

	if(pAimDebug->GetIVal() != 0)
	{
		const ColorF	queueFireCol( .4f, 1.0f, 0.4f, 1.0f );

		for(int dbgIdx(0); dbgIdx < DGB_curLimit; ++dbgIdx)
		{
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine( DGB_shots[dbgIdx].src, queueFireCol, DGB_shots[dbgIdx].dst, queueFireCol );
		}
	}

	if(g_pGameCVars->i_debug_zoom_mods != 0 && m_pWeapon->GetOwnerActor() && m_pWeapon->GetOwnerActor()->IsPlayer())
	{
		float white[4] = {1, 1, 1, 1};
		gEnv->pRenderer->Draw2dLabel(50.0f, 50.0f, 1.4f, white, false, "Recoil.angular_impulse : %f", m_recoilparams.angular_impulse);
		gEnv->pRenderer->Draw2dLabel(50.0f, 60.0f, 1.4f, white, false, "Recoil.attack : %f", m_recoilparams.attack);
		gEnv->pRenderer->Draw2dLabel(50.0f, 70.0f, 1.4f, white, false, "Recoil.back_impulse : %f", m_recoilparams.back_impulse);
		gEnv->pRenderer->Draw2dLabel(50.0f, 80.0f, 1.4f, white, false, "Recoil.decay : %f", m_recoilparams.decay);
		gEnv->pRenderer->Draw2dLabel(50.0f, 90.0f, 1.4f, white, false, "Recoil.impulse : %f", m_recoilparams.impulse);
		gEnv->pRenderer->Draw2dLabel(50.0f, 100.0f, 1.4f, white, false, "Recoil.max x,y : %f, %f", m_recoilparams.max.x, m_recoilparams.max.y);
		gEnv->pRenderer->Draw2dLabel(50.0f, 110.0f, 1.4f, white, false, "Recoil.max_recoil : %f", m_recoilparams.max_recoil);
		gEnv->pRenderer->Draw2dLabel(50.0f, 120.0f, 1.4f, white, false, "Recoil.recoil_crouch_m : %f", m_recoilparams.recoil_crouch_m);
		gEnv->pRenderer->Draw2dLabel(50.0f, 130.0f, 1.4f, white, false, "Recoil.recoil_jump_m : %f", m_recoilparams.recoil_jump_m);
		gEnv->pRenderer->Draw2dLabel(50.0f, 140.0f, 1.4f, white, false, "Recoil.recoil_prone_m : %f", m_recoilparams.recoil_prone_m);
		gEnv->pRenderer->Draw2dLabel(50.0f, 150.0f, 1.4f, white, false, "Recoil.recoil_strMode_m : %f", m_recoilparams.recoil_strMode_m);
		gEnv->pRenderer->Draw2dLabel(50.0f, 160.0f, 1.4f, white, false, "Recoil.recoil_zeroG_m : %f", m_recoilparams.recoil_zeroG_m);
		gEnv->pRenderer->Draw2dLabel(300.0f, 50.0f, 1.4f, white, false, "Spread.attack : %f", m_spreadparams.attack);
		gEnv->pRenderer->Draw2dLabel(300.0f, 60.0f, 1.4f, white, false, "Spread.decay : %f", m_spreadparams.decay);
		gEnv->pRenderer->Draw2dLabel(300.0f, 70.0f, 1.4f, white, false, "Spread.max : %f", m_spreadparams.max);
		gEnv->pRenderer->Draw2dLabel(300.0f, 80.0f, 1.4f, white, false, "Spread.min : %f", m_spreadparams.min);
		gEnv->pRenderer->Draw2dLabel(300.0f, 90.0f, 1.4f, white, false, "Spread.rotation_m : %f", m_spreadparams.rotation_m);
		gEnv->pRenderer->Draw2dLabel(300.0f, 100.0f, 1.4f, white, false, "Spread.speed_m : %f", m_spreadparams.speed_m);
		gEnv->pRenderer->Draw2dLabel(300.0f, 110.0f, 1.4f, white, false, "Spread.spread_crouch_m : %f", m_spreadparams.spread_crouch_m);
		gEnv->pRenderer->Draw2dLabel(300.0f, 120.0f, 1.4f, white, false, "Spread.spread_jump_m : %f", m_spreadparams.spread_jump_m);
		gEnv->pRenderer->Draw2dLabel(300.0f, 130.0f, 1.4f, white, false, "Spread.spread_prone_m : %f", m_spreadparams.spread_prone_m);
		gEnv->pRenderer->Draw2dLabel(300.0f, 130.0f, 1.4f, white, false, "Spread.spread_zeroG_m : %f", m_spreadparams.spread_zeroG_m);
	}

	//---------------------------------------------------------------------------
}

void CSingle::PostUpdate(float frameTime)
{
	bool ok = false;
	bool startTarget = false;

	if (m_targetSpotSelected)
	{
		if(m_autoAimHelperTimer > 0.0f)
		{
			m_autoAimHelperTimer -= frameTime;

			if(m_autoAimHelperTimer <= 0.0f)
			{
				bool bHit = true;
				Vec3 hit = GetProbableHit(WEAPON_HIT_RANGE, &bHit);
				Vec3 pos = GetFiringPos(hit);

				if(bHit && !OutOfAmmo())
				{
					m_targetSpotSelected = true;
					m_targetSpot = hit;
					startTarget = true;
					ok = true;
				}
				else
				{
					m_targetSpot.Set(0.0f, 0.0f, 0.0f);
				}
			}
		}
		else
		{
			ok = true;
		}

		const SAmmoParams *pAmmoParams = g_pGame->GetWeaponSystem()->GetAmmoParams(GetAmmoType());

		if (pAmmoParams && ok)
		{
			if (pAmmoParams->physicalizationType != ePT_None)
			{
				float speed = pAmmoParams->speed;

				if (speed == 0.0f)
				{
					speed = 60.0f;
				}

				Vec3 hit = m_targetSpot;
				Vec3 pos = m_lastAimSpot;
				float x, y;
				Vec3 diff = hit - pos;
				y = diff.z;
				diff.z = 0;
				x = diff.GetLength();
				float angle = GetProjectileFiringAngle(speed, 9.8f, x, y);
				Matrix33 m = Matrix33::CreateRotationVDir(diff.normalize());
				m.OrthonormalizeFast();
				Ang3 aAng = RAD2DEG(Ang3::GetAnglesXYZ(m));
				aAng.x = angle;
				Ang3 ang2 = DEG2RAD(aAng);
				Matrix33 m2 = Matrix33::CreateRotationXYZ(ang2);
				Vec3 dir3 = m2.GetColumn(1);
				dir3 = dir3.normalize() * WEAPON_HIT_RANGE;
				Vec3 spot = pos + dir3;

				if(spot.z > m_targetSpot.z)
				{
					m_pWeapon->SetAimLocation(spot);
					m_pWeapon->SetTargetLocation(m_targetSpot);
				}
				else
				{
					startTarget = false;
				}
			}

			if(startTarget)
			{
				m_pWeapon->ActivateTarget(true);		//Activate Targeting on the weapon
				m_pWeapon->OnStartTargetting(m_pWeapon);
			}
		}
	}
}

void CSingle::UpdateFPView(float frameTime)
{
	if (m_targetSpotSelected)
	{
		Vec3 hit = m_targetSpot;
		m_lastAimSpot = GetFiringPos(hit);
	}
}

//------------------------------------------------------------------------
bool CSingle::IsValidAutoAimTarget(IEntity *pEntity, int partId /*= 0*/)
{
	if (pEntity->IsHidden())
	{
		return false;
	}

	AABB box;
	pEntity->GetLocalBounds(box);
	float vol = box.GetVolume();

	if (vol < m_pShared->fireparams.autoaim_minvolume || vol > m_pShared->fireparams.autoaim_maxvolume)
	{
		//CryLogAlways("volume check failed: %f", vol);
		return false;
	}

	CActor *pPlayer = m_pWeapon->GetOwnerActor();

	if (!pPlayer)
	{
		return false;
	}

	EntityId entityId = pEntity->GetId();
	IGameFramework *pGameFramework = gEnv->pGame->GetIGameFramework();

	if (IActor *pActor = pGameFramework->GetIActorSystem()->GetActor(entityId))
	{
		if (pPlayer->GetHealth() > 0.f)
		{
			if (IAIObject *pAIActor = pActor->GetEntity()->GetAI())
			{
				return pAIActor->IsHostile(pPlayer->GetEntity()->GetAI(), false);
			}
		}
	}

	if (IVehicle *pVehicle = pGameFramework->GetIVehicleSystem()->GetVehicle(entityId))
	{
		if (pVehicle->GetStatus().health > 0.f)
		{
			if (gEnv->bMultiplayer)
			{
				// Check for teams
				if (CGameRules *pGameRules = g_pGame->GetGameRules())
				{
					return (pGameRules->GetTeam(pVehicle->GetEntityId()) != pGameRules->GetTeam(pPlayer->GetEntityId()));
				}
			}
			else
			{
				if (IAIObject *pAIVehicle = pVehicle->GetEntity()->GetAI())
				{
					return pAIVehicle->IsHostile(pPlayer->GetEntity()->GetAI(), false);
				}
			}
		}
	}

	return false;
}

//------------------------------------------------------------------------
bool CSingle::CheckAutoAimTolerance(const Vec3 &aimPos, const Vec3 &aimDir)
{
	// todo: this check is probably not sufficient
	IEntity *pLocked = gEnv->pEntitySystem->GetEntity(m_lockedTarget);

	if(!pLocked)
	{
		return false;
	}

	AABB bbox;
	pLocked->GetWorldBounds(bbox);
	Vec3 targetPos = bbox.GetCenter();
	Vec3 dirToTarget = (targetPos - aimPos).normalize();
	float dot = aimDir.Dot(dirToTarget);
	Matrix33 mat = Matrix33::CreateRotationVDir(dirToTarget);
	Vec3 right = mat.GetColumn(0).normalize();
	Vec3 maxVec = (targetPos - aimPos) + (right * m_pShared->fireparams.autoaim_tolerance);
	float maxDot = dirToTarget.Dot(maxVec.normalize());
	return (dot >= maxDot);
}

void CSingle::Lock(EntityId targetId, int partId /*=0*/)
{
	m_lockedTarget = targetId;
	m_bLocking = false;
	m_bLocked = true;
	m_autoaimTimeOut = AUTOAIM_TIME_OUT;

	if (CActor *pActor = m_pWeapon->GetOwnerActor())
	{
		if (pActor->IsClient())
		{
			_smart_ptr< ISound > pBeep = gEnv->pSoundSystem->CreateSound("Sounds/interface:hud:target_lock", 0);

			if (pBeep)
			{
				pBeep->SetSemantic(eSoundSemantic_HUD);
				pBeep->Play();
			}
		}
	}
}

void CSingle::ResetLock()
{
	if (CActor *pActor = m_pWeapon->GetOwnerActor())
	{
		if ((m_bLocking || m_bLocked) && pActor->IsClient())
		{
		}
	}

	m_bLocked = false;
	m_bLocking = false;
	m_lockedTarget = 0;
	m_fStareTime = 0.0f;
	m_autoaimTimeOut = AUTOAIM_TIME_OUT;
}

void CSingle::Unlock()
{
	if (CActor *pActor = m_pWeapon->GetOwnerActor())
	{
		if (pActor->IsClient())
		{
		}
	}

	m_bLocked = false;
	m_bLocking = false;
	m_lockedTarget = 0;
	m_fStareTime = 0.0f;
	m_autoaimTimeOut = AUTOAIM_TIME_OUT;
}

void CSingle::StartLocking(EntityId targetId, int partId /*=0*/)
{
	// start locking
	m_lockedTarget = targetId;
	m_bLocking = true;
	m_bLocked = false;
	m_fStareTime = 0.0f;

	if (CActor *pActor = m_pWeapon->GetOwnerActor())
	{
		if (pActor->IsClient())
		{
		}
	}
}

//------------------------------------------------------------------------
void CSingle::UpdateAutoAim(float frameTime)
{
	static IGameObjectSystem *pGameObjectSystem = gEnv->pGame->GetIGameFramework()->GetIGameObjectSystem();
	CActor *pOwner = m_pWeapon->GetOwnerActor();

	if (!pOwner || !pOwner->IsPlayer())
	{
		return;
	}

	// todo: use crosshair/aiming dir
	IMovementController *pMC = pOwner->GetMovementController();

	if (!pMC)
	{
		return;
	}

	SMovementState state;
	pMC->GetMovementState(state);
	Vec3 aimDir = state.eyeDirection;
	Vec3 aimPos = state.eyePosition;
	float maxDistance = m_pShared->fireparams.autoaim_distance;
	ray_hit ray;
	IPhysicalEntity *pSkipEnts[10];
	int nSkipEnts = GetSkipEntities(m_pWeapon, pSkipEnts, 10);
	const int objects = ent_all;
	const int flags = (geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any | (8 & rwi_pierceability_mask) | (geom_colltype14 << rwi_colltype_bit);
	int result = gEnv->pPhysicalWorld->RayWorldIntersection(aimPos, aimDir * 2.f * maxDistance,
				 objects, flags, &ray, 1, pSkipEnts, nSkipEnts);
	bool hitValidTarget = false;
	IEntity *pEntity = 0;

	if (result && ray.pCollider)
	{
		pEntity = (IEntity *)ray.pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);

		if (pEntity && IsValidAutoAimTarget(pEntity))
		{
			hitValidTarget = true;
		}
	}

	if(m_bLocked)
	{
		m_autoaimTimeOut -= frameTime;
	}

	if (hitValidTarget && ray.dist <= maxDistance)
	{
		if (m_bLocked)
		{
			if ((m_lockedTarget != pEntity->GetId()) && m_autoaimTimeOut <= 0.0f)
			{
				StartLocking(pEntity->GetId());
			}
		}
		else
		{
			if (!m_bLocking || m_lockedTarget != pEntity->GetId())
			{
				StartLocking(pEntity->GetId());
			}
			else
			{
				m_fStareTime += frameTime;
			}
		}
	}
	else if(!hitValidTarget && m_bLocking)
	{
		m_pWeapon->RequestUnlock();
		Unlock();
	}
	else
	{
		// check if we're looking far away from our locked target
		if ((m_bLocked && !(ray.dist <= maxDistance && CheckAutoAimTolerance(aimPos, aimDir))) || (!m_bLocked && m_lockedTarget && m_fStareTime != 0.0f))
		{
			if(!m_pShared->fireparams.autoaim_timeout)
			{
				m_pWeapon->RequestUnlock();
				Unlock();
			}
		}
	}

	if (m_bLocking && !m_bLocked && m_fStareTime >= m_pShared->fireparams.autoaim_locktime && m_lockedTarget)
	{
		m_pWeapon->RequestLock(m_lockedTarget);
	}
	else if(m_bLocked && hitValidTarget && m_lockedTarget != pEntity->GetId())
	{
		m_pWeapon->RequestLock(pEntity->GetId());
	}
	else if (m_bLocked)
	{
		// check if target still valid (can e.g. be killed)
		pEntity = gEnv->pEntitySystem->GetEntity(m_lockedTarget);

		if ((pEntity && !IsValidAutoAimTarget(pEntity)) || (m_pShared->fireparams.autoaim_timeout && m_autoaimTimeOut <= 0.0f))
		{
			m_pWeapon->RequestUnlock();
			Unlock();
		}
	}
}

//------------------------------------------------------------------------
void CSingle::Release()
{
	delete this;
}

//------------------------------------------------------------------------
void CSingle::ResetParams(const IItemParamsNode *params)
{
	const IItemParamsNode *recoil = params ? params->GetChild("recoil") : 0;
	const IItemParamsNode *spread = params ? params->GetChild("spread") : 0;

	if(!m_fireParams->Valid())
	{
		const IItemParamsNode *fire = params ? params->GetChild("fire") : 0;
		const IItemParamsNode *tracer = params ? params->GetChild("tracer") : 0;
		const IItemParamsNode *ooatracer = params ? params->GetChild("outofammotracer") : 0;
		const IItemParamsNode *actions = params ? params->GetChild("actions") : 0;
		const IItemParamsNode *muzzleflash = params ? params->GetChild("muzzleflash") : 0;
		const IItemParamsNode *muzzlesmoke = params ? params->GetChild("muzzlesmoke") : 0;
		const IItemParamsNode *muzzlesmokeice = params ? params->GetChild("muzzlesmoke_ice") : 0;
		const IItemParamsNode *reject = params ? params->GetChild("reject") : 0;
		const IItemParamsNode *spinup = params ? params->GetChild("spinup") : 0;
		const IItemParamsNode *heating = params ? params->GetChild("heating") : 0;
		const IItemParamsNode *dust = params ? params->GetChild("dust") : 0;
		m_pShared->fireparams.Reset(fire);
		m_pShared->tracerparams.Reset(tracer);
		m_pShared->ooatracerparams.Reset(ooatracer);
		m_pShared->recoilparamsCopy.Reset(recoil);
		m_pShared->spreadparamsCopy.Reset(spread);
		m_pShared->actions.Reset(actions);
		m_pShared->muzzleflash.Reset(muzzleflash);
		m_pShared->muzzlesmoke.Reset(muzzlesmoke);
		m_pShared->muzzlesmokeice.Reset(muzzlesmokeice);
		m_pShared->reject.Reset(reject);
		m_pShared->spinup.Reset(spinup);
		m_pShared->heatingparams.Reset(heating);
		m_pShared->dustparams.Reset(dust);
	}

	//Still I need these ones per instance
	m_recoilparams.Reset(recoil);
	m_spreadparams.Reset(spread);
}

//------------------------------------------------------------------------
void CSingle::PatchParams(const IItemParamsNode *patch)
{
	const IItemParamsNode *recoil = patch->GetChild("recoil");
	const IItemParamsNode *spread = patch->GetChild("spread");

	if(!m_fireParams->Valid())
	{
		const IItemParamsNode *fire = patch->GetChild("fire");
		const IItemParamsNode *tracer = patch->GetChild("tracer");
		const IItemParamsNode *ooatracer = patch->GetChild("outofammotracer");
		const IItemParamsNode *actions = patch->GetChild("actions");
		const IItemParamsNode *muzzleflash = patch->GetChild("muzzleflash");
		const IItemParamsNode *muzzlesmoke = patch->GetChild("muzzlesmoke");
		const IItemParamsNode *muzzlesmokeice = patch->GetChild("muzzlesmoke_ice");
		const IItemParamsNode *reject = patch->GetChild("reject");
		const IItemParamsNode *spinup = patch->GetChild("spinup");
		const IItemParamsNode *heating = patch->GetChild("heating");
		const IItemParamsNode *dust = patch->GetChild("dust");
		m_pShared->fireparams.Reset(fire, false);
		m_pShared->tracerparams.Reset(tracer, false);
		m_pShared->ooatracerparams.Reset(ooatracer, false);
		m_pShared->recoilparamsCopy.Reset(recoil, false);
		m_pShared->spreadparamsCopy.Reset(spread, false);
		m_pShared->actions.Reset(actions, false);
		m_pShared->muzzleflash.Reset(muzzleflash, false);
		m_pShared->muzzlesmoke.Reset(muzzlesmoke, false);
		m_pShared->muzzlesmokeice.Reset(muzzlesmokeice, false);
		m_pShared->reject.Reset(reject, false);
		m_pShared->spinup.Reset(spinup, false);
		m_pShared->heatingparams.Reset(heating, false);
		m_pShared->dustparams.Reset(dust, false);
	}

	//Still I need these ones per instance
	m_recoilparams.Reset(recoil, false);
	m_spreadparams.Reset(spread, false);
	Activate(true);
}

//------------------------------------------------------------------------
void CSingle::Activate(bool activate)
{
	m_fired = m_firing = m_reloading = m_emptyclip = m_cocking = false;
	m_spinUpTime = 0.0f;
	m_next_shot = 0.0f;
	m_next_shot_dt = __fsel(-m_pShared->fireparams.rate, 0.001f, 60.0f / m_pShared->fireparams.rate);
	m_autoFireTimerLength = __fsel(-m_pShared->fireparams.auto_fire_rate, 0.001f, 60.0f / m_pShared->fireparams.auto_fire_rate);
	m_mfIds.clear();
	m_barrelId = 0;
	m_mfIds.resize(m_pShared->fireparams.barrel_count);

	if(CanOverheat() && !activate)
	{
		RestoreOverHeating(false);
	}

	m_heat = 0.0f;
	m_overheat = 0.0f;
	m_reloadPending = false;

	if(CanOverheat() && activate)
	{
		RestoreOverHeating(true);
	}

	m_pWeapon->StopSound(m_heatSoundId);
	m_heatSoundId = INVALID_SOUNDID;

	if (!activate && m_heatEffectId)
	{
		m_heatEffectId = m_pWeapon->AttachEffect(0, m_heatEffectId, false);
	}

	if (!activate && m_smokeEffectId)
	{
		m_heatEffectId = m_pWeapon->AttachEffect(0, m_smokeEffectId, false);
	}

	m_targetSpotSelected = false;
	m_reloadCancelled = false;

	if (!activate)
	{
		MuzzleFlashEffect(false);
	}

	SpinUpEffect(false);
	m_firstShot = activate;
	ResetLock();

	if (activate && m_pShared->fireparams.autoaim)
	{
		m_pWeapon->RequireUpdate(eIUS_FireMode);
	}

	m_fStareTime = 0.f;
	CActor *owner = m_pWeapon->GetOwnerActor();

	if (owner && !activate)
	{
		if (CScreenEffects *pSE = owner->GetScreenEffects())
		{
			pSE->ResetBlendGroup(CScreenEffects::eSFX_GID_ZoomIn, false);
			pSE->ResetBlendGroup(CScreenEffects::eSFX_GID_ZoomOut, false);
		}
	}

	ResetRecoil();

	if(m_pWeapon->IsZoomed())
	{
		if(activate)
		{
			if(IZoomMode *pZm = m_pWeapon->GetZoomMode(m_pWeapon->GetCurrentZoomMode()))
			{
				pZm->ApplyZoomMod(this);
			}
		}
		else
		{
			ResetRecoilMod();
			ResetSpreadMod();
		}
	}

	if (!activate)
	{
		Cancel();
	}
}

//------------------------------------------------------------------------
int CSingle::GetAmmoCount() const
{
	return m_pWeapon->GetAmmoCount(m_pShared->fireparams.ammo_type_class);
}

//------------------------------------------------------------------------
int CSingle::GetClipSize() const
{
	return m_pShared->fireparams.clip_size;
}

//------------------------------------------------------------------------
bool CSingle::OutOfAmmo() const
{
	if (m_pShared->fireparams.clip_size != 0)
	{
		return m_pShared->fireparams.ammo_type_class && m_pShared->fireparams.clip_size != -1 && m_pWeapon->GetAmmoCount(m_pShared->fireparams.ammo_type_class) < 1;
	}

	return m_pShared->fireparams.ammo_type_class && m_pWeapon->GetInventoryAmmoCount(m_pShared->fireparams.ammo_type_class) < 1;
}

//------------------------------------------------------------------------
bool CSingle::CanReload() const
{
	int clipSize = GetClipSize();
	bool isAI = m_pWeapon->GetOwnerActor() ? !m_pWeapon->GetOwnerActor()->IsPlayer() : false;

	if(m_pShared->fireparams.bullet_chamber)
	{
		clipSize += 1;
	}

	if (m_pShared->fireparams.clip_size != 0)
	{
		return !m_reloading && !m_reloadPending && (GetAmmoCount() < clipSize) && ((m_pWeapon->GetInventoryAmmoCount(m_pShared->fireparams.ammo_type_class) > 0) || (isAI));
	}

	return false;
}

bool CSingle::IsReloading(bool includePending /* =true */ )
{
	return m_reloading || m_reloadPending;
}

//------------------------------------------------------------------------
void CSingle::Reload(int zoomed)
{
	StartReload(zoomed);
}

//------------------------------------------------------------------------
bool CSingle::CanFire(bool considerAmmo) const
{
	return !m_reloading && !m_reloadPending && !m_cocking && (m_next_shot <= 0.0f) && (m_spinUpTime <= 0.0f) && (m_overheat <= 0.0f) &&
		   !m_pWeapon->IsBusy() && (!considerAmmo || !OutOfAmmo() || !m_pShared->fireparams.ammo_type_class || m_pShared->fireparams.clip_size == -1);
}

//------------------------------------------------------------------------
void CSingle::StartFire()
{
	if (m_pShared->fireparams.aim_helper && !m_targetSpotSelected)
	{
		m_targetSpotSelected = true;
		SetAutoAimHelperTimer(m_pShared->fireparams.aim_helper_delay);
		m_pWeapon->GetGameObject()->EnablePostUpdates(m_pWeapon);
		return;
	}

	if (m_pWeapon->IsBusy())
	{
		return;
	}

	if (m_pShared->fireparams.spin_up_time > 0.0f)
	{
		m_firing = true;
		m_spinUpTime = m_pShared->fireparams.spin_up_time;
		m_pWeapon->PlayAction(m_pShared->actions.spin_up);
		SpinUpEffect(true);
	}
	else
	{
		m_firing = Shoot(true);
	}

	if(m_firing && m_pShared->fireparams.auto_fire && m_pWeapon->GetOwnerActor() && m_pWeapon->GetOwnerActor()->IsPlayer())
	{
		SetAutoFireTimer(m_autoFireTimerLength);
	}

	m_pWeapon->RequireUpdate(eIUS_FireMode);
}

//------------------------------------------------------------------------
void CSingle::StopFire()
{
	if (m_targetSpotSelected)
	{
		CActor *pOwner = m_pWeapon->GetOwnerActor();

		if(pOwner && !pOwner->CanFire())
		{
			Cancel();
		}
		else
		{
			bool frozen = (pOwner && pOwner->IsFrozen());

			if (!frozen)
			{
				if (m_pShared->fireparams.spin_up_time > 0.0f)
				{
					m_firing = true;
					m_spinUpTime = m_pShared->fireparams.spin_up_time;
					m_pWeapon->PlayAction(m_pShared->actions.spin_up);
					SpinUpEffect(true);
				}
				else
				{
					m_firing = Shoot(true);
				}
			}

			m_targetSpotSelected = false;
			m_pWeapon->RequireUpdate(eIUS_FireMode);
			m_pWeapon->ActivateTarget(false);
			m_pWeapon->OnStopTargetting(m_pWeapon);
			m_pWeapon->GetGameObject()->DisablePostUpdates(m_pWeapon);
		}
	}

	if(m_pShared->fireparams.auto_fire)
	{
		SetAutoFireTimer(-1.0f);
	}

	if(m_firing)
	{
		SmokeEffect();
	}

	m_firing = false;
}

//------------------------------------------------------------------------
const char *CSingle::GetType() const
{
	return "Single";
}

//------------------------------------------------------------------------
IEntityClass *CSingle::GetAmmoType() const
{
	return m_pShared->fireparams.ammo_type_class;
}

//------------------------------------------------------------------------
float CSingle::GetSpinUpTime() const
{
	return m_pShared->fireparams.spin_up_time;
}

//------------------------------------------------------------------------
float CSingle::GetSpinDownTime() const
{
	return m_pShared->fireparams.spin_down_time;
}

//------------------------------------------------------------------------
float CSingle::GetNextShotTime() const
{
	return m_next_shot;
}

//------------------------------------------------------------------------
void CSingle::SetNextShotTime(float time)
{
	m_next_shot = time;

	if (time > 0.0f)
	{
		m_pWeapon->RequireUpdate(eIUS_FireMode);
	}
}

//------------------------------------------------------------------------
float CSingle::GetFireRate() const
{
	return m_pShared->fireparams.rate;
}

//------------------------------------------------------------------------
void CSingle::Enable(bool enable)
{
	m_enabled = enable;
}

//------------------------------------------------------------------------
bool CSingle::IsEnabled() const
{
	return m_enabled;
}

//------------------------------------------------------------------------
struct CSingle::EndReloadAction
{
	EndReloadAction(CSingle *_single, int zoomed, int reloadStartFrame):
		single(_single), _zoomed(zoomed), _reloadStartFrame(reloadStartFrame) {};

	CSingle *single;
	int _zoomed;
	int _reloadStartFrame;

	void execute(CItem *_this)
	{
		if(single->m_reloadStartFrame == _reloadStartFrame)
		{
			single->EndReload(_zoomed);
		}
	}
};

struct CSingle::StartReload_SliderBack
{
	StartReload_SliderBack(CSingle *_single): single(_single) {};
	CSingle *single;

	void execute(CItem *_this)
	{
		_this->StopLayer(single->m_pShared->fireparams.slider_layer);
	}
};

void CSingle::CancelReload()
{
	m_reloadCancelled = true;
	m_reloadPending = false;
	EndReload(0);
}

void CSingle::StartReload(int zoomed)
{
	m_reloading = true;

	if (zoomed != 0)
	{
		m_pWeapon->ExitZoom();
	}

	m_pWeapon->SetBusy(true);
	const char *action = m_pShared->actions.reload.c_str();
	IEntityClass *ammo = m_pShared->fireparams.ammo_type_class;
	m_pWeapon->OnStartReload(m_pWeapon->GetOwnerId(), ammo);
	//When interrupting reload to melee, scheduled reload action can get a bit "confused"
	//This way we can verify that the scheduled EndReloadAction matches this StartReload call...
	m_reloadStartFrame = gEnv->pRenderer->GetFrameID();

	if (m_pShared->fireparams.bullet_chamber)
	{
		int ammoCount = m_pWeapon->GetAmmoCount(ammo);

		if ((ammoCount > 0) && (ammoCount < (m_pShared->fireparams.clip_size + 1)))
		{
			action = m_pShared->actions.reload_chamber_full.c_str();
		}
		else
		{
			action = m_pShared->actions.reload_chamber_empty.c_str();
		}
	}

	m_pWeapon->PlayAction(action);
	int time = 0;

	if(m_pWeapon->IsOwnerFP())
	{
		uint32 animTime = m_pWeapon->GetCurrentAnimationTime( eIGS_FirstPerson);

		if (m_pWeapon->GetHostId() && animTime == 0)
		{
			animTime = (uint32)(m_pShared->fireparams.reload_time * 1000.0f);
		}
		else if(animTime > 1200)
		{
			animTime -= 500;    //Trigger it a bit earlier to make anim match better with the upgraded ammo count
		}

		m_pWeapon->GetScheduler()->TimerAction(animTime, CSchedulerAction<EndReloadAction>::Create(EndReloadAction(this, zoomed, m_reloadStartFrame)), false);
		time = (int)(MAX(0, ((m_pWeapon->GetCurrentAnimationTime( eIGS_FirstPerson)) - m_pShared->fireparams.slider_layer_time)));
	}
	else
	{
		m_pWeapon->GetScheduler()->TimerAction((uint32)(m_pShared->fireparams.reload_time * 1000), CSchedulerAction<EndReloadAction>::Create(EndReloadAction(this, zoomed, m_reloadStartFrame)), false);
		time = (int)(MAX(0, ((m_pShared->fireparams.reload_time * 1000) - m_pShared->fireparams.slider_layer_time)));
	}

	//Proper end reload timing for MP only (for clients, not host)
	if (gEnv->IsClient() && !gEnv->bServer)
	{
		if (CActor *pOwner = m_pWeapon->GetOwnerActor())
			if (pOwner->IsClient() && !gEnv->bServer)
			{
				m_reloadPending = true;
			}
	}

	m_pWeapon->GetScheduler()->TimerAction(time, CSchedulerAction<StartReload_SliderBack>::Create(this), false);
}

//------------------------------------------------------------------------
void CSingle::EndReload(int zoomed)
{
	m_reloading = false;
	m_emptyclip = false;
	m_spinUpTime = m_firing ? m_pShared->fireparams.spin_up_time : 0.0f;
	IEntityClass *ammo = m_pShared->fireparams.ammo_type_class;

	if (m_pWeapon->IsServer() && !m_reloadCancelled)
	{
		bool ai = m_pWeapon->GetOwnerActor() ? !m_pWeapon->GetOwnerActor()->IsPlayer() : false;
		int ammoCount = m_pWeapon->GetAmmoCount(ammo);
		int inventoryCount = m_pWeapon->GetInventoryAmmoCount(m_pShared->fireparams.ammo_type_class);
		int refill = MIN(inventoryCount, m_pShared->fireparams.clip_size - ammoCount);

		if (m_pShared->fireparams.bullet_chamber && (ammoCount > 0) && (ammoCount < m_pShared->fireparams.clip_size + 1) && ((inventoryCount - refill) > 0))
		{
			ammoCount += ++refill;
		}
		else
		{
			ammoCount += refill;
		}

		if(ai)
		{
			ammoCount = m_pShared->fireparams.clip_size;
		}

		m_pWeapon->SetAmmoCount(ammo, ammoCount);

		if (m_pWeapon->IsServer())
		{
			if ((g_pGameCVars->i_unlimitedammo == 0 && m_pShared->fireparams.max_clips != -1) && !ai)
			{
				m_pWeapon->SetInventoryAmmoCount(ammo, m_pWeapon->GetInventoryAmmoCount(ammo) - refill);
			}

			m_pWeapon->SendEndReload();
			m_pWeapon->GetGameObject()->Pulse('bang');
			g_pGame->GetIGameFramework()->GetIGameplayRecorder()->Event(m_pWeapon->GetOwner(), GameplayEvent(eGE_WeaponReload, GetAmmoType()->GetName(), refill, (void *)(m_pWeapon->GetEntityId())));
		}
	}

	m_pWeapon->OnEndReload(m_pWeapon->GetOwnerId(), ammo);
	m_reloadStartFrame = 0;
	m_reloadCancelled = false;
	m_pWeapon->SetBusy(false);
	//m_pWeapon->ForcePendingActions();
	//Do not zoom after reload
	//if (zoomed && m_pWeapon->IsSelected())
	//m_pWeapon->StartZoom(m_pWeapon->GetOwnerId(),zoomed);
}

//------------------------------------------------------------------------
struct CSingle::RezoomAction
{
	RezoomAction() {};
	void execute(CItem *pItem)
	{
		CWeapon *pWeapon = static_cast<CWeapon *>(pItem);
		IZoomMode *pIZoomMode = pWeapon->GetZoomMode(pWeapon->GetCurrentZoomMode());

		if (pIZoomMode)
		{
			CIronSight *pZoomMode = static_cast<CIronSight *>(pIZoomMode);
			pZoomMode->TurnOff(false);
		}
	}
};

struct CSingle::Shoot_SliderBack
{
	Shoot_SliderBack(CSingle *_single): pSingle(_single) {};
	CSingle *pSingle;

	void execute(CItem *pItem)
	{
		pItem->StopLayer(pSingle->m_pShared->fireparams.slider_layer);
	}
};

struct CSingle::CockAction
{
	CSingle *pSingle;
	CockAction(CSingle *_single): pSingle(_single) {};
	void execute(CItem *pItem)
	{
		pItem->PlayAction(pSingle->m_pShared->actions.cock);
		pItem->GetScheduler()->TimerAction(pItem->GetCurrentAnimationTime( eIGS_FirstPerson), CSchedulerAction<RezoomAction>::Create(), false);
		int time = MAX(0, pItem->GetCurrentAnimationTime( eIGS_FirstPerson) - pSingle->m_pShared->fireparams.slider_layer_time);
		pItem->GetScheduler()->TimerAction(time, CSchedulerAction<Shoot_SliderBack>::Create(pSingle), false);
	}
};

class CSingle::ScheduleReload
{
	public:
		ScheduleReload(CWeapon *wep)
		{
			_pWeapon = wep;
		}
		void execute(CItem *item)
		{
			_pWeapon->SetBusy(false);
			_pWeapon->Reload();
		}
	private:
		CWeapon *_pWeapon;
};

namespace
{
	struct CompareEntityDist
	{
		CompareEntityDist(const Vec3 &to) : m_to(to) {}

		ILINE bool operator()( const IEntity *lhs, const IEntity *rhs ) const
		{
			return m_to.GetSquaredDistance(lhs->GetWorldPos()) < m_to.GetSquaredDistance(rhs->GetWorldPos());
		}

		Vec3 m_to;
	};
}

bool CSingle::CrosshairAssistAiming(const Vec3 &firingPos, Vec3 &firingDir, ray_hit *pRayhit)
{
	// farcry-style crosshair-overlap aim assistance
	IEntity *pSelf = m_pWeapon->GetOwner();

	if (!pSelf )
	{
		return false;
	}

	IEntity *pEntity = pRayhit->pCollider ? gEnv->pEntitySystem->GetEntityFromPhysics(pRayhit->pCollider) : 0;

	if (pEntity && m_pWeapon->IsValidAssistTarget(pEntity, pSelf, false))
	{
		return false;
	}

	const CCamera &cam = gEnv->pRenderer->GetCamera();
	Lineseg lineseg(cam.GetPosition(), cam.GetPosition() + m_pShared->fireparams.crosshair_assist_range * cam.GetViewdir());
	float t = 0.f;
	AABB bounds;
	int debugY = 100;
	std::vector<IEntity *> ents;
	int highestIndex = 0;

	if (ents.empty())
	{
		return false;
	}

	// make sure camera is set correctly; can still be set to ortho projection by text rendering or similar (ask Andrej)
	gEnv->pRenderer->PushMatrix();
	gEnv->pRenderer->SetCamera(cam);
	const float crosshairSize = g_pGameCVars->aim_assistCrosshairSize; // should be queried from HUD, but not possible yet
	float cx = crosshairSize / gEnv->pRenderer->GetWidth() * 100.f / 2.f;
	float cy = crosshairSize / gEnv->pRenderer->GetHeight() * 100.f / 2.f;
	std::sort(ents.begin(), ents.end(), CompareEntityDist(firingPos));
	IEntity *pTarget = 0;
	Vec3 newPos, curr;

	for (int i = 0, n = ents.size(); i < n; ++i)
	{
		pEntity = ents[i];
		Vec3 wpos = pEntity->GetWorldPos();
		Quat rot = pEntity->GetWorldRotation();
		pEntity->GetLocalBounds(bounds);
		static Vec3 points[8];
		points[0] = wpos + rot * bounds.min;
		points[1] = wpos + rot * Vec3(bounds.min.x, bounds.max.y, bounds.min.z);
		points[2] = wpos + rot * Vec3(bounds.max.x, bounds.max.y, bounds.min.z);
		points[3] = wpos + rot * Vec3(bounds.max.x, bounds.min.y, bounds.min.z);
		points[4] = wpos + rot * Vec3(bounds.min.x, bounds.min.y, bounds.max.z);
		points[5] = wpos + rot * Vec3(bounds.min.x, bounds.max.y, bounds.max.z);
		points[6] = wpos + rot * Vec3(bounds.max.x, bounds.min.y, bounds.max.z);
		points[7] = wpos + rot * bounds.max;
		Vec2 smin(100, 100), smax(-100, -100);

		for (int j = 0; j < 8; ++j)
		{
			gEnv->pRenderer->ProjectToScreen(points[j].x, points[j].y, points[j].z, &curr.x, &curr.y, &curr.z);
			smin.x = min(smin.x, curr.x);
			smin.y = min(smin.y, curr.y);
			smax.x = max(smax.x, curr.x);
			smax.y = max(smax.y, curr.y);
		}

		smin.x -= 50.f;
		smin.y -= 50.f;
		smax.x -= 50.f;
		smax.y -= 50.f;
		bool xin = smin.x <= -cx && smax.x >= cx;
		bool yin = smin.y <= -cy && smax.y >= cy;
		bool overlap = (xin || abs(smin.x) <= cx || abs(smax.x) <= cx) && (yin || abs(smin.y) <= cy || abs(smax.y) <= cy);

		if (g_pGameCVars->aim_assistCrosshairDebug)
		{
			float color[] = {1, 1, 1, 1};
			gEnv->pRenderer->Draw2dLabel(100, debugY += 20, 1.3f, color, false, "%s: min (%.1f %.1f), max: (%.1f %.1f) %s", pEntity->GetName(), smin.x, smin.y, smax.x, smax.y, overlap ? "OVERLAP" : "");
		}

		if (overlap && !pTarget)
		{
			pe_status_dynamics dyn;

			if (pEntity->GetPhysics() && pEntity->GetPhysics()->GetStatus(&dyn))
			{
				newPos = dyn.centerOfMass;
			}
			else
			{
				newPos = wpos + rot * bounds.GetCenter();
			}

			// check path to new target pos
			ray_hit chkhit;
			IPhysicalEntity *pSkip = m_pWeapon->GetEntity()->GetPhysics(); // we shouldn't need all child entities for skipping at this point (subject to be proven)

			if (gEnv->pPhysicalWorld->RayWorldIntersection(firingPos, 1.1f * (newPos - firingPos), ent_all, (13 & rwi_pierceability_mask), &chkhit, 1, &pSkip, 1))
			{
				IEntity *pFound = chkhit.pCollider ? gEnv->pEntitySystem->GetEntityFromPhysics(chkhit.pCollider) : 0;

				if (pFound != pEntity)
				{
					continue;
				}
			}

			pTarget = pEntity;

			if (!g_pGameCVars->aim_assistCrosshairDebug)
			{
				break;
			}
		}
	}

	gEnv->pRenderer->PopMatrix();

	if (pTarget)
	{
		firingDir = newPos - firingPos;
		firingDir.Normalize();

		if (g_pGameCVars->aim_assistCrosshairDebug)
		{
			IPersistantDebug *pDebug = g_pGame->GetIGameFramework()->GetIPersistantDebug();
			pDebug->Begin("CHAimAssistance", false);
			pDebug->AddLine(firingPos, newPos, ColorF(0, 1, 0, 1), 0.5f);
			pDebug->AddSphere(newPos, 0.3f, ColorF(1, 0, 0, 1), 0.5f);
		}
	}

	return true;
}

struct CSingle::EndCockingAction
{
	public:
		EndCockingAction(CSingle *_single): pSingle(_single) {};
		CSingle *pSingle;

		void execute(CItem *item)
		{
			pSingle->m_cocking = false;
		}
};

bool CSingle::Shoot(bool resetAnimation, bool autoreload, bool noSound)
{
	IEntityClass *ammo = m_pShared->fireparams.ammo_type_class;
	int ammoCount = m_pWeapon->GetAmmoCount(ammo);
	CActor *pActor = m_pWeapon->GetOwnerActor();
	bool playerIsShooter = pActor ? pActor->IsPlayer() : false;
	bool clientIsShooter = pActor ? pActor->IsClient() : false;

	if (m_pShared->fireparams.clip_size == 0)
	{
		ammoCount = m_pWeapon->GetInventoryAmmoCount(ammo);
	}

	if (!CanFire(true))
	{
		if ((ammoCount <= 0) && !m_reloading && !m_reloadPending)
		{
			m_pWeapon->PlayAction(m_pShared->actions.empty_clip);
			//Auto reload
			m_pWeapon->Reload();
		}

		return false;
	}
	else if(m_pWeapon->IsWeaponLowered())
	{
		m_pWeapon->PlayAction(m_pShared->actions.null_fire);
		return false;
	}

	// Aim assistance
	m_pWeapon->AssistAiming();
	bool bHit = false;
	ray_hit rayhit;
	rayhit.pCollider = 0;
	Vec3 hit = GetProbableHit(WEAPON_HIT_RANGE, &bHit, &rayhit);
	Vec3 pos = GetFiringPos(hit);
	Vec3 dir = ApplySpread(GetFiringDir(hit, pos), GetSpread());
	Vec3 vel = GetFiringVelocity(dir);

	// Advanced aiming (VTOL Ascension)
	if(m_pShared->fireparams.advanced_AAim)
	{
		m_pWeapon->AdvancedAssistAiming(m_pShared->fireparams.advanced_AAim_Range, pos, dir);
	}

	if (!gEnv->bMultiplayer && clientIsShooter && m_pShared->fireparams.crosshair_assist_range > 0.0f && !m_pWeapon->IsZoomed())
	{
		CrosshairAssistAiming(pos, dir, &rayhit);
	}

	const char *action = m_pShared->actions.fire_cock.c_str();

	if (ammoCount == 1 || (m_pShared->fireparams.no_cock && m_pWeapon->IsZoomed()) || (m_pShared->fireparams.unzoomed_cock && m_pWeapon->IsZoomed()))
	{
		action = m_pShared->actions.fire.c_str();
	}

	int flags = CItem::eIPAF_Default | CItem::eIPAF_RestartAnimation | CItem::eIPAF_CleanBlending;

	if (m_firstShot)
	{
		m_firstShot = false;
		flags |= CItem::eIPAF_NoBlend;
	}

	flags = PlayActionSAFlags(flags);

	if(noSound)
	{
		flags &= ~CItem::eIPAF_Sound;
	}

	m_pWeapon->PlayAction(action, 0, false, flags);
	//Check for fire+cocking anim
	uint32 time = m_pWeapon->GetCurrentAnimationTime( eIGS_FirstPerson);

	if(time > 800)
	{
		m_cocking = true;
		m_pWeapon->GetScheduler()->TimerAction(time - 100, CSchedulerAction<EndCockingAction>::Create(this), false);
	}

	// debug
	static ICVar *pAimDebug = gEnv->pConsole->GetCVar("g_aimdebug");

	if (pAimDebug->GetIVal())
	{
		IPersistantDebug *pDebug = g_pGame->GetIGameFramework()->GetIPersistantDebug();
		pDebug->Begin("CSingle::Shoot", false);
		pDebug->AddSphere(hit, 0.6f, ColorF(0, 0, 1, 1), 10.f);
		pDebug->AddDirection(pos, 0.25f, dir, ColorF(0, 0, 1, 1), 1.f);
	}

	/*
		DebugShoot shoot;
		shoot.pos=pos;
		shoot.dir=dir;
		shoot.hit=hit;
		g_shoots.push_back(shoot);*/
	CheckNearMisses(hit, pos, dir, (hit - pos).len(), 1.0f);
	CProjectile *pAmmo = m_pWeapon->SpawnAmmo(ammo, false);

	if (pAmmo)
	{
		if (m_pShared->fireparams.track_projectiles)
		{
			pAmmo->SetTrackedByHUD();
		}

		CGameRules *pGameRules = g_pGame->GetGameRules();
		float damage = m_pShared->fireparams.damage;

		if(m_pShared->fireparams.secondary_damage && !playerIsShooter)
		{
			damage = m_pShared->fireparams.ai_vs_player_damage;
		}

		pAmmo->SetParams(m_pWeapon->GetOwnerId(), m_pWeapon->GetHostId(), m_pWeapon->GetEntityId(), (int)damage, pGameRules->GetHitTypeId(m_pShared->fireparams.hit_type.c_str()), playerIsShooter ? m_pShared->fireparams.damage_drop_per_meter : 0.0f, m_pShared->fireparams.damage_drop_min_distance);
		// this must be done after owner is set
		pAmmo->InitWithAI();

		if (m_bLocked)
		{
			pAmmo->SetDestination(m_lockedTarget);
		}
		else
		{
			pAmmo->SetDestination(m_pWeapon->GetDestination());
		}

		pAmmo->Launch(pos, dir, vel, m_speed_scale);
		int frequency = m_pShared->tracerparams.frequency;

		// marcok: please don't touch
		if (g_pGameCVars->bt_ironsight || g_pGameCVars->bt_speed)
		{
			frequency = 1;
		}

		bool emit = false;

		if(m_pWeapon->GetStats().fp)
		{
			emit = (!m_pShared->tracerparams.geometryFP.empty() || !m_pShared->tracerparams.effectFP.empty()) && (ammoCount == GetClipSize() || (ammoCount % frequency == 0));
		}
		else
		{
			emit = (!m_pShared->tracerparams.geometry.empty() || !m_pShared->tracerparams.effect.empty()) && (ammoCount == GetClipSize() || (ammoCount % frequency == 0));
		}

		bool ooa = ((m_pShared->fireparams.ooatracer_treshold > 0) && m_pShared->fireparams.ooatracer_treshold >= ammoCount);

		if (emit || ooa)
		{
			EmitTracer(pos, hit, ooa);
		}

		m_projectileId = pAmmo->GetEntity()->GetId();
	}

	if (playerIsShooter && pActor->IsClient())
	{
		pActor->ExtendCombat();
		CScreenEffects *pSE = pActor->GetScreenEffects();

		// Only start a zoom in if we're not zooming in or out
		if (m_pShared->fireparams.autozoom && pSE && !pSE->HasJobs(CScreenEffects::eSFX_GID_ZoomIn) && !pSE->HasJobs(CScreenEffects::eSFX_GID_ZoomOut))
		{
			IBlendType   *blend = CBlendType<CLinearBlend>::Create(CLinearBlend(1.0f));
			IBlendedEffect *fovEffect	= CBlendedEffect<CFOVEffect>::Create(CFOVEffect(pActor->GetEntityId(), 0.75f));
			pActor->GetScreenEffects()->StartBlend(fovEffect, blend, 1.0f / 5.0f, CScreenEffects::eSFX_GID_ZoomIn);
		}
	}

	if (pAmmo && pAmmo->IsPredicted() && gEnv->IsClient() && gEnv->bServer && pActor && pActor->IsClient())
	{
		pAmmo->GetGameObject()->BindToNetwork();
	}

	if (m_pWeapon->IsServer())
	{
		const char *ammoName = ammo != NULL ? ammo->GetName() : NULL;
		g_pGame->GetIGameFramework()->GetIGameplayRecorder()->Event(m_pWeapon->GetOwner(), GameplayEvent(eGE_WeaponShot, ammoName, 1, (void *)m_pWeapon->GetEntityId()));
	}

	m_pWeapon->OnShoot(m_pWeapon->GetOwnerId(), pAmmo ? pAmmo->GetEntity()->GetId() : 0, ammo, pos, dir, vel);
	MuzzleFlashEffect(true);
	//SmokeEffect();  //Only when stop firing - for Sean
	DustEffect(pos);
	RejectEffect();
	RecoilImpulse(pos, dir);
	m_fired = true;
	m_next_shot += m_next_shot_dt;
	m_zoomtimeout = m_next_shot + 0.5f;

	if (++m_barrelId == m_pShared->fireparams.barrel_count)
	{
		m_barrelId = 0;
	}

	if(g_pGameCVars->i_unlimitedammo == 0)
	{
		ammoCount--;
	}

	if(m_pShared->fireparams.fake_fire_rate && playerIsShooter )
	{
		//Hurricane fire rate fake
		ammoCount -= Random(m_pShared->fireparams.fake_fire_rate);

		if(ammoCount < 0)
		{
			ammoCount = 0;
		}
	}

	if (m_pShared->fireparams.clip_size != -1)
	{
		if (m_pShared->fireparams.clip_size != 0)
		{
			m_pWeapon->SetAmmoCount(ammo, ammoCount);
		}
		else
		{
			m_pWeapon->SetInventoryAmmoCount(ammo, ammoCount);
		}
	}

	bool dounzoomcock = (m_pWeapon->IsZoomed() && !OutOfAmmo() && m_pShared->fireparams.unzoomed_cock);

	if (!m_pShared->fireparams.slider_layer.empty() && (dounzoomcock || (ammoCount < 1)))
	{
		const char *slider_back_layer = m_pShared->fireparams.slider_layer.c_str();
		m_pWeapon->PlayLayer(slider_back_layer, CItem::eIPAF_Default | CItem::eIPAF_NoBlend);
	}

	if (dounzoomcock)
	{
		IZoomMode *pIZoomMode = m_pWeapon->GetZoomMode(m_pWeapon->GetCurrentZoomMode());

		if (pIZoomMode && pIZoomMode->IsZoomed())
		{
			CIronSight *pZoomMode = static_cast<CIronSight *>(pIZoomMode);
			pZoomMode->TurnOff(true);
			m_pWeapon->GetScheduler()->TimerAction(m_pWeapon->GetCurrentAnimationTime( eIGS_FirstPerson), CSchedulerAction<CockAction>::Create(this), false);
		}
	}

	if (OutOfAmmo())
	{
		m_pWeapon->OnOutOfAmmo(ammo);

		if (autoreload && (!pActor || pActor->IsPlayer()))
		{
			m_pWeapon->SetBusy(true);
			m_pWeapon->GetScheduler()->TimerAction(m_pWeapon->GetCurrentAnimationTime( eIGS_FirstPerson), CSchedulerAction<ScheduleReload>::Create(m_pWeapon), false);
		}
	}

	//---------------------------------------------------------------------------
	// TODO remove when aiming/fire direction is working
	// debugging aiming dir
	if(++DGB_curLimit > DGB_ShotCounter)
	{
		DGB_curLimit = DGB_ShotCounter;
	}

	if(++DGB_curIdx >= DGB_ShotCounter)
	{
		DGB_curIdx = 0;
	}

	DGB_shots[DGB_curIdx].dst = pos + dir * 200.f;
	DGB_shots[DGB_curIdx].src = pos;
	//---------------------------------------------------------------------------
	//CryLog("RequestShoot - pos(%f,%f,%f), dir(%f,%f,%f), hit(%f,%f,%f)", pos.x, pos.y, pos.z, dir.x, dir.y, dir.z, hit.x, hit.y, hit.z);
	m_pWeapon->RequestShoot(ammo, pos, dir, vel, hit, m_speed_scale, pAmmo ? pAmmo->GetGameObject()->GetPredictionHandle() : 0, false);
	return true;
}

//------------------------------------------------------------------------
bool CSingle::ShootFromHelper(const Vec3 &eyepos, const Vec3 &probableHit) const
{
	Vec3 dp(eyepos - probableHit);
	return dp.len2() > (WEAPON_HIT_MIN_DISTANCE * WEAPON_HIT_MIN_DISTANCE);
}

//------------------------------------------------------------------------
bool CSingle::HasFireHelper() const
{
	return !m_pShared->fireparams.helper[m_pWeapon->GetStats().fp ? 0 : 1].empty();
}

//------------------------------------------------------------------------
Vec3 CSingle::GetFireHelperPos() const
{
	if (HasFireHelper())
	{
		int id = m_pWeapon->GetStats().fp ? 0 : 1;
		int slot = id ? eIGS_ThirdPerson : eIGS_FirstPerson;
		return m_pWeapon->GetSlotHelperPos(slot, m_pShared->fireparams.helper[id].c_str(), true);
	}

	return Vec3(ZERO);
}

//------------------------------------------------------------------------
Vec3 CSingle::GetFireHelperDir() const
{
	if (HasFireHelper())
	{
		int id = m_pWeapon->GetStats().fp ? 0 : 1;
		int slot = id ? eIGS_ThirdPerson : eIGS_FirstPerson;
		return m_pWeapon->GetSlotHelperRotation(slot, m_pShared->fireparams.helper[id].c_str(), true).GetColumn(1);
	}

	return FORWARD_DIRECTION;
}

namespace
{
	int AddSkipPhysics(IEntity *pEntity, IPhysicalEntity **pSkipEnts, int nMaxSkip, int nSkip)
	{
		if (pEntity)
		{
			IPhysicalEntity *pPhysics = pEntity->GetPhysics();

			if (nSkip < nMaxSkip)
			{
				pSkipEnts[nSkip++] = pPhysics;
			}

			if (nSkip < nMaxSkip)
			{
				if (pPhysics && pPhysics->GetType() == PE_LIVING)
				{
					if (ICharacterInstance *pCharacter = pEntity->GetCharacter(0))
					{
						if (IPhysicalEntity *pCharPhys = pCharacter->GetISkeletonPose()->GetCharacterPhysics())
						{
							pSkipEnts[nSkip++] = pCharPhys;
						}
					}
				}
			}
		}

		return nSkip;
	}
}

//------------------------------------------------------------------------
int CSingle::GetSkipEntities(CWeapon *pWeapon, IPhysicalEntity **pSkipEnts, int nMaxSkip)
{
	int nSkip = 0;

	if (CActor *pActor = pWeapon->GetOwnerActor())
	{
		if (IVehicle *pVehicle = pActor->GetLinkedVehicle())
		{
			nSkip = AddSkipPhysics(pActor->GetEntity(), pSkipEnts, nMaxSkip, nSkip);
			// skip vehicle and all child entities
			IEntity *pVehicleEntity = pVehicle->GetEntity();

			if (nSkip < nMaxSkip)
			{
				pSkipEnts[nSkip++] = pVehicleEntity->GetPhysics();
			}

			int count = pVehicleEntity->GetChildCount();

			for (int i = 0; i < count && nSkip < nMaxSkip; ++i)
			{
				nSkip = AddSkipPhysics(pVehicleEntity->GetChild(i), pSkipEnts, nMaxSkip, nSkip);
			}
		}
		else
		{
			nSkip = AddSkipPhysics(pActor->GetEntity(), pSkipEnts, nMaxSkip, nSkip);

			if (nSkip < nMaxSkip)
			{
				if (IPhysicalEntity *pPhysics = pWeapon->GetEntity()->GetPhysics())
				{
					pSkipEnts[nSkip++] = pPhysics;
				}
			}
		}
	}

	// If this ASSERT fires, it means the passed skip array was too small.  Try increasing it.
	CRY_ASSERT_MESSAGE(nSkip < nMaxSkip, "out of space in Skip Entities array");
	return nSkip;
}

//------------------------------------------------------------------------
void CSingle::SetProjectileLaunchParams(const SProjectileLaunchParams &launchParams)
{
	if (launchParams.fSpeedScale > FLT_EPSILON)
	{
		m_speed_scale = launchParams.fSpeedScale;
	}
	else
	{
		CRY_ASSERT_MESSAGE(0, "Attempting to set a non-positive speed scale value on a projectile");
	}
}

//----------------------------------------------------
void CSingle::SetProjectileSpeedScale( float fSpeedScale )
{
	m_speed_scale = fSpeedScale;
}

void CSingle::StopPendingFire()
{
	m_firePending = false;
}

//------------------------------------------------------------------------
Vec3 CSingle::GetFireTarget() const
{
	Vec3 vResult(ZERO);
	CActor *pActor = m_pWeapon->GetOwnerActor();
	IMovementController *pMovementController = pActor ? pActor->GetMovementController() : NULL;

	if (pMovementController)
	{
		SMovementState info;
		pMovementController->GetMovementState(info);
		vResult = info.fireTarget;
	}

	return vResult;
}

//------------------------------------------------------------------------
Vec3 CSingle::GetProbableHit(float range, bool *pbHit, ray_hit *pHit) const
{
	static Vec3 pos, dir;
	static ICVar *pAimDebug = gEnv->pConsole->GetCVar("g_aimdebug");
	CActor *pActor = m_pWeapon->GetOwnerActor();
	static IPhysicalEntity *pSkipEntities[10];
	int nSkip = GetSkipEntities(m_pWeapon, pSkipEntities, 10);
	IWeaponFiringLocator *pLocator = m_pWeapon->GetFiringLocator();

	if (pLocator)
	{
		Vec3 hit;

		if (pLocator->GetProbableHit(m_pWeapon->GetEntityId(), this, hit))
		{
			return hit;
		}
	}

	IMovementController *pMC = pActor ? pActor->GetMovementController() : 0;

	if (pMC)
	{
		SMovementState info;
		pMC->GetMovementState(info);
		pos = info.weaponPosition;

		if (!pActor->IsPlayer())
		{
			if (pAimDebug->GetIVal())
			{
				//gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(e_Def3DPublicRenderflags);
				//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(info.fireTarget, 0.5f, ColorB(255,0,0,255));
			}

			dir = range * (GetFireTarget() - pos).normalized();
		}
		else
		{
			dir = range * info.fireDirection;

			// marcok: leave this alone
			if (g_pGameCVars->goc_enable && pActor->IsClient())
			{
				CPlayer *pPlayer = (CPlayer *)pActor;
				pos = pPlayer->GetViewMatrix().GetTranslation();
			}
		}
	}
	else
	{
		// fallback
		pos = GetFiringPos(Vec3Constants<float>::fVec3_Zero);
		dir = range * GetFiringDir(Vec3Constants<float>::fVec3_Zero, pos);
	}

	static ray_hit hit;
	// use the ammo's pierceability
	uint32 flags = (geom_colltype_ray | geom_colltype13) << rwi_colltype_bit | rwi_colltype_any | rwi_force_pierceable_noncoll | rwi_ignore_solid_back_faces;
	uint8 pierceability = 8;

	if (m_pShared->fireparams.ammo_type_class)
	{
		if (const SAmmoParams *pAmmoParams = g_pGame->GetWeaponSystem()->GetAmmoParams(m_pShared->fireparams.ammo_type_class))
		{
			if (pAmmoParams->pParticleParams && !is_unused(pAmmoParams->pParticleParams->iPierceability))
			{
				pierceability = pAmmoParams->pParticleParams->iPierceability;
			}
		}
	}

	flags |= pierceability;

	if (gEnv->pPhysicalWorld->RayWorldIntersection(pos, dir, ent_all, flags, &hit, 1, pSkipEntities, nSkip))
	{
		if (pbHit)
		{
			*pbHit = true;
		}

		if (pHit)
		{
			*pHit = hit;
		}

		// players in vehicles need to check position isn't behind target (since info.weaponPosition is
		//	actually the camera pos - argh...)
		if(pbHit && *pbHit && pActor && pActor->GetLinkedVehicle())
		{
			Matrix34 tm = m_pWeapon->GetEntity()->GetWorldTM();
			Vec3 wppos = tm.GetTranslation();
			{
				// find the plane perpendicular to the aim direction that passes through the weapon position
				Vec3 n = dir.GetNormalizedSafe();
				float d = -(n.Dot(wppos));
				Plane	plane(n, d);
				float dist = plane.DistFromPlane(pos);

				if(dist < 0.0f)
				{
					// now do a new intersection test forwards from the point where the previous rwi intersected the plane...
					Vec3 newPos = pos - dist * n;

					if (gEnv->pPhysicalWorld->RayWorldIntersection(newPos, dir, ent_all,
							rwi_stop_at_pierceable | rwi_ignore_back_faces, &hit, 1, pSkipEntities, nSkip))
					{
						if (pbHit)
						{
							*pbHit = true;
						}

						if (pHit)
						{
							*pHit = hit;
						}
					}
				}
			}
		}

		return hit.pt;
	}

	if (pbHit)
	{
		*pbHit = false;
	}

	return pos + dir;
}

//------------------------------------------------------------------------
Vec3 CSingle::GetFiringPos(const Vec3 &probableHit) const
{
	static Vec3 pos;
	IWeaponFiringLocator *pLocator = m_pWeapon->GetFiringLocator();

	if (pLocator)
	{
		if (pLocator->GetFiringPos(m_pWeapon->GetEntityId(), this, pos))
		{
			return pos;
		}
	}

	int id = m_pWeapon->GetStats().fp ? 0 : 1;
	int slot = id ? eIGS_ThirdPerson : eIGS_FirstPerson;
	pos = m_pWeapon->GetEntity()->GetWorldPos();
	CActor *pActor = m_pWeapon->GetOwnerActor();
	IMovementController *pMC = pActor ? pActor->GetMovementController() : 0;

	if (pMC)
	{
		SMovementState info;
		pMC->GetMovementState(info);
		pos = info.weaponPosition;

		// FIXME
		// should be getting it from MovementCotroller (same for AIProxy::QueryBodyInfo)
		// update: now AI always should be using the fire_pos from movement controller
		if (/*pActor->IsPlayer() && */(HasFireHelper() && ShootFromHelper(pos, probableHit)))
		{
			// FIXME
			// making fire pos be at eye when animation is not updated (otherwise shooting from ground)
			bool	isCharacterVisible(false);

			if (pActor)
			{
				IEntity *pEntity(pActor->GetEntity());
				ICharacterInstance *pCharacter(pEntity ? pEntity->GetCharacter(0) : NULL);

				if(pCharacter && pCharacter->IsCharacterVisible() != 0)
				{
					isCharacterVisible = true;
				}
			}

			if(isCharacterVisible)
			{
				pos = m_pWeapon->GetSlotHelperPos(slot, m_pShared->fireparams.helper[id].c_str(), true);
			}
		}
	}
	else
	{
		// when no MC, fall back to helper
		if (HasFireHelper())
		{
			pos = m_pWeapon->GetSlotHelperPos(slot, m_pShared->fireparams.helper[id].c_str(), true);
		}
	}

	return pos;
}


//------------------------------------------------------------------------
Vec3 CSingle::GetFiringDir(const Vec3 &probableHit, const Vec3 &firingPos) const
{
	static Vec3 dir;

	if (m_pShared->fireparams.autoaim && m_pShared->fireparams.autoaim_autofiringdir)
	{
		if (m_bLocked)
		{
			IEntity *pEnt = gEnv->pEntitySystem->GetEntity(m_lockedTarget);

			if (pEnt)
			{
				AABB bbox;
				pEnt->GetWorldBounds(bbox);
				Vec3 center = bbox.GetCenter();
				IActor *pAct = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_lockedTarget);

				if (pAct)
				{
					if (IMovementController *pMV = pAct->GetMovementController())
					{
						SMovementState ms;
						pMV->GetMovementState(ms);
						center = ms.eyePosition;
					}
				}

				dir = (center - firingPos).normalize();
				return dir;
			}
		}
	}

	IWeaponFiringLocator *pLocator = m_pWeapon->GetFiringLocator();

	if (pLocator)
	{
		if (pLocator->GetFiringDir(m_pWeapon->GetEntityId(), this, dir, probableHit, firingPos))
		{
			return dir;
		}
	}

	int id = m_pWeapon->GetStats().fp ? 0 : 1;
	int slot = id ? eIGS_ThirdPerson : eIGS_FirstPerson;
	dir = m_pWeapon->GetEntity()->GetWorldRotation().GetColumn1();
	CActor *pActor = m_pWeapon->GetOwnerActor();
	IMovementController *pMC = pActor ? pActor->GetMovementController() : 0;

	if (pMC)
	{
		SMovementState info;
		pMC->GetMovementState(info);
		dir = info.fireDirection;

		if (HasFireHelper() && ShootFromHelper(info.weaponPosition, probableHit))
		{
			if (!pActor->IsPlayer())
			{
				dir = (GetFireTarget() - firingPos).normalized();
			}
			else
			{
				dir = (probableHit - firingPos).normalized();
			}
		}
	}
	else
	{
		// if no MC, fall back to helper
		if (HasFireHelper())
		{
			dir = m_pWeapon->GetSlotHelperRotation(slot, m_pShared->fireparams.helper[id].c_str(), true).GetColumn(1);
		}
	}

	return dir;
}

//------------------------------------------------------------------------
Vec3 CSingle::GetFiringVelocity(const Vec3 &dir) const
{
	IWeaponFiringLocator *pLocator = m_pWeapon->GetFiringLocator();

	if (pLocator)
	{
		Vec3 vel;

		if (pLocator->GetFiringVelocity(m_pWeapon->GetEntityId(), this, vel, dir))
		{
			return vel;
		}
	}

	CActor *pActor = m_pWeapon->GetOwnerActor();

	if (pActor)
	{
		IPhysicalEntity *pPE = pActor->GetEntity()->GetPhysics();

		if (pPE)
		{
			pe_status_dynamics sv;

			if (pPE->GetStatus(&sv))
			{
				if (sv.v.len2() > 0.01f)
				{
					float dot = sv.v.GetNormalized().Dot(dir);

					if (dot < 0.0f)
					{
						dot = 0.0f;
					}

					//CryLogAlways("velocity dot: %.3f", dot);
					return sv.v * dot;
				}
			}
		}
	}

	return Vec3(0, 0, 0);
}

//------------------------------------------------------------------------
Vec3 CSingle::NetGetFiringPos(const Vec3 &probableHit) const
{
	IWeaponFiringLocator *pLocator = m_pWeapon->GetFiringLocator();

	if (pLocator)
	{
		Vec3 pos;

		if (pLocator->GetFiringPos(m_pWeapon->GetEntityId(), this, pos))
		{
			return pos;
		}
	}

	int id = m_pWeapon->GetStats().fp ? 0 : 1;
	int slot = id ? eIGS_ThirdPerson : eIGS_FirstPerson;
	Vec3 pos = m_pWeapon->GetEntity()->GetWorldPos();
	CActor *pActor = m_pWeapon->GetOwnerActor();
	IMovementController *pMC = pActor ? pActor->GetMovementController() : 0;

	if (pMC)
	{
		SMovementState info;
		pMC->GetMovementState(info);
		pos = info.weaponPosition;

		if (!m_pShared->fireparams.helper[id].empty() && ShootFromHelper(pos, probableHit))
		{
			pos = m_pWeapon->GetSlotHelperPos(slot, m_pShared->fireparams.helper[id].c_str(), true);
		}
	}
	else if (!m_pShared->fireparams.helper[id].empty())
	{
		pos = m_pWeapon->GetSlotHelperPos(slot, m_pShared->fireparams.helper[id].c_str(), true);
	}

	return pos;
}

//------------------------------------------------------------------------
Vec3 CSingle::NetGetFiringDir(const Vec3 &probableHit, const Vec3 &firingPos) const
{
	IWeaponFiringLocator *pLocator = m_pWeapon->GetFiringLocator();

	if (pLocator)
	{
		Vec3 dir;

		if (pLocator->GetFiringDir(m_pWeapon->GetEntityId(), this, dir, probableHit, firingPos))
		{
			return dir;
		}
	}

	Vec3 dir = (probableHit - firingPos).normalized();
	return dir;
}

//------------------------------------------------------------------------
Vec3 CSingle::NetGetFiringVelocity(const Vec3 &dir) const
{
	return GetFiringVelocity(dir);
}

//------------------------------------------------------------------------
Vec3 CSingle::ApplySpread(const Vec3 &dir, float spread)
{
	Ang3 angles = Ang3::GetAnglesXYZ(Matrix33::CreateRotationVDir(dir));
	float rx = Random() - 0.5f;
	float rz = Random() - 0.5f;
	angles.x += rx * DEG2RAD(spread);
	angles.z += rz * DEG2RAD(spread);
	return Matrix33::CreateRotationXYZ(angles).GetColumn(1).normalized();
}

//------------------------------------------------------------------------
Vec3 CSingle::GetTracerPos(const Vec3 &firingPos, bool ooa)
{
	int id = m_pWeapon->GetStats().fp ? 0 : 1;
	int slot = id ? eIGS_ThirdPerson : eIGS_FirstPerson;
	const char *helper = 0;

	if (ooa)
	{
		helper = m_pShared->ooatracerparams.helper[id].c_str();
	}
	else
	{
		helper = m_pShared->tracerparams.helper[id].c_str();
	}

	if (!helper[0])
	{
		return firingPos;
	}

	return m_pWeapon->GetSlotHelperPos(slot, helper, true);
}
//------------------------------------------------------------------------
void CSingle::SetupEmitters(bool attach)
{
	if (attach)
	{
		int id = m_pWeapon->GetStats().fp ? 0 : 1;
		Vec3 offset(ZERO);

		if (m_pShared->muzzleflash.helper[id].empty())
		{
			// if no helper specified, try getting pos from firing locator
			IWeaponFiringLocator *pLocator = m_pWeapon->GetFiringLocator();

			if (pLocator && pLocator->GetFiringPos(m_pWeapon->GetEntityId(), this, offset))
			{
				offset = m_pWeapon->GetEntity()->GetWorldTM().GetInvertedFast() * offset;
			}
		}

		if ((id == 0) && !m_pShared->muzzleflash.effect[0].empty())
		{
			m_mfIds[m_barrelId].mfId[0] = m_pWeapon->AttachEffect( eIGS_FirstPerson, ~0, true, m_pShared->muzzleflash.effect[0].c_str(),
										  m_pShared->muzzleflash.helper[0].c_str(), offset, Vec3Constants<float>::fVec3_OneY, 1.0f, false);
		}
		else if ((id == 1) && !m_pShared->muzzleflash.effect[1].empty())
		{
			m_mfIds[m_barrelId].mfId[1] = m_pWeapon->AttachEffect( eIGS_ThirdPerson, ~0, true, m_pShared->muzzleflash.effect[1].c_str(),
										  m_pShared->muzzleflash.helper[1].c_str(), offset, Vec3Constants<float>::fVec3_OneY, 1.0f, false);
		}
	}
	else
	{
		for (int i = 0; i < m_mfIds.size(); ++i)
		{
			m_mfIds[i].mfId[0] = m_pWeapon->AttachEffect( eIGS_FirstPerson, m_mfIds[i].mfId[0], false);
			m_mfIds[i].mfId[1] = m_pWeapon->AttachEffect( eIGS_ThirdPerson, m_mfIds[i].mfId[1], false);
		}
	}
}


//------------------------------------------------------------------------
void CSingle::MuzzleFlashEffect(bool attach, bool light, bool effect)
{
	// muzzle effects & lights are permanently attached and emitted on attach==true
	// calling with attach==false removes the emitters
	if (attach)
	{
		int slot = m_pWeapon->GetStats().fp ?  eIGS_FirstPerson :  eIGS_ThirdPerson;
		int id = m_pWeapon->GetStats().fp ? 0 : 1;

		if (light && m_pShared->muzzleflash.light_radius[id] != 0.f)
		{
			if (!m_mflightId[id] && !m_pShared->muzzleflash.light_helper[id].empty())
			{
				m_mflightId[id] = m_pWeapon->AttachLight(slot, 0, true, m_pShared->muzzleflash.light_radius[id],
								  m_pShared->muzzleflash.light_color[id], 1.0f, 0, 0, m_pShared->muzzleflash.light_helper[id].c_str());
				//m_muzzleflash.light_color[id], Vec3Constants<float>::fVec3_One, 0, 0, m_muzzleflash.light_helper[id].c_str());
			}

			m_pWeapon->EnableLight(true, m_mflightId[id]);
			m_mflTimer = m_pShared->muzzleflash.light_time[id];

			// Report muzzle flash to AI.
			if (m_pWeapon->GetOwner() && m_pWeapon->GetOwner()->GetAI())
			{
				gEnv->pAISystem->DynOmniLightEvent(m_pWeapon->GetOwner()->GetWorldPos(),
												   m_pShared->muzzleflash.light_radius[id], AILE_MUZZLE_FLASH, m_pWeapon->GetOwnerId());
			}
		}

		if (effect)
		{
			if (!m_pShared->muzzleflash.effect[id].empty() && !m_pWeapon->GetEntity()->IsHidden())
			{
				if (!m_mfIds[m_barrelId].mfId[id])
				{
					SetupEmitters(true);
				}

				IParticleEmitter *pEmitter = m_pWeapon->GetEffectEmitter(m_mfIds[m_barrelId].mfId[id]);

				if (pEmitter)
				{
					pEmitter->EmitParticle();
				}
			}
		}
	}
	else
	{
		if (light)
		{
			m_mflightId[0] = m_pWeapon->AttachLight( eIGS_FirstPerson, m_mflightId[0], false);
			m_mflightId[1] = m_pWeapon->AttachLight( eIGS_ThirdPerson, m_mflightId[1], false);
		}

		if (effect)
		{
			SetupEmitters(false);
		}
	}
}

//------------------------------------------------------------------------
class CSingle::SmokeEffectAction
{
	public:
		SmokeEffectAction(const char *_effect, const char *_helper, bool _fp, bool _attach)
		{
			helper = _helper;
			effect = _effect;
			fp = _fp;
			attach = _attach;
		}

		void execute(CItem *_item)
		{
			if (fp != _item->GetStats().fp)
			{
				return;
			}

			Vec3 dir(0, 1.0f, 0);
			CActor *pActor = _item->GetOwnerActor();
			IMovementController *pMC = pActor ? pActor->GetMovementController() : 0;

			if (pMC)
			{
				SMovementState info;
				pMC->GetMovementState(info);
				dir = info.aimDirection;
			}

			int slot = _item->GetStats().fp ?  eIGS_FirstPerson :  eIGS_ThirdPerson;
			_item->SpawnEffect(slot, effect.c_str(), helper.c_str(), Vec3(0, 0, 0), dir);
		}

		string effect;
		string helper;
		bool fp;
		bool attach;
};

void CSingle::SmokeEffect(bool effect)
{
	if (effect)
	{
		Vec3 dir(0, 1.0f, 0);
		CActor *pActor = m_pWeapon->GetOwnerActor();
		IMovementController *pMC = pActor ? pActor->GetMovementController() : 0;

		if (pMC)
		{
			SMovementState info;
			pMC->GetMovementState(info);
			dir = info.aimDirection;
		}

		int slot = m_pWeapon->GetStats().fp ?  eIGS_FirstPerson :  eIGS_ThirdPerson;
		int id = m_pWeapon->GetStats().fp ? 0 : 1;

		if(m_smokeEffectId)
		{
			m_smokeEffectId = m_pWeapon->AttachEffect(0, m_smokeEffectId, false);
		}

		if((g_pGameCVars->i_iceeffects != 0 || g_pGame->GetWeaponSystem()->IsFrozenEnvironment()) && !m_pShared->muzzlesmokeice.effect[id].empty())
		{
			m_smokeEffectId = m_pWeapon->AttachEffect(slot, ~0, true, m_pShared->muzzlesmokeice.effect[id].c_str(), m_pShared->muzzlesmokeice.helper[id].c_str());
		}
		else if (!m_pShared->muzzlesmoke.effect[id].empty())
		{
			m_smokeEffectId = m_pWeapon->AttachEffect(slot, ~0, true, m_pShared->muzzlesmoke.effect[id].c_str(), m_pShared->muzzlesmoke.helper[id].c_str());
		}
	}
}

//------------------------------------------------------------------------
void CSingle::DustEffect(const Vec3 &pos)
{
	if (!m_pShared->dustparams.mfxtag.empty())
	{
		IPhysicalEntity *pSkipEnts[10];
		int nSkip = GetSkipEntities(m_pWeapon, pSkipEnts, 10);
		ray_hit hit;

		if (gEnv->pPhysicalWorld->RayWorldIntersection(pos, m_pShared->dustparams.maxheight * Vec3(0, 0, -1), ent_static | ent_terrain | ent_water, 0, &hit, 1, pSkipEnts, nSkip))
		{
			if(IMaterialEffects *pMaterialEffects = gEnv->pGame->GetIGameFramework()->GetIMaterialEffects())
			{
				TMFXEffectId effectId = pMaterialEffects->GetEffectId(m_pShared->dustparams.mfxtag.c_str(), hit.surface_idx);

				if (effectId != InvalidEffectId)
				{
					SMFXRunTimeEffectParams params;
					params.pos = hit.pt;
					params.normal = hit.n;
					params.soundSemantic = eSoundSemantic_Player_Foley;

					if (m_pShared->dustparams.maxheightscale < 1.f)
					{
						params.scale = 1.f - (hit.dist / m_pShared->dustparams.maxheight) * (1.f - m_pShared->dustparams.maxheightscale);
					}

					pMaterialEffects->ExecuteEffect(effectId, params);
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CSingle::SpinUpEffect(bool attach)
{
	m_pWeapon->AttachEffect(0, m_suId, false);
	m_pWeapon->AttachLight(0, m_sulightId, false);
	m_suId = 0;
	m_sulightId = 0;

	if (attach)
	{
		int slot = m_pWeapon->GetStats().fp ?  eIGS_FirstPerson :  eIGS_ThirdPerson;
		int id = m_pWeapon->GetStats().fp ? 0 : 1;

		if (!m_pShared->spinup.effect[0].empty() || !m_pShared->spinup.effect[1].empty())
		{
			//CryLog("[%s] spinup effect (true)", m_pWeapon->GetEntity()->GetName());
			m_suId = m_pWeapon->AttachEffect(slot, 0, true, m_pShared->spinup.effect[id].c_str(),
											 m_pShared->spinup.helper[id].c_str(), Vec3Constants<float>::fVec3_Zero, Vec3Constants<float>::fVec3_OneY, 1.0f, false);
			//m_sulightId = m_pWeapon->AttachLight(slot, 0, true, m_spinup.light_radius[id], m_spinup.light_color[id], Vec3(1,1,1), 0, 0,
			m_sulightId = m_pWeapon->AttachLight(slot, 0, true, m_pShared->spinup.light_radius[id], m_pShared->spinup.light_color[id], 1, 0, 0,
												 m_pShared->spinup.light_helper[id].c_str());
		}

		m_suTimer = (uint32)(m_pShared->spinup.time[id]);
	}
	else
	{
		//CryLog("[%s] spinup effect (false)", m_pWeapon->GetEntity()->GetName());
	}
}

//------------------------------------------------------------------------
void CSingle::RejectEffect()
{
	if (g_pGameCVars->i_rejecteffects == 0)
	{
		return;
	}

	int slot = m_pWeapon->GetStats().fp ?  eIGS_FirstPerson :  eIGS_ThirdPerson;
	int id = m_pWeapon->GetStats().fp ? 0 : 1;

	if (!m_pShared->reject.effect[id].empty())
	{
		Vec3 front(m_pWeapon->GetEntity()->GetWorldTM().TransformVector(FORWARD_DIRECTION));
		Vec3 up(m_pWeapon->GetEntity()->GetWorldTM().TransformVector(Vec3(0.0f, 0.0f, 1.0f)));
		CActor *pActor = m_pWeapon->GetOwnerActor();
		IMovementController *pMC = pActor ? pActor->GetMovementController() : 0;

		if (pMC)
		{
			SMovementState info;
			pMC->GetMovementState(info);
			front = info.aimDirection;
			up = info.upDirection;
		}

		IActor *pClientActor = gEnv->pGame->GetIGameFramework()->GetClientActor();

		if (pClientActor)
		{
			Vec3 vPlayerPos = pClientActor->GetEntity()->GetWorldPos();
			Vec3 vEffectPos = m_pWeapon->GetEntity()->GetWorldPos();
			float fDist2 = (vPlayerPos - vEffectPos).len2();

			if (fDist2 > 25.0f * 25.0f)
			{
				return;    // too far, do not spawn physicalized empty shells and make sounds
			}
		}

		Vec3 offset = m_pShared->reject.offset;
		Vec3 dir = front.Cross(up);

		if(m_pWeapon->IsZoomed())
		{
			offset += ((front * 0.1f) + (dir * 0.04f));
		}

		float dot = fabs_tpl(front.Dot(up));
		dir += (up * (1.0f - dot) * 0.65f);
		dir.normalize();
		m_pWeapon->SpawnEffect(slot, m_pShared->reject.effect[id].c_str(), m_pShared->reject.helper[id].c_str(),
							   offset, dir, m_pShared->reject.scale[id]);
	}
}

//------------------------------------------------------------------------
int CSingle::GetDamage() const
{
	return m_pShared->fireparams.damage;
}

//------------------------------------------------------------------------
float CSingle::GetRecoil() const
{
	return m_recoil;
}

//------------------------------------------------------------------------
float CSingle::GetRecoilScale() const
{
	CActor *owner = m_pWeapon->GetOwnerActor();
	//Same as for the spread (apply stance multipliers)
	float stanceScale = 1.0f;

	if(owner)
	{
		bool inAir = owner->GetActorStats()->inAir >= 0.05f;
		bool inZeroG = owner->GetActorStats()->inZeroG;

		if (owner->GetStance() == STANCE_CROUCH && !inAir)
		{
			stanceScale = m_recoilparams.recoil_crouch_m;
		}
		else if (owner->GetStance() == STANCE_PRONE && !inAir)
		{
			stanceScale = m_recoilparams.recoil_prone_m;
		}
		else if (inAir && !inZeroG)
		{
			stanceScale = m_recoilparams.recoil_jump_m;
		}
		else if(inZeroG)
		{
			stanceScale = m_recoilparams.recoil_zeroG_m;
		}
	}

	IZoomMode *pZoomMode = m_pWeapon->GetZoomMode(m_pWeapon->GetCurrentZoomMode());

	if (!pZoomMode)
	{
		return 1.0f * m_recoilMultiplier * stanceScale;
	}

	return m_recoilMultiplier * stanceScale;
}

//------------------------------------------------------------------------
float CSingle::GetSpread() const
{
	CActor *pActor = m_pWeapon->GetOwnerActor();

	if (!pActor)
	{
		return m_spread;
	}

	// No spread for AI.
	if (m_pWeapon->GetOwner())
	{
		if (IAIObject *pAI = m_pWeapon->GetOwner()->GetAI())
			if (pAI->GetAIType() != AIOBJECT_PLAYER)
			{
				return m_spread;
			}
	}

	float stanceScale = 1.0f;
	float speedSpread = 0.0f;
	float rotationSpread = 0.0f;
	bool inAir = pActor->GetActorStats()->inAir >= 0.05f && !pActor->GetLinkedVehicle();
	bool inZeroG = pActor->GetActorStats()->inZeroG;

	if (pActor->GetStance() == STANCE_CROUCH && !inAir)
	{
		stanceScale = m_spreadparams.spread_crouch_m;
	}
	else if (pActor->GetStance() == STANCE_PRONE && !inAir)
	{
		stanceScale = m_spreadparams.spread_prone_m;
	}
	else if (inAir && !inZeroG)
	{
		stanceScale = m_spreadparams.spread_jump_m;
	}
	else if (inZeroG)
	{
		stanceScale = m_spreadparams.spread_zeroG_m;
	}

	pe_status_dynamics dyn;
	IPhysicalEntity *pPhysicalEntity = pActor->GetEntity()->GetPhysics();

	if (pPhysicalEntity && pPhysicalEntity->GetStatus(&dyn))
	{
		speedSpread = dyn.v.len() * m_spreadparams.speed_m;
		rotationSpread = dyn.w.len() * m_spreadparams.rotation_m;
		rotationSpread = CLAMP(rotationSpread, 0.0f, 3.0f);
	}

	IZoomMode *pZoomMode = m_pWeapon->GetZoomMode(m_pWeapon->GetCurrentZoomMode());

	if (!pZoomMode)
	{
		return (speedSpread + rotationSpread + m_spread) * stanceScale;
	}

	return (speedSpread + rotationSpread + m_spread) * stanceScale;
	//return (speedSpread+m_spread)*stanceScale;
}

//------------------------------------------------------------------------
float CSingle::GetMinSpread() const
{
	return m_spreadparams.min;
}

//------------------------------------------------------------------------
float CSingle::GetMaxSpread() const
{
	return m_spreadparams.max;
}

//------------------------------------------------------------------------
const char *CSingle::GetCrosshair() const
{
	return m_pShared->fireparams.crosshair.c_str();
}

//------------------------------------------------------------------------
float CSingle::GetHeat() const
{
	return m_heat;
}

//------------------------------------------------------------------------
void CSingle::UpdateHeat(float frameTime)
{
	CActor *pActor = m_pWeapon->GetOwnerActor();
	IAIObject *pOwnerAI = (pActor && pActor->GetEntity()) ? pActor->GetEntity()->GetAI() : 0;
	bool isOwnerAIControlled = pOwnerAI && pOwnerAI->GetAIType() != AIOBJECT_PLAYER;

	if(isOwnerAIControlled)
	{
		return;
	}

	float oldheat = m_heat;

	if (CanOverheat())
	{
		if (m_overheat > 0.0f)
		{
			m_overheat -= frameTime;

			if (m_overheat <= 0.0f)
			{
				m_overheat = 0.0f;
				m_pWeapon->StopSound(m_heatSoundId);
				m_pWeapon->PlayAction(m_pShared->actions.cooldown);
			}
		}
		else
		{
			float add = 0.0f;
			float sub = 0.0f;

			if (m_fired)
			{
				add = m_pShared->heatingparams.attack;
			}
			else if (m_next_shot <= 0.0001f)
			{
				sub = frameTime / m_pShared->heatingparams.decay;
			}

			m_heat += add - sub;
			m_heat = CLAMP(m_heat, 0.0f, 1.0f);
			static ICVar *pAimDebug = gEnv->pConsole->GetCVar("g_aimdebug");

			if (pAimDebug && pAimDebug->GetIVal() > 1)
			{
				float color[] = {1, 1, 1, 1};
				gEnv->pRenderer->Draw2dLabel(300, 300, 1.2f, color, false, "    + %.2f", add);
				gEnv->pRenderer->Draw2dLabel(300, 315, 1.2f, color, false, "    - %.2f", sub);
				gEnv->pRenderer->Draw2dLabel(300, 335, 1.3f, color, false, "heat: %.2f", m_heat);
			}

			if (m_heat >= 0.999f && oldheat < 0.999f)
			{
				m_overheat = m_pShared->heatingparams.duration;
				StopFire();
				m_heatSoundId = m_pWeapon->PlayAction(m_pShared->actions.overheating);
				int slot = m_pWeapon->GetStats().fp ? eIGS_FirstPerson : eIGS_ThirdPerson;

				if (!m_pShared->heatingparams.effect[slot].empty())
				{
					if (m_heatEffectId)
					{
						m_heatEffectId = m_pWeapon->AttachEffect(0, m_heatEffectId, false);
					}

					m_heatEffectId = m_pWeapon->AttachEffect(slot, 0, true, m_pShared->heatingparams.effect[slot].c_str(), m_pShared->heatingparams.helper[slot].c_str());
				}
			}
		}

		if (m_heat >= 0.0001f)
		{
			m_pWeapon->RequireUpdate(eIUS_FireMode);
		}
	}
}

//------------------------------------------------------------------------
void CSingle::UpdateRecoil(float frameTime)
{
	//float white[4]={1,1,1,1};
	// spread
	float spread_add = 0.0f;
	float spread_sub = 0.0f;
	bool dw = m_pWeapon->IsDualWield();
	CActor *pActor = m_pWeapon->GetOwnerActor();
	IAIObject *pOwnerAI = (pActor && pActor->GetEntity()) ? pActor->GetEntity()->GetAI() : 0;
	bool isOwnerAIControlled = pOwnerAI && pOwnerAI->GetAIType() != AIOBJECT_PLAYER;

	if(isOwnerAIControlled)
	{
		// The AI system will offset the shoot target in any case, so do not apply spread.
		m_spread = 0.0f;
	}
	else
	{
		if (m_spreadparams.attack > 0.0f)
		{
			// shot
			float attack = m_spreadparams.attack;

			if (dw) // experimental recoil increase for dual wield
			{
				attack *= 1.20f;
			}

			if (m_fired)
			{
				spread_add = m_spreadparams.attack;
			}

			float decay = m_recoilparams.decay;

			if (dw) // experimental recoil increase for dual wield
			{
				decay *= 1.25f;
			}

			spread_sub = frameTime * (m_spreadparams.max - m_spreadparams.min) / decay;
			m_spread += (spread_add - spread_sub) * m_recoilMultiplier;
			m_spread = CLAMP(m_spread, m_spreadparams.min, m_spreadparams.max * m_recoilMultiplier);
			//gEnv->pRenderer->Draw2dLabel(50,75,2.0f,white,false,"Current Spread: %f", m_spread);
		}
		else
		{
			m_spread = m_spreadparams.min;
		}
	}

	float strenghtScale = 1.0f;
	//recoil
	float scale = GetRecoilScale();
	float recoil_add = 0.0f;
	float recoil_sub = 0.0f;

	if (m_recoilparams.decay > 0.0f)
	{
		if (m_fired)
		{
			float attack = m_recoilparams.attack;

			if (dw) // experimental recoil increase for dual wield
			{
				attack *= 1.20f;
			}

			recoil_add = attack * scale * strenghtScale;
			//CryLogAlways("recoil scale: %.3f", scale);

			if (pActor && m_pWeapon->ApplyActorRecoil())
			{
				SGameObjectEvent e(eCGE_Recoil, eGOEF_ToExtensions);
				e.param = (void *)(&recoil_add);
				pActor->HandleEvent(e);
			}
		}

		float decay = m_recoilparams.decay;

		if (dw) // experimental recoil increase for dual wield
		{
			decay *= 1.25f;
		}

		recoil_sub = frameTime * m_recoilparams.max_recoil / decay;
		recoil_sub *= scale;

		if(m_fired)
		{
			recoil_sub *= strenghtScale;
		}

		m_recoil += recoil_add - recoil_sub;
		m_recoil = CLAMP(m_recoil, 0.0f, m_recoilparams.max_recoil * m_recoilMultiplier);
		//gEnv->pRenderer->Draw2dLabel(50,50,2.0f,white,false,"Current recoil: %f", m_recoil);
	}
	else
	{
		m_recoil = 0.0f;
	}

	if ((m_recoilparams.max.x > 0) || (m_recoilparams.max.y > 0))
	{
		Vec2 recoil_dir_add(0.0f, 0.0f);

		if (m_fired)
		{
			int n = m_recoilparams.hints.size();

			if (m_recoil_dir_idx >= 0 && m_recoil_dir_idx < n)
			{
				recoil_dir_add = m_recoilparams.hints[m_recoil_dir_idx];

				if (m_recoil_dir_idx + 1 >= n)
				{
					if (m_recoilparams.hint_loop_start < n)
					{
						m_recoil_dir_idx = m_recoilparams.hint_loop_start;
					}
					else
					{
						m_recoil_dir_idx = 0;
					}
				}
				else
				{
					++m_recoil_dir_idx;
				}
			}
		}

		CActor *pOwner = m_pWeapon->GetOwnerActor();
		CItem  *pMaster = NULL;

		if(m_pWeapon->IsDualWieldSlave())
		{
			pMaster = static_cast<CItem *>(m_pWeapon->GetDualWieldMaster());
		}

		if (pOwner && (m_pWeapon->IsCurrentItem() || (pMaster && pMaster->IsCurrentItem())))
		{
			if (m_fired)
			{
				Vec2 rdir(Random()*m_recoilparams.randomness, BiRandom(m_recoilparams.randomness));
				m_recoil_dir = Vec2(recoil_dir_add.x + rdir.x, recoil_dir_add.y + rdir.y);
				m_recoil_dir.NormalizeSafe();
			}

			if (m_recoil > 0.001f)
			{
				float t = m_recoil / m_recoilparams.max_recoil;
				Vec2 new_offset = Vec2(m_recoil_dir.x * m_recoilparams.max.x, m_recoil_dir.y * m_recoilparams.max.y) * t * 3.141592f / 180.0f;
				m_recoil_offset = new_offset * 0.66f + m_recoil_offset * 0.33f;
				pOwner->SetViewAngleOffset(Vec3(m_recoil_offset.x, 0.0f, m_recoil_offset.y));
				m_pWeapon->RequireUpdate(eIUS_FireMode);
			}
			else
			{
				ResetRecoil(false);
			}
		}
	}
	else
	{
		ResetRecoil();
	}

	/*g_shoots.clear();
	for (std::vector<DebugShoot>::iterator it=g_shoots.begin(); g_shoots.end() != it; it++)
	{
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(it->pos, ColorB(200, 200, 0), it->hit, ColorB(200, 200, 0) );
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(it->pos, ColorB(0, 200, 0), it->pos+it->dir*((it->hit-it->pos).len()), ColorB(0, 200, 0) );

		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(it->pos, 0.125, ColorB(200, 0, 0));
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(it->hit, 0.125, ColorB(0, 0, 200));
	}*/
}

//------------------------------------------------------------------------
void CSingle::ResetRecoil(bool spread)
{
	m_recoil = 0.0f;
	m_recoil_dir_idx = 0;
	m_recoil_dir = Vec2(0.0f, 0.0f);
	m_recoil_offset = Vec2(0.0f, 0.0f);

	if (spread)
	{
		m_spread = m_spreadparams.min;
	}

	CActor *pOwner = m_pWeapon->GetOwnerActor();

	if (pOwner && m_pWeapon->IsCurrentItem())
	{
		pOwner->SetViewAngleOffset(Vec3(0.0f, 0.0f, 0.0f));
	}
}

//------------------------------------------------------------------------
void CSingle::NetShoot(const Vec3 &hit, int predictionHandle)
{
	bool bHit = false;
	ray_hit rayhit;
	rayhit.pCollider = 0;
	Vec3 probableHit = GetProbableHit(WEAPON_HIT_RANGE, &bHit, &rayhit);
	Vec3 pos = NetGetFiringPos(probableHit);
	Vec3 dir = ApplySpread(NetGetFiringDir(probableHit, pos), GetSpread());
	Vec3 vel = NetGetFiringVelocity(dir);
	NetShootEx(pos, dir, vel, probableHit, 1.0f, predictionHandle);
}

//------------------------------------------------------------------------
void CSingle::NetShootEx(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int predictionHandle)
{
	IEntityClass *ammo = m_pShared->fireparams.ammo_type_class;
	int ammoCount = m_pWeapon->GetAmmoCount(ammo);
	CActor *pActor = m_pWeapon->GetOwnerActor();
	bool playerIsShooter = pActor ? pActor->IsPlayer() : false;

	if (m_pShared->fireparams.clip_size == 0)
	{
		ammoCount = m_pWeapon->GetInventoryAmmoCount(ammo);
	}

	const char *action = m_pShared->actions.fire_cock.c_str();

	if (ammoCount == 1)
	{
		action = m_pShared->actions.fire.c_str();
	}

	m_pWeapon->ResetAnimation();
	int flags = CItem::eIPAF_Default | CItem::eIPAF_NoBlend;
	flags = PlayActionSAFlags(flags);
	m_pWeapon->PlayAction(action, 0, false, flags);
	CProjectile *pAmmo = m_pWeapon->SpawnAmmo(ammo, true);

	if (pAmmo)
	{
		int hitTypeId = g_pGame->GetGameRules()->GetHitTypeId(m_pShared->fireparams.hit_type.c_str());
		pAmmo->SetParams(m_pWeapon->GetOwnerId(), m_pWeapon->GetHostId(), m_pWeapon->GetEntityId(), m_pShared->fireparams.damage, hitTypeId, playerIsShooter ? m_pShared->fireparams.damage_drop_per_meter : 0.0f, m_pShared->fireparams.damage_drop_min_distance);

		if (m_bLocked)
		{
			pAmmo->SetDestination(m_lockedTarget);
		}
		else
		{
			pAmmo->SetDestination(m_pWeapon->GetDestination());
		}

		pAmmo->SetRemote(true);
		m_speed_scale = extra;
		pAmmo->Launch(pos, dir, vel, m_speed_scale);
		bool emit = (!m_pShared->tracerparams.geometry.empty() || !m_pShared->tracerparams.effect.empty()) && (ammoCount == GetClipSize() || (ammoCount % m_pShared->tracerparams.frequency == 0));
		bool ooa = ((m_pShared->fireparams.ooatracer_treshold > 0) && m_pShared->fireparams.ooatracer_treshold >= ammoCount);

		if (emit || ooa)
		{
			EmitTracer(pos, hit, ooa);
		}

		m_projectileId = pAmmo->GetEntity()->GetId();
	}

	if (m_pWeapon->IsServer())
	{
		const char *ammoName = ammo != NULL ? ammo->GetName() : NULL;
		g_pGame->GetIGameFramework()->GetIGameplayRecorder()->Event(m_pWeapon->GetOwner(), GameplayEvent(eGE_WeaponShot, ammoName, 1, (void *)m_pWeapon->GetEntityId()));
	}

	m_pWeapon->OnShoot(m_pWeapon->GetOwnerId(), pAmmo ? pAmmo->GetEntity()->GetId() : 0, ammo, pos, dir, vel);
	MuzzleFlashEffect(true);
	DustEffect(pos);
	RejectEffect();
	RecoilImpulse(pos, dir);
	m_fired = true;
	m_next_shot = 0.0f;

	if (++m_barrelId == m_pShared->fireparams.barrel_count)
	{
		m_barrelId = 0;
	}

	ammoCount--;

	/*
	if(m_pShared->fireparams.clip_size!=-1) //Don't trigger the assert in this case
	{
		assert(ammoCount>=0);
	}
	*/
	if(ammoCount < 0)
	{
		ammoCount = 0;
	}

	if(m_pShared->fireparams.fake_fire_rate && playerIsShooter )
	{
		//Hurricane fire rate fake
		ammoCount -= Random(m_pShared->fireparams.fake_fire_rate);
	}

	if (m_pWeapon->IsServer())
	{
		if (m_pShared->fireparams.clip_size != -1)
		{
			if (m_pShared->fireparams.clip_size != 0)
			{
				m_pWeapon->SetAmmoCount(ammo, ammoCount);
			}
			else
			{
				m_pWeapon->SetInventoryAmmoCount(ammo, ammoCount);
			}
		}
	}

	if ((ammoCount < 1) && !m_pShared->fireparams.slider_layer.empty())
	{
		const char *slider_back_layer = m_pShared->fireparams.slider_layer.c_str();
		m_pWeapon->PlayLayer(slider_back_layer, CItem::eIPAF_Default | CItem::eIPAF_NoBlend);
	}

	if (OutOfAmmo())
	{
		m_pWeapon->OnOutOfAmmo(ammo);
	}

	if (pAmmo && predictionHandle && pActor)
	{
		pAmmo->GetGameObject()->RegisterAsValidated(pActor->GetGameObject(), predictionHandle);
		pAmmo->GetGameObject()->BindToNetwork();
	}
	else if (pAmmo && pAmmo->IsPredicted() && gEnv->IsClient() && gEnv->bServer && pActor && pActor->IsClient())
	{
		pAmmo->GetGameObject()->BindToNetwork();
	}

	m_pWeapon->RequireUpdate(eIUS_FireMode);
}

//------------------------------------------------------------------------
void CSingle::RecoilImpulse(const Vec3 &firingPos, const Vec3 &firingDir)
{
	// todo: integrate the impulse params when time..
	if (m_recoilparams.impulse > 0.f)
	{
		EntityId id = (m_pWeapon->GetHostId()) ? m_pWeapon->GetHostId() : m_pWeapon->GetOwnerId();
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(id);

		if (pEntity && pEntity->GetPhysics())
		{
			pe_action_impulse impulse;
			impulse.impulse = -firingDir * m_recoilparams.impulse;
			impulse.point = firingPos;
			pEntity->GetPhysics()->Action(&impulse);
		}
	}

	CActor *pActor = m_pWeapon->GetOwnerActor();

	if (pActor && pActor->IsPlayer())
	{
		if(pActor->InZeroG())
		{
			IEntityPhysicalProxy *pPhysicsProxy = (IEntityPhysicalProxy *)pActor->GetEntity()->GetProxy(ENTITY_PROXY_PHYSICS);
			SMovementState ms;
			pActor->GetMovementController()->GetMovementState(ms);
			CPlayer *plr = (CPlayer *)pActor;

			if(m_recoilparams.back_impulse > 0.0f)
			{
				Vec3 impulseDir = ms.aimDirection * -1.0f;
				Vec3 impulsePos = ms.pos;
				float impulse = m_recoilparams.back_impulse;
				pPhysicsProxy->AddImpulse(-1, impulsePos, impulseDir * impulse * 100.0f, true, 1.0f);
			}
		}

		if(pActor->IsClient())
			if (gEnv->pInput)
			{
				gEnv->pInput->ForceFeedbackEvent( SFFOutputEvent(eDI_XI, eFF_Rumble_Basic, 0.05f, 0.0f, max(0.35f, fabsf(m_recoilparams.back_impulse) * 2.0f) ));
			}
	}
}

//------------------------------------------------------------------------
void CSingle::CheckNearMisses(const Vec3 &probableHit, const Vec3 &pos, const Vec3 &dir, float range, float radius)
{
	FUNCTION_PROFILER( GetISystem(), PROFILE_GAME );

	if (!gEnv->pAISystem)
	{
		return;
	}

	if (!m_pWeapon->GetOwnerId())
	{
		return;
	}

	// Associate event with vehicle if the shooter is in a vehicle (tank cannon shot, etc)
	EntityId ownerId = m_pWeapon->GetOwnerId();
	IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(ownerId);

	if (pActor && pActor->GetLinkedVehicle() && pActor->GetLinkedVehicle()->GetEntityId())
	{
		ownerId = pActor->GetLinkedVehicle()->GetEntityId();
	}

	SAIStimulus stim(AISTIM_BULLET_WHIZZ, 0, ownerId, 0, pos, dir, range);
	gEnv->pAISystem->RegisterStimulus(stim);
}

//------------------------------------------------------------------------
void CSingle::CacheTracer()
{
	if (!m_pShared->tracerparams.geometry.empty())
	{
		IStatObj *pStatObj = gEnv->p3DEngine->LoadStatObj(m_pShared->tracerparams.geometry.c_str());

		if (pStatObj)
		{
			pStatObj->AddRef();
			m_tracerCache.push_back(pStatObj);
		}
	}

	if (!m_pShared->tracerparams.geometryFP.empty() && (m_pShared->tracerparams.geometryFP != m_pShared->tracerparams.geometry))
	{
		IStatObj *pStatObjFP = gEnv->p3DEngine->LoadStatObj(m_pShared->tracerparams.geometryFP.c_str());

		if (pStatObjFP)
		{
			pStatObjFP->AddRef();
			m_tracerCache.push_back(pStatObjFP);
		}
	}

	if (!m_pShared->ooatracerparams.geometry.empty())
	{
		IStatObj *pStatObj = gEnv->p3DEngine->LoadStatObj(m_pShared->ooatracerparams.geometry.c_str());

		if (pStatObj)
		{
			pStatObj->AddRef();
			m_tracerCache.push_back(pStatObj);
		}
	}

	if (!m_pShared->ooatracerparams.geometryFP.empty() && (m_pShared->ooatracerparams.geometryFP != m_pShared->ooatracerparams.geometry))
	{
		IStatObj *pStatObjFP = gEnv->p3DEngine->LoadStatObj(m_pShared->ooatracerparams.geometryFP.c_str());

		if (pStatObjFP)
		{
			pStatObjFP->AddRef();
			m_tracerCache.push_back(pStatObjFP);
		}
	}
}

//-----------------------------------------------------------------------
void CSingle::CacheAmmoGeometry()
{
	if(m_pShared->fireparams.ammo_type_class)
	{
		if(const SAmmoParams *pAmmoParams = g_pGame->GetWeaponSystem()->GetAmmoParams(m_pShared->fireparams.ammo_type_class))
		{
			pAmmoParams->CacheGeometry();
		}
	}
}

//------------------------------------------------------------------------
void CSingle::ClearTracerCache()
{
	for (std::vector<IStatObj *>::iterator it = m_tracerCache.begin(); it != m_tracerCache.end(); ++it)
	{
		(*it)->Release();
	}

	m_tracerCache.resize(0);
}
//------------------------------------------------------------------------
void CSingle::SetName(const char *name)
{
	m_name = name;
}
//------------------------------------------------------------------------
float CSingle::GetProjectileFiringAngle(float v, float g, float x, float y)
{
	float angle = 0.0, t, a;
	// Avoid square root in script
	float d = cry_sqrtf(max(0.0f, powf(v, 4) - 2 * g * y * powf(v, 2) - powf(g, 2) * powf(x, 2)));

	if(d >= 0)
	{
		a = powf(v, 2) - g * y;

		if (a - d > 0)
		{
			t = cry_sqrtf(2 * (a - d) / powf(g, 2));
			angle = (float)acos_tpl(x / (v * t));
			float y_test;
			y_test = float(-v * sin_tpl(angle) * t - g * powf(t, 2) / 2);

			if (fabsf(y - y_test) < 0.02f)
			{
				return RAD2DEG(-angle);
			}

			y_test = float(v * sin_tpl(angle) * t - g * pow(t, 2) / 2);

			if (fabsf(y - y_test) < 0.02f)
			{
				return RAD2DEG(angle);
			}
		}

		t = cry_sqrtf(2 * (a + d) / powf(g, 2));
		angle = (float)acos_tpl(x / (v * t));
		float y_test = float(v * sin_tpl(angle) * t - g * pow(t, 2) / 2);

		if (fabsf(y - y_test) < 0.02f)
		{
			return RAD2DEG(angle);
		}

		return 0;
	}

	return 0;
}
//------------------------------------------------------------------------
void CSingle::Cancel()
{
	if(m_targetSpotSelected)
	{
		m_pWeapon->ActivateTarget(false);
		m_pWeapon->OnStopTargetting(m_pWeapon);
		m_pWeapon->GetGameObject()->DisablePostUpdates(m_pWeapon);
	}

	m_targetSpotSelected = false;
}
//------------------------------------------------------------------------
bool CSingle::AllowZoom() const
{
	return !m_targetSpotSelected && !m_firing && !m_cocking;
}
//------------------------------------------------------------------------

void CSingle::GetMemoryUsage(ICrySizer *s) const
{
	s->Add(*this);
	s->AddContainer(m_tracerCache);

	if(m_useCustomParams)
	{
		m_pShared->fireparams.GetMemoryUsage(s);
		m_pShared->tracerparams.GetMemoryUsage(s);
		m_pShared->ooatracerparams.GetMemoryUsage(s);
		m_pShared->actions.GetMemoryUsage(s);
		m_pShared->muzzleflash.GetMemoryUsage(s);
		m_pShared->muzzlesmoke.GetMemoryUsage(s);
		m_pShared->muzzlesmokeice.GetMemoryUsage(s);
		m_pShared->reject.GetMemoryUsage(s);
		m_pShared->spinup.GetMemoryUsage(s);
		m_pShared->recoilparamsCopy.GetMemoryUsage(s);
		m_pShared->spreadparamsCopy.GetMemoryUsage(s);
		m_pShared->heatingparams.GetMemoryUsage(s);
		m_pShared->dustparams.GetMemoryUsage(s);
	}

	s->Add(m_name);
	m_spreadparams.GetMemoryUsage(s);
	m_recoilparams.GetMemoryUsage(s);
}


//-------------------------------------------------------------------------------
//PatchSpreadMod(SSpreadModParams &sSMP)
//
// - sSMP - Some multipliers to modify the spread per value

void CSingle::PatchSpreadMod(const SSpreadModParams &sSMP)
{
	m_spreadparams.attack			*= sSMP.attack_mod;
	m_spreadparams.decay			*= sSMP.decay_mod;
	m_spreadparams.max				*= sSMP.max_mod;
	m_spreadparams.min				*= sSMP.min_mod;
	m_spreadparams.rotation_m *= sSMP.rotation_m_mod;
	m_spreadparams.speed_m    *= sSMP.speed_m_mod;
	m_spreadparams.spread_crouch_m *= sSMP.spread_crouch_m_mod;
	m_spreadparams.spread_jump_m   *= sSMP.spread_jump_m_mod;
	m_spreadparams.spread_prone_m  *= sSMP.spread_prone_m_mod;
	m_spreadparams.spread_zeroG_m	 *= sSMP.spread_zeroG_m_mod;
	//Reset spread
	m_spread = m_spreadparams.min;
}

//-------------------------------------------------------------------------------
//ResetSpreadMod()
//
// Restore initial values

void CSingle::ResetSpreadMod()
{
	m_spreadparams = m_pShared->spreadparamsCopy;
	//Reset min spread too
	m_spread = m_spreadparams.min;
}

//-------------------------------------------------------------------------------
//PatchRecoilMod(SRecoilModParams &sRMP)
//
// - sRMP - Some multipliers to modify the recoil per value

void CSingle::PatchRecoilMod(const SRecoilModParams &sRMP)
{
	m_recoilparams.angular_impulse					*= sRMP.angular_impulse_mod;
	m_recoilparams.attack										*= sRMP.attack_mod;
	m_recoilparams.back_impulse							*= sRMP.back_impulse_mod;
	m_recoilparams.decay										*= sRMP.decay_mod;
	m_recoilparams.impulse									*= sRMP.impulse_mod;
	m_recoilparams.max.x										*= sRMP.max_mod.x;
	m_recoilparams.max.y										*= sRMP.max_mod.y;
	m_recoilparams.max_recoil								*= sRMP.max_recoil_mod;
	m_recoilparams.recoil_crouch_m					*= sRMP.recoil_crouch_m_mod;
	m_recoilparams.recoil_jump_m						*= sRMP.recoil_jump_m_mod;
	m_recoilparams.recoil_prone_m						*= sRMP.recoil_prone_m_mod;
	m_recoilparams.recoil_strMode_m					*= sRMP.recoil_strMode_m_mod;
	m_recoilparams.recoil_zeroG_m						*= sRMP.recoil_zeroG_m_mod;
}

//-------------------------------------------------------------------------------
//ResetRecoilMod()
//
// Restore initial values

void CSingle::ResetRecoilMod()
{
	m_recoilparams.angular_impulse					= m_pShared->recoilparamsCopy.angular_impulse;
	m_recoilparams.attack										= m_pShared->recoilparamsCopy.attack;
	m_recoilparams.back_impulse							= m_pShared->recoilparamsCopy.back_impulse;
	m_recoilparams.decay										= m_pShared->recoilparamsCopy.decay;
	m_recoilparams.impulse									= m_pShared->recoilparamsCopy.impulse;
	m_recoilparams.max											= m_pShared->recoilparamsCopy.max;
	m_recoilparams.max_recoil								= m_pShared->recoilparamsCopy.max_recoil;
	m_recoilparams.recoil_crouch_m					= m_pShared->recoilparamsCopy.recoil_crouch_m;
	m_recoilparams.recoil_jump_m						= m_pShared->recoilparamsCopy.recoil_jump_m;
	m_recoilparams.recoil_prone_m						= m_pShared->recoilparamsCopy.recoil_prone_m;
	m_recoilparams.recoil_strMode_m					= m_pShared->recoilparamsCopy.recoil_strMode_m;
	m_recoilparams.recoil_zeroG_m						= m_pShared->recoilparamsCopy.recoil_zeroG_m;
}

//-----------------------------------------------------------------------------
EntityId CSingle::RemoveProjectileId()
{
	EntityId ret = m_projectileId;
	m_projectileId = 0;
	return ret;
}
//----------------------------------------------------------------------------------
void CSingle::AutoFire()
{
	if(!m_pWeapon->IsDualWield())
	{
		Shoot(true);
	}
	else
	{
		if(m_pWeapon->IsDualWieldMaster())
		{
			IItem *slave = m_pWeapon->GetDualWieldSlave();

			if(slave && slave->GetIWeapon())
			{
				CWeapon *dualWieldSlave = static_cast<CWeapon *>(slave);
				{
					if(!dualWieldSlave->IsWeaponRaised())
					{
						m_pWeapon->StopFire();
						m_pWeapon->SetFireAlternation(!m_pWeapon->GetFireAlternation());
						dualWieldSlave->StartFire();
					}
					else
					{
						Shoot(true);
					}
				}
			}
		}
		else if(m_pWeapon->IsDualWieldSlave())
		{
			IItem *master = m_pWeapon->GetDualWieldMaster();

			if(master && master->GetIWeapon())
			{
				CWeapon *dualWieldMaster = static_cast<CWeapon *>(master);
				{
					if(!dualWieldMaster->IsWeaponRaised())
					{
						m_pWeapon->StopFire();
						dualWieldMaster->SetFireAlternation(!dualWieldMaster->GetFireAlternation());
						dualWieldMaster->StartFire();
					}
					else
					{
						Shoot(true);
					}
				}
			}
		}
	}
}

//--------------------------------------------------------------
void CSingle::EmitTracer(const Vec3 &pos, const Vec3 &destination, bool ooa)
{
	CTracerManager::STracerParams params;
	params.position = GetTracerPos(pos, ooa);
	params.destination = destination;

	if(m_pWeapon->GetStats().fp)
	{
		Vec3 dir = (destination - params.position);
		float lenght = dir.NormalizeSafe();

		if(lenght < (g_pGameCVars->tracer_min_distance * 0.5f))
		{
			return;
		}

		params.position += (dir * g_pGameCVars->tracer_min_distance * 0.5f);
	}

	if (ooa)
	{
		if(m_pWeapon->GetStats().fp)
		{
			params.geometry = m_pShared->ooatracerparams.geometryFP.c_str();
			params.effect = m_pShared->ooatracerparams.effect.c_str();
			params.speed = m_pShared->ooatracerparams.speedFP;
		}
		else
		{
			params.geometry = m_pShared->ooatracerparams.geometry.c_str();
			params.effect = m_pShared->ooatracerparams.effect.c_str();
			params.speed = m_pShared->ooatracerparams.speed;
		}

		params.lifetime = m_pShared->ooatracerparams.lifetime;
	}
	else
	{
		if(m_pWeapon->GetStats().fp)
		{
			params.geometry = m_pShared->tracerparams.geometryFP.c_str();
			params.effect = m_pShared->tracerparams.effectFP.c_str();
			params.speed = m_pShared->tracerparams.speedFP;
		}
		else
		{
			params.geometry = m_pShared->tracerparams.geometry.c_str();
			params.effect = m_pShared->tracerparams.effect.c_str();
			params.speed = m_pShared->tracerparams.speed;
		}

		params.lifetime = m_pShared->tracerparams.lifetime;
	}

	g_pGame->GetWeaponSystem()->GetTracerManager().EmitTracer(params);
}

//------------------------------------------------
void CSingle::RestoreOverHeating(bool activate)
{
	if(activate)
	{
		if(m_nextHeatTime > 0.0f)
		{
			CTimeValue time = gEnv->pTimer->GetFrameStartTime();
			float dt = m_nextHeatTime - time.GetSeconds();

			if(dt > 0.0f)
			{
				m_heat = max(dt, 1.0f);
			}

			if(dt > 1.0f)
			{
				m_overheat = dt - 1.0f;
			}

			m_nextHeatTime = 0.0f;
		}
	}
	else
	{
		m_nextHeatTime = 0.0f;

		if(m_heat > 0.0f)
		{
			CTimeValue time = gEnv->pTimer->GetFrameStartTime();
			m_nextHeatTime = time.GetSeconds() + m_heat + m_overheat;
		}
	}
}

//----------------------------------------------------
bool CSingle::CanOverheat() const
{
	return m_pShared->heatingparams.attack > 0.0f;
}
