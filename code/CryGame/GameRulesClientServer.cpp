/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 23:5:2006   9:27 : Created by Marcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "ScriptBind_GameRules.h"
#include "GameRules.h"
#include "Game.h"
#include "GameCVars.h"
#include "Actor.h"
#include "Player.h"

#include "IVehicleSystem.h"
#include "IItemSystem.h"
#include "IMaterialEffects.h"

#include "WeaponSystem.h"
#include "IWorldQuery.h"

#include <StlUtils.h>

#include "PlayerMovementController.h"

//------------------------------------------------------------------------
void CGameRules::ClientSimpleHit(const SimpleHitInfo &simpleHitInfo)
{
	if (!simpleHitInfo.remote)
	{
		if (!gEnv->bServer)
		{
			GetGameObject()->InvokeRMI(SvRequestSimpleHit(), simpleHitInfo, eRMI_ToServer);
		}
		else
		{
			ServerSimpleHit(simpleHitInfo);
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::ClientHit(const HitInfo &hitInfo)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);
	IActor *pClientActor = g_pGame->GetIGameFramework()->GetClientActor();
	IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(hitInfo.targetId);

	if(pActor == pClientActor)
		if (gEnv->pInput)
		{
			gEnv->pInput->ForceFeedbackEvent( SFFOutputEvent(eDI_XI, eFF_Rumble_Basic, 0.5f * hitInfo.damage * 0.01f, hitInfo.damage * 0.02f, 0.0f));
		}

	CreateScriptHitInfo(m_scriptHitInfo, hitInfo);
	CallScript(m_clientStateScript, "OnHit", m_scriptHitInfo);
	bool backface = hitInfo.dir.Dot(hitInfo.normal) > 0;

	if (!hitInfo.remote && hitInfo.targetId && !backface)
	{
		if (!gEnv->bServer)
		{
			GetGameObject()->InvokeRMI(SvRequestHit(), hitInfo, eRMI_ToServer);
		}
		else
		{
			ServerHit(hitInfo);
		}

		if(gEnv->IsClient())
		{
			ProcessLocalHit(hitInfo);
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::ServerSimpleHit(const SimpleHitInfo &simpleHitInfo)
{
	switch (simpleHitInfo.type)
	{
		case 0: // tag
			{
				if (!simpleHitInfo.targetId)
				{
					return;
				}

				// tagged entities are temporary in MP, not in SP.
				bool temp = gEnv->bMultiplayer;
				AddTaggedEntity(simpleHitInfo.shooterId, simpleHitInfo.targetId, temp);
			}
			break;

		case 1: // tac
			{
				if (!simpleHitInfo.targetId)
				{
					return;
				}

				CActor *pActor = (CActor *)gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(simpleHitInfo.targetId);

				if (pActor && pActor->CanSleep())
				{
					pActor->Fall(Vec3(0.0f, 0.0f, 0.0f), simpleHitInfo.value);
				}
			}
			break;

		case 0xe: // freeze
			{
				if (!simpleHitInfo.targetId)
				{
					return;
				}

				// call OnFreeze
				bool allow = true;

				if (m_serverStateScript.GetPtr() && m_serverStateScript->GetValueType("OnFreeze") == svtFunction)
				{
					HSCRIPTFUNCTION func = 0;
					m_serverStateScript->GetValue("OnFreeze", func);
					Script::CallReturn(m_serverStateScript->GetScriptSystem(), func, m_script, ScriptHandle(simpleHitInfo.targetId), ScriptHandle(simpleHitInfo.shooterId), ScriptHandle(simpleHitInfo.weaponId), simpleHitInfo.value, allow);
					gEnv->pScriptSystem->ReleaseFunc(func);
				}

				if (!allow)
				{
					return;
				}

				if (IEntity *pEntity = gEnv->pEntitySystem->GetEntity(simpleHitInfo.targetId))
				{
					IScriptTable *pScriptTable = pEntity->GetScriptTable();

					// call OnFrost
					if (pScriptTable && pScriptTable->GetValueType("OnFrost") == svtFunction)
					{
						HSCRIPTFUNCTION func = 0;
						pScriptTable->GetValue("OnFrost", func);
						Script::Call(pScriptTable->GetScriptSystem(), func, pScriptTable, ScriptHandle(simpleHitInfo.shooterId), ScriptHandle(simpleHitInfo.weaponId), simpleHitInfo.value);
						gEnv->pScriptSystem->ReleaseFunc(func);
					}

					FreezeEntity(simpleHitInfo.targetId, true, true, simpleHitInfo.value > 0.999f);
				}
			}
			break;

		default:
			assert(!"Unknown Simple Hit type!");
	}
}

//------------------------------------------------------------------------
void CGameRules::ServerHit(const HitInfo &hitInfo)
{
	if (m_processingHit)
	{
		m_queuedHits.push(hitInfo);
		return;
	}

	++m_processingHit;
	ProcessServerHit(hitInfo);

	while (!m_queuedHits.empty())
	{
		HitInfo info(m_queuedHits.front());
		ProcessServerHit(info);
		m_queuedHits.pop();
	}

	--m_processingHit;
}

//------------------------------------------------------------------------
void CGameRules::ProcessServerHit(const HitInfo &hitInfo)
{
	bool ok = true;
	// check if shooter is alive
	CActor *pShooter = GetActorByEntityId(hitInfo.shooterId);
	CActor *pTarget = GetActorByEntityId(hitInfo.targetId);

	if (hitInfo.shooterId)
	{
		if (pShooter && pShooter->GetHealth() <= 0)
		{
			ok = false;
		}
	}

	if (hitInfo.targetId)
	{
		if (pTarget && pTarget->GetSpectatorMode())
		{
			ok = false;
		}
	}

	if (ok)
	{
		float fTargetHealthBeforeHit = 0.0f;

		if(pTarget)
		{
			fTargetHealthBeforeHit = pTarget->GetHealth();
		}

		CreateScriptHitInfo(m_scriptHitInfo, hitInfo);
		CallScript(m_serverStateScript, "OnHit", m_scriptHitInfo);

		if(pTarget && !pTarget->IsDead())
		{
			const float fCausedDamage = fTargetHealthBeforeHit - pTarget->GetHealth();
			ProcessLocalHit(hitInfo, fCausedDamage);
		}

		// call hit listeners if any
		if (m_hitListeners.empty() == false)
		{
			for (size_t i = 0; i < m_hitListeners.size(); )
			{
				size_t count = m_hitListeners.size();
				m_hitListeners[i]->OnHit(hitInfo);

				if (count == m_hitListeners.size())
				{
					i++;
				}
				else
				{
					continue;
				}
			}
		}

		if (pShooter && hitInfo.shooterId != hitInfo.targetId && hitInfo.weaponId != hitInfo.shooterId && hitInfo.weaponId != hitInfo.targetId && hitInfo.damage >= 0)
		{
			EntityId params[2];
			params[0] = hitInfo.weaponId;
			params[1] = hitInfo.targetId;
			m_pGameplayRecorder->Event(pShooter->GetEntity(), GameplayEvent(eGE_WeaponHit, 0, 0, (void *)params));
		}

		if (pShooter)
		{
			m_pGameplayRecorder->Event(pShooter->GetEntity(), GameplayEvent(eGE_Hit, 0, 0, (void *)hitInfo.weaponId));
		}

		if (pShooter)
		{
			m_pGameplayRecorder->Event(pShooter->GetEntity(), GameplayEvent(eGE_Damage, 0, hitInfo.damage, (void *)hitInfo.weaponId));
		}
	}
}

void CGameRules::ProcessLocalHit( const HitInfo &hitInfo, float fCausedDamage /*= 0.0f*/ )
{
}

//------------------------------------------------------------------------
void CGameRules::ServerExplosion(const ExplosionInfo &explosionInfo)
{
	m_queuedExplosions.push(explosionInfo);
}

//------------------------------------------------------------------------
void CGameRules::ProcessServerExplosion(const ExplosionInfo &explosionInfo)
{
	//CryLog("[ProcessServerExplosion] (frame %i) shooter %i, damage %.0f, radius %.1f", gEnv->pRenderer->GetFrameID(), explosionInfo.shooterId, explosionInfo.damage, explosionInfo.radius);
	GetGameObject()->InvokeRMI(ClExplosion(), explosionInfo, eRMI_ToRemoteClients);
	ClientExplosion(explosionInfo);
}

//------------------------------------------------------------------------
void CGameRules::ProcessQueuedExplosions()
{
	const static uint8 nMaxExp = 3;

	for (uint8 exp = 0; !m_queuedExplosions.empty() && exp < nMaxExp; ++exp)
	{
		ExplosionInfo info(m_queuedExplosions.front());
		ProcessServerExplosion(info);
		m_queuedExplosions.pop();
	}
}

//------------------------------------------------------------------------
void CGameRules::KnockActorDown( EntityId actorEntityId )
{
}


//------------------------------------------------------------------------
void CGameRules::CullEntitiesInExplosion(const ExplosionInfo &explosionInfo)
{
	if (!g_pGameCVars->g_ec_enable || explosionInfo.damage <= 0.1f)
	{
		return;
	}

	IPhysicalEntity **pents;
	float radiusScale = g_pGameCVars->g_ec_radiusScale;
	float minVolume = g_pGameCVars->g_ec_volume;
	float minExtent = g_pGameCVars->g_ec_extent;
	int   removeThreshold = max(1, g_pGameCVars->g_ec_removeThreshold);
	IActor *pClientActor = g_pGame->GetIGameFramework()->GetClientActor();
	Vec3 radiusVec(radiusScale * explosionInfo.physRadius);
	int i = gEnv->pPhysicalWorld->GetEntitiesInBox(explosionInfo.pos - radiusVec, explosionInfo.pos + radiusVec, pents, ent_rigid | ent_sleeping_rigid);
	int removedCount = 0;
	static IEntityClass *s_pInteractiveEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("InteractiveEntity");
	static IEntityClass *s_pDeadBodyClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("DeadBody");

	if (i > removeThreshold)
	{
		int entitiesToRemove = i - removeThreshold;

		for(--i; i >= 0; i--)
		{
			if(removedCount >= entitiesToRemove)
			{
				break;
			}

			IEntity *pEntity = (IEntity *) pents[i]->GetForeignData(PHYS_FOREIGN_ID_ENTITY);

			if (pEntity)
			{
				// don't remove if entity is held by the player
				if (pClientActor && pEntity->GetId() == pClientActor->GetGrabbedEntityId())
				{
					continue;
				}

				// don't remove items/pickups
				if (IItem *pItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pEntity->GetId()))
				{
					continue;
				}

				// don't remove enemies/ragdolls
				if (IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId()))
				{
					continue;
				}

				// if there is a flowgraph attached, never remove!
				if (pEntity->GetProxy(ENTITY_PROXY_FLOWGRAPH) != 0)
				{
					continue;
				}

				IEntityClass *pClass = pEntity->GetClass();

				if (pClass == s_pInteractiveEntityClass || pClass == s_pDeadBodyClass)
				{
					continue;
				}

				// get bounding box
				if (IEntityPhysicalProxy *pPhysProxy = (IEntityPhysicalProxy *)pEntity->GetProxy(ENTITY_PROXY_PHYSICS))
				{
					AABB aabb;
					pPhysProxy->GetWorldBounds(aabb);

					// don't remove objects which are larger than a predefined minimum volume
					if (aabb.GetVolume() > minVolume)
					{
						continue;
					}

					// don't remove objects which are larger than a predefined minimum volume
					Vec3 size(aabb.GetSize().abs());

					if (size.x > minExtent || size.y > minExtent || size.z > minExtent)
					{
						continue;
					}
				}

				// marcok: somehow editor doesn't handle deleting non-dynamic entities very well
				// but craig says, hiding is not synchronized for DX11 breakable MP, so we remove entities only when playing pure game
				// alexl: in SinglePlayer, we also currently only hide the object because it could be part of flowgraph logic
				//        which would break if Entity was removed and could not propagate events anymore
				if (gEnv->bMultiplayer == false || gEnv->IsEditor())
				{
					pEntity->Hide(true);
				}
				else
				{
					gEnv->pEntitySystem->RemoveEntity(pEntity->GetId());
				}

				removedCount++;
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::ClientExplosion(const ExplosionInfo &explosionInfo)
{
	// let 3D engine know about explosion (will create holes and remove vegetation)
	if (explosionInfo.hole_size > 1.0f && gEnv->p3DEngine->GetIVoxTerrain())
	{
		gEnv->p3DEngine->OnExplosion(explosionInfo.pos, explosionInfo.hole_size, true);
	}

	TExplosionAffectedEntities affectedEntities;

	if (gEnv->bServer)
	{
		CullEntitiesInExplosion(explosionInfo);
		pe_explosion explosion;
		explosion.epicenter = explosionInfo.pos;
		explosion.rmin = explosionInfo.minRadius;
		explosion.rmax = explosionInfo.radius;

		if (explosion.rmax == 0)
		{
			explosion.rmax = 0.0001f;
		}

		explosion.r = explosion.rmin;
		explosion.impulsivePressureAtR = explosionInfo.pressure;
		explosion.epicenterImp = explosionInfo.pos;
		explosion.explDir = explosionInfo.dir;
		explosion.nGrow = 0;
		explosion.rminOcc = 0.07f;
		// we separate the calls to SimulateExplosion so that we can define different radii for AI and physics bodies
		explosion.holeSize = 0.0f;
		explosion.nOccRes = explosion.rmax > 50.0f ? 0 : 16;
		gEnv->pPhysicalWorld->SimulateExplosion( &explosion, 0, 0, ent_living);
		CreateScriptExplosionInfo(m_scriptExplosionInfo, explosionInfo);
		UpdateAffectedEntitiesSet(affectedEntities, &explosion);
		// check vehicles
		IVehicleSystem *pVehicleSystem = g_pGame->GetIGameFramework()->GetIVehicleSystem();
		uint32 vcount = pVehicleSystem->GetVehicleCount();

		if (vcount > 0)
		{
			IVehicleIteratorPtr iter = g_pGame->GetIGameFramework()->GetIVehicleSystem()->CreateVehicleIterator();

			while (IVehicle *pVehicle = iter->Next())
			{
				if(IEntity *pEntity = pVehicle->GetEntity())
				{
					AABB aabb;
					pEntity->GetWorldBounds(aabb);
					IPhysicalEntity *pEnt = pEntity->GetPhysics();

					if (pEnt && aabb.GetDistanceSqr(explosionInfo.pos) <= explosionInfo.radius * explosionInfo.radius)
					{
						float affected = gEnv->pPhysicalWorld->CalculateExplosionExposure(&explosion, pEnt);
						AddOrUpdateAffectedEntity(affectedEntities, pEntity, affected);
					}
				}
			}
		}

		explosion.rmin = explosionInfo.minPhysRadius;
		explosion.rmax = explosionInfo.physRadius;

		if (explosion.rmax == 0)
		{
			explosion.rmax = 0.0001f;
		}

		explosion.r = explosion.rmin;
		explosion.holeSize = explosionInfo.hole_size;

		if (explosion.nOccRes > 0)
		{
			explosion.nOccRes = -1;    // makes second call re-use occlusion info
		}

		gEnv->pPhysicalWorld->SimulateExplosion( &explosion, 0, 0, ent_rigid | ent_sleeping_rigid | ent_independent | ent_static | ent_delayed_deformations);
		UpdateAffectedEntitiesSet(affectedEntities, &explosion);
		CommitAffectedEntitiesSet(m_scriptExplosionInfo, affectedEntities);
		float fSuitEnergyBeforeExplosion = 0.0f;
		float fHealthBeforeExplosion = 0.0f;
		IActor *pClientActor = g_pGame->GetIGameFramework()->GetClientActor();

		if(pClientActor)
		{
			fHealthBeforeExplosion = (float)pClientActor->GetHealth();
		}

		CallScript(m_serverStateScript, "OnExplosion", m_scriptExplosionInfo);

		// call hit listeners if any
		if (m_hitListeners.empty() == false)
		{
			for (size_t i = 0; i < m_hitListeners.size(); )
			{
				size_t count = m_hitListeners.size();
				m_hitListeners[i]->OnServerExplosion(explosionInfo);

				if (count == m_hitListeners.size())
				{
					i++;
				}
				else
				{
					continue;
				}
			}
		}
	}

	if (gEnv->IsClient())
	{
		if (explosionInfo.pParticleEffect)
		{
			explosionInfo.pParticleEffect->Spawn(true, IParticleEffect::ParticleLoc(explosionInfo.pos, explosionInfo.dir, explosionInfo.effect_scale));
		}

		if (!gEnv->bServer)
		{
			CreateScriptExplosionInfo(m_scriptExplosionInfo, explosionInfo);
		}
		else
		{
			affectedEntities.clear();
			CommitAffectedEntitiesSet(m_scriptExplosionInfo, affectedEntities);
		}

		CallScript(m_clientStateScript, "OnExplosion", m_scriptExplosionInfo);

		// call hit listeners if any
		if (m_hitListeners.empty() == false)
		{
			THitListenerVec::iterator iter = m_hitListeners.begin();

			while (iter != m_hitListeners.end())
			{
				(*iter)->OnExplosion(explosionInfo);
				++iter;
			}
		}
	}

	ProcessClientExplosionScreenFX(explosionInfo);
	ProcessExplosionMaterialFX(explosionInfo);

	if (gEnv->pAISystem && !gEnv->bMultiplayer)
	{
		// Associate event with vehicle if the shooter is in a vehicle (tank cannon shot, etc)
		EntityId ownerId = explosionInfo.shooterId;
		IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(ownerId);

		if (pActor && pActor->GetLinkedVehicle() && pActor->GetLinkedVehicle()->GetEntityId())
		{
			ownerId = pActor->GetLinkedVehicle()->GetEntityId();
		}

		if (ownerId != 0)
		{
			SAIStimulus stim(AISTIM_EXPLOSION, 0, ownerId, 0,
							 explosionInfo.pos, ZERO, explosionInfo.radius);
			gEnv->pAISystem->RegisterStimulus(stim);
			SAIStimulus stimSound(AISTIM_SOUND, AISOUND_EXPLOSION, ownerId, 0,
								  explosionInfo.pos, ZERO, explosionInfo.radius * 3.0f, AISTIMPROC_FILTER_LINK_WITH_PREVIOUS);
			gEnv->pAISystem->RegisterStimulus(stimSound);
		}
	}
}

//-------------------------------------------
void CGameRules::ProcessClientExplosionScreenFX(const ExplosionInfo &explosionInfo)
{
	IActor *pClientActor = g_pGame->GetIGameFramework()->GetClientActor();

	if (pClientActor)
	{
		//Distance
		float dist = (pClientActor->GetEntity()->GetWorldPos() - explosionInfo.pos).len();
		//Is the explosion in Player's FOV (let's suppose the FOV a bit higher, like 80)
		CActor *pActor = (CActor *)pClientActor;
		SMovementState state;

		if (IMovementController *pMV = pActor->GetMovementController())
		{
			pMV->GetMovementState(state);
		}

		Vec3 eyeToExplosion = explosionInfo.pos - state.eyePosition;
		eyeToExplosion.Normalize();
		bool inFOV = (state.eyeDirection.Dot(eyeToExplosion) > 0.68f);

		// if in a vehicle eyeDirection is wrong
		if(pActor && pActor->GetLinkedVehicle())
		{
			Vec3 eyeDir = static_cast<CPlayer *>(pActor)->GetVehicleViewDir();
			inFOV = (eyeDir.Dot(eyeToExplosion) > 0.68f);
		}

		//All explosions have radial blur (default 30m radius, to make Sean happy =))
		float maxBlurDistance = (explosionInfo.maxblurdistance > 0.0f) ? explosionInfo.maxblurdistance : 30.0f;

		if (maxBlurDistance > 0.0f && g_pGameCVars->g_radialBlur > 0.0f && m_explosionScreenFX && explosionInfo.radius > 0.5f)
		{
			if (inFOV && dist < maxBlurDistance)
			{
				ray_hit hit;
				int col = gEnv->pPhysicalWorld->RayWorldIntersection(explosionInfo.pos , -eyeToExplosion * dist, ent_static | ent_terrain, rwi_stop_at_pierceable | rwi_colltype_any, &hit, 1);

				//If there was no obstacle between flashbang grenade and player
				if(!col)
				{
					float distAmp = 1.0f - (dist / maxBlurDistance);

					if (gEnv->pInput)
					{
						gEnv->pInput->ForceFeedbackEvent( SFFOutputEvent(eDI_XI, eFF_Rumble_Basic, 0.5f, distAmp * 3.0f, 0.0f));
					}
				}
			}
		}

		//Flashbang effect
		if(dist < explosionInfo.radius && inFOV &&
				(!strcmp(explosionInfo.effect_class, "flashbang") || !strcmp(explosionInfo.effect_class, "FlashbangAI")))
		{
			ray_hit hit;
			int col = gEnv->pPhysicalWorld->RayWorldIntersection(explosionInfo.pos , -eyeToExplosion * dist, ent_static | ent_terrain, rwi_stop_at_pierceable | rwi_colltype_any, &hit, 1);

			//If there was no obstacle between flashbang grenade and player
			if(!col)
			{
				float power = explosionInfo.flashbangScale;
				power *= max(0.0f, 1 - (dist / explosionInfo.radius));
				float lookingAt = (eyeToExplosion.Dot(state.eyeDirection.normalize()) + 1) * 0.5f;
				power *= lookingAt;
				gEnv->p3DEngine->SetPostEffectParam("Flashbang_Time", 1.0f + (power * 4));
				gEnv->p3DEngine->SetPostEffectParam("FlashBang_BlindAmount", explosionInfo.blindAmount);
				gEnv->p3DEngine->SetPostEffectParam("Flashbang_DifractionAmount", (power * 2));
				gEnv->p3DEngine->SetPostEffectParam("Flashbang_Active", 1);
			}
		}
		else if(inFOV && (dist < explosionInfo.radius))
		{
			if (explosionInfo.damage > 10.0f || explosionInfo.pressure > 100.0f)
			{
				//Add some angular impulse to the client actor depending on distance, direction...
				float dt = (1.0f - dist / explosionInfo.radius);
				dt = dt * dt;
				float angleZ = gf_PI * 0.15f * dt;
				float angleX = gf_PI * 0.15f * dt;
				pActor->AddAngularImpulse(Ang3(Random(-angleX * 0.5f, angleX), 0.0f, Random(-angleZ, angleZ)), 0.0f, dt * 2.0f);
			}
		}
	}
}

//---------------------------------------------------
void CGameRules::UpdateScoreBoardItem(EntityId id, const string name, int kills, int deaths)
{
}

//---------------------------------------------------
void CGameRules::ProcessExplosionMaterialFX(const ExplosionInfo &explosionInfo)
{
	// if an effect was specified, don't use MFX
	if (explosionInfo.pParticleEffect)
	{
		return;
	}

	// impact stuff here
	SMFXRunTimeEffectParams params;
	params.soundSemantic = eSoundSemantic_Explosion;
	params.pos = params.decalPos = explosionInfo.pos;
	params.trg = 0;
	params.trgRenderNode = 0;
	Vec3 gravity;
	pe_params_buoyancy buoyancy;
	gEnv->pPhysicalWorld->CheckAreas(params.pos, gravity, &buoyancy);
	// 0 for water, 1 for air
	Vec3 pos = params.pos;
	params.inWater = (buoyancy.waterPlane.origin.z > params.pos.z) && (gEnv->p3DEngine->GetWaterLevel(&pos) >= params.pos.z);
	params.inZeroG = (gravity.len2() < 0.0001f);
	params.trgSurfaceId = 0;
	static const int objTypes = ent_all;
	static const unsigned int flags = rwi_stop_at_pierceable | rwi_colltype_any;
	ray_hit ray;

	if (explosionInfo.impact)
	{
		params.dir[0] = explosionInfo.impact_velocity.normalized();
		params.normal = explosionInfo.impact_normal;

		if (gEnv->pPhysicalWorld->RayWorldIntersection(params.pos - params.dir[0] * 0.0125f, params.dir[0] * 0.25f, objTypes, flags, &ray, 1))
		{
			params.trgSurfaceId = ray.surface_idx;

			if (ray.pCollider->GetiForeignData() == PHYS_FOREIGN_ID_STATIC)
			{
				params.trgRenderNode = (IRenderNode *)ray.pCollider->GetForeignData(PHYS_FOREIGN_ID_STATIC);
			}
		}
	}
	else
	{
		params.dir[0] = gravity;
		params.normal = -gravity.normalized();

		if (gEnv->pPhysicalWorld->RayWorldIntersection(params.pos, gravity, objTypes, flags, &ray, 1))
		{
			params.trgSurfaceId = ray.surface_idx;

			if (ray.pCollider->GetiForeignData() == PHYS_FOREIGN_ID_STATIC)
			{
				params.trgRenderNode = (IRenderNode *)ray.pCollider->GetForeignData(PHYS_FOREIGN_ID_STATIC);
			}
		}
	}

	string effectClass = explosionInfo.effect_class;

	if (effectClass.empty())
	{
		effectClass = "generic";
	}

	string query = effectClass + "_explode";

	if(gEnv->p3DEngine->GetWaterLevel(&explosionInfo.pos) > explosionInfo.pos.z)
	{
		query = query + "_underwater";
	}

	if(IMaterialEffects *pMaterialEffects = gEnv->pGame->GetIGameFramework()->GetIMaterialEffects())
	{
		TMFXEffectId effectId = pMaterialEffects->GetEffectId(query.c_str(), params.trgSurfaceId);

		if (effectId == InvalidEffectId)
		{
			effectId = pMaterialEffects->GetEffectId(query.c_str(), pMaterialEffects->GetDefaultSurfaceIndex());
		}

		if (effectId != InvalidEffectId)
		{
			pMaterialEffects->ExecuteEffect(effectId, params);
		}
	}
}
//------------------------------------------------------------------------
// RMI
//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvRequestRename)
{
	CActor *pActor = GetActorByEntityId(params.entityId);

	if (!pActor)
	{
		return true;
	}

	RenamePlayer(pActor, params.name.c_str());
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClRenameEntity)
{
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(params.entityId);

	if (pEntity)
	{
		string old = pEntity->GetName();
		pEntity->SetName(params.name.c_str());
		CryLogAlways("$8%s$o renamed to $8%s", old.c_str(), params.name.c_str());
		// if this was a remote player, check we're not spectating them.
		//	If we are, we need to trigger a spectator hud update for the new name
		EntityId clientId = g_pGame->GetIGameFramework()->GetClientActorId();

		if(gEnv->bMultiplayer && params.entityId != clientId)
		{
			CActor *pClientActor = static_cast<CActor *>(g_pGame->GetIGameFramework()->GetClientActor());
		}
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvRequestChatMessage)
{
	SendChatMessage((EChatMessageType)params.type, params.sourceId, params.targetId, params.msg.c_str());
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClChatMessage)
{
	OnChatMessage((EChatMessageType)params.type, params.sourceId, params.targetId, params.msg.c_str(), params.onlyTeam);
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClForbiddenAreaWarning)
{
	return true;
}


//------------------------------------------------------------------------

IMPLEMENT_RMI(CGameRules, SvRequestRadioMessage)
{
	SendRadioMessage(params.sourceId, params.msg);
	return true;
}

//------------------------------------------------------------------------

IMPLEMENT_RMI(CGameRules, ClRadioMessage)
{
	OnRadioMessage(params.sourceId, params.msg);
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvRequestChangeTeam)
{
	CActor *pActor = GetActorByEntityId(params.entityId);

	if (!pActor)
	{
		return true;
	}

	ChangeTeam(pActor, params.teamId);
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvRequestSpectatorMode)
{
	CActor *pActor = GetActorByEntityId(params.entityId);

	if (!pActor)
	{
		return true;
	}

	ChangeSpectatorMode(pActor, params.mode, params.targetId, params.resetAll);
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClSetTeam)
{
	if (!params.entityId) // ignore these for now
	{
		return true;
	}

	int oldTeam = GetTeam(params.entityId);

	if (oldTeam == params.teamId)
	{
		return true;
	}

	TEntityTeamIdMap::iterator it = m_entityteams.find(params.entityId);

	if (it != m_entityteams.end())
	{
		m_entityteams.erase(it);
	}

	IActor *pActor = m_pActorSystem->GetActor(params.entityId);
	bool isplayer = pActor != 0;

	if (isplayer && oldTeam)
	{
		TPlayerTeamIdMap::iterator pit = m_playerteams.find(oldTeam);
		assert(pit != m_playerteams.end());
		stl::find_and_erase(pit->second, params.entityId);
	}

	if (params.teamId)
	{
		m_entityteams.insert(TEntityTeamIdMap::value_type(params.entityId, params.teamId));

		if (isplayer)
		{
			TPlayerTeamIdMap::iterator pit = m_playerteams.find(params.teamId);
			assert(pit != m_playerteams.end());
			pit->second.push_back(params.entityId);
		}
	}

	if(isplayer)
	{
		ReconfigureVoiceGroups(params.entityId, oldTeam, params.teamId);
	}

	ScriptHandle handle(params.entityId);
	CallScript(m_clientStateScript, "OnSetTeam", handle, params.teamId);
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClTextMessage)
{
	OnTextMessage((ETextMessageType)params.type, params.msg.c_str(),
				  params.params[0].empty() ? 0 : params.params[0].c_str(),
				  params.params[1].empty() ? 0 : params.params[1].c_str(),
				  params.params[2].empty() ? 0 : params.params[2].c_str(),
				  params.params[3].empty() ? 0 : params.params[3].c_str()
				 );
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvRequestSimpleHit)
{
	ServerSimpleHit(params);
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvRequestHit)
{
	HitInfo info(params);
	info.remote = true;
	ServerHit(info);
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClExplosion)
{
	ClientExplosion(params);
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClFreezeEntity)
{
	//IEntity *pEntity=gEnv->pEntitySystem->GetEntity(params.entityId);
	//CryLogAlways("ClFreezeEntity: %s %s", pEntity?pEntity->GetName():"<<null>>", params.freeze?"true":"false");
	FreezeEntity(params.entityId, params.freeze, 0);
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClShatterEntity)
{
	ShatterEntity(params.entityId, params.pos, params.impulse);
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClSetGameTime)
{
	m_endTime = params.endTime;
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClSetRoundTime)
{
	m_roundEndTime = params.endTime;
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClSetPreRoundTime)
{
	m_preRoundEndTime = params.endTime;
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClSetReviveCycleTime)
{
	m_reviveCycleEndTime = params.endTime;
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClSetGameStartTimer)
{
	m_gameStartTime = params.endTime;
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClTaggedEntity)
{
	if (!params.entityId)
	{
		return true;
	}

	SEntityEvent scriptEvent( ENTITY_EVENT_SCRIPT_EVENT );
	scriptEvent.nParam[0] = (INT_PTR)"OnGPSTagged";
	scriptEvent.nParam[1] = IEntityClass::EVT_BOOL;
	bool bValue = true;
	scriptEvent.nParam[2] = (INT_PTR)&bValue;
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(params.entityId);

	if (pEntity)
	{
		pEntity->SendEvent( scriptEvent );
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClTempRadarEntity)
{
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClAddSpawnGroup)
{
	AddSpawnGroup(params.entityId);
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClRemoveSpawnGroup)
{
	RemoveSpawnGroup(params.entityId);
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClAddMinimapEntity)
{
	AddMinimapEntity(params.entityId, params.type, params.lifetime);
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClRemoveMinimapEntity)
{
	RemoveMinimapEntity(params.entityId);
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClResetMinimap)
{
	ResetMinimap();
	return true;
}


//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClSetObjective)
{
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClSetObjectiveStatus)
{
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClSetObjectiveEntity)
{
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClResetObjectives)
{
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClHitIndicator)
{
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClDamageIndicator)
{
	Vec3 dir(ZERO);
	bool vehicle = false;

	if (IEntity *pEntity = gEnv->pEntitySystem->GetEntity(params.shooterId))
	{
		if (IActor *pLocal = m_pGameFramework->GetClientActor())
		{
			dir = (pLocal->GetEntity()->GetWorldPos() - pEntity->GetWorldPos());
			dir.NormalizeSafe();
			vehicle = (pLocal->GetLinkedVehicle() != 0);
		}
	}

	return true;
}

//------------------------------------------------------------------------

IMPLEMENT_RMI(CGameRules, SvVote)
{
	CActor *pActor = GetActorByChannelId(m_pGameFramework->GetGameChannelId(pNetChannel));

	if(pActor)
	{
		Vote(pActor, true);
	}

	return true;
}

IMPLEMENT_RMI(CGameRules, SvVoteNo)
{
	CActor *pActor = GetActorByChannelId(m_pGameFramework->GetGameChannelId(pNetChannel));

	if(pActor)
	{
		Vote(pActor, false);
	}

	return true;
}

IMPLEMENT_RMI(CGameRules, SvStartVoting)
{
	CActor *pActor = GetActorByChannelId(m_pGameFramework->GetGameChannelId(pNetChannel));

	if(pActor)
	{
		StartVoting(pActor, params.vote_type, params.entityId, params.param);
	}

	return true;
}

IMPLEMENT_RMI(CGameRules, ClVotingStatus)
{
	return true;
}


IMPLEMENT_RMI(CGameRules, ClEnteredGame)
{
	CPlayer *pClientActor = static_cast<CPlayer *>(m_pGameFramework->GetClientActor());

	if (pClientActor)
	{
		IEntity *pClientEntity = pClientActor->GetEntity();
		const EntityId clientEntityId = pClientEntity->GetId();

		if(!gEnv->bServer)
		{
			int status[2];
			status[0] = GetTeam(clientEntityId);
			status[1] = pClientActor->GetSpectatorMode();
			m_pGameplayRecorder->Event(pClientEntity, GameplayEvent(eGE_Connected, 0, 0, (void *)status));
		}
	}

	return true;
}

IMPLEMENT_RMI(CGameRules, ClPlayerJoined)
{
	return true;
}

IMPLEMENT_RMI(CGameRules, ClPlayerLeft)
{
	return true;
}
