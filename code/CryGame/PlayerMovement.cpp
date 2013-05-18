#include "StdAfx.h"
#include "PlayerMovement.h"
#include "GameUtils.h"
#include "Game.h"
#include "GameCVars.h"
#include "PlayerInput.h"
#include "GameActions.h"

#undef CALL_PLAYER_EVENT_LISTENERS
#define CALL_PLAYER_EVENT_LISTENERS(func) \
	{ \
		if (m_player.m_playerEventListeners.empty() == false) \
		{ \
			CPlayer::TPlayerEventListeners::const_iterator iter = m_player.m_playerEventListeners.begin(); \
			CPlayer::TPlayerEventListeners::const_iterator cur; \
			while (iter != m_player.m_playerEventListeners.end()) \
			{ \
				cur = iter; \
				++iter; \
				(*cur)->func; \
			} \
		} \
	}

#define LADDER_TOP_DISTANCE 2.41f
//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
CPlayerMovement::CPlayerMovement(CPlayer &player, const SActorFrameMovementParams &movement, float m_frameTime ) :
	m_frameTime(m_frameTime),
	m_params(player.m_params),
	m_stats(player.m_stats),
	m_viewQuat(player.m_viewQuat),
	m_baseQuat(player.m_baseQuat),
	m_movement(movement),
	m_player(player),
	m_velocity(player.m_velocity),
	m_upVector(player.m_upVector),
	m_detachLadder(false),
	m_onGroundWBoots(player.m_stats.onGroundWBoots),
	m_jumped(player.m_stats.jumped),
	m_actions(player.m_actions),
	m_turnTarget(player.m_turnTarget),
	m_thrusters(player.m_stats.thrusters),
	m_zgDashTimer(player.m_stats.zgDashTimer),
	m_zgDashWorldDir(player.m_stats.zgDashWorldDir),
	m_hasJumped(false),
	m_swimJumping(player.m_stats.swimJumping),
	m_waveRandomMult(1.0f),
	m_stickySurfaceTimer(player.m_stickySurfaceTimer)
{
	// derive some values that will be useful later
	m_worldPos = player.GetEntity()->GetWorldPos();
	m_waveTimer = Random() * gf_PI;
}

void CPlayerMovement::Process(CPlayer &player)
{
	if (m_stats.isOnLadder)
	{
		ProcessMovementOnLadder(player);
	}
	else if (/*m_stats.inAir &&*/ m_stats.inZeroG)
	{
		ProcessFlyingZeroG();
	}
	else if (m_stats.inFreefall.Value() == 1)
	{
		m_request.type = eCMT_Normal;
		m_request.velocity.zero();
	}
	else if (m_stats.inFreefall.Value() == 2)
	{
		ProcessParachute();
	}
	else if (player.ShouldSwim())
	{
		ProcessSwimming();
	}
	else
	{
		ProcessOnGroundOrJumping(player);
	}

	// if (!m_player.GetLinkedEntity() && !m_player.GetEntity()->GetParent()) // Leipzig hotfix, these can get out of sync
	if (player.m_linkStats.CanRotate())
	{
		ProcessTurning();
	}
}

void CPlayerMovement::Commit( CPlayer &player )
{
	if (player.m_pAnimatedCharacter)
	{
		m_request.allowStrafe = m_movement.allowStrafe;
		m_request.prediction = m_movement.prediction;
		m_request.jumping = m_stats.jumped;
		m_player.DebugGraph_AddValue("ReqVelo", m_request.velocity.GetLength());
		m_player.DebugGraph_AddValue("ReqVeloX", m_request.velocity.x);
		m_player.DebugGraph_AddValue("ReqVeloY", m_request.velocity.y);
		m_player.DebugGraph_AddValue("ReqVeloZ", m_request.velocity.z);
		m_player.DebugGraph_AddValue("ReqRotZ", RAD2DEG(m_request.rotation.GetRotZ()));
		player.m_pAnimatedCharacter->AddMovement( m_request );
	}

	if (m_detachLadder)
	{
		player.CreateScriptEvent("detachLadder", 0);
	}

	/*
		if (m_thrusters > .1f && m_stats.onGroundWBoots>-0.01f)
			player.CreateScriptEvent("thrusters",(m_actions & ACTION_SPRINT)?1:0);
	/**/

	if (m_hasJumped)
	{
		player.CreateScriptEvent("jumped", 0);
	}

	// Reset ground timer to prevent ground time before the jump to be inherited
	// and incorrectly/prematurely used to identify landing in mid air in MP.
	if (m_jumped && !player.m_stats.jumped)
	{
		player.m_stats.onGround = 0.0f;
	}

	player.m_velocity = m_velocity;
	player.m_stats.jumped = m_jumped;
	player.m_stats.onGroundWBoots = m_onGroundWBoots;
	player.m_turnTarget = m_turnTarget;
	player.m_lastRequestedVelocity = m_request.velocity;
	player.m_stats.thrusters = m_thrusters;
	player.m_stats.zgDashTimer = m_zgDashTimer;
	player.m_stats.zgDashWorldDir = m_zgDashWorldDir;
	player.m_stats.swimJumping = m_swimJumping;
	player.m_stickySurfaceTimer = m_stickySurfaceTimer;

	if(!player.m_stats.bIgnoreSprinting)
	{
		player.m_stats.bSprinting = ((m_stats.onGround > 0.1f || (m_stats.inWaterTimer > 0.0f)) && m_stats.inMovement > 0.1f && m_actions & ACTION_SPRINT);
	}

	if(player.m_stats.isOnLadder)
	{
		player.m_stats.bSprinting = ((m_actions & ACTION_SPRINT) && (m_movement.desiredVelocity.len2() > 0.0f));
	}
}

//-----------------------------------------------------------------------------------------------
// utility functions
//-----------------------------------------------------------------------------------------------
static Vec3 ProjectPointToLine(const Vec3 &point, const Vec3 &lineStart, const Vec3 &lineEnd)
{
	Lineseg seg(lineStart, lineEnd);
	float t;
	Distance::Point_Lineseg( point, seg, t );
	return seg.GetPoint(t);
}

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
void CPlayerMovement::ProcessFlyMode()
{
	Vec3 move = m_viewQuat * m_movement.desiredVelocity;
	float zMove(0.0f);

	if (m_actions & ACTION_JUMP)
	{
		zMove += 1.0f;
	}

	if (m_actions & ACTION_CROUCH)
	{
		zMove -= 1.0f;
	}

	move += m_viewQuat.GetColumn2() * zMove;
	//cap the movement vector to max 1
	float moveModule(move.len());

	if (moveModule > 1.0f)
	{
		move /= moveModule;
	}

	move *= m_params.speedMultiplier * m_player.GetZoomSpeedMultiplier(); // respect speed multiplier as well
	move *= 30.0f;

	if (m_actions & ACTION_SPRINT)
	{
		move *= 10.0f;
	}

	m_request.type = eCMT_Fly;
	m_request.velocity = move;
}

//-----------------------------------------------------------------------------------------------
void CPlayerMovement::ProcessFlyingZeroG()
{
	bool debug = false;
	Vec3 entityPos = m_player.GetEntity()->GetWorldPos();
	Vec3 vRight(m_baseQuat.GetColumn0());
	Vec3 vFwd(m_baseQuat.GetColumn1());
	bool trail = (g_pGameCVars->pl_zeroGParticleTrail != 0);

	if (trail)
	{
		m_player.SpawnParticleEffect("alien_special.human_thruster_zerog.human", entityPos, vFwd);
	}

	IPhysicalEntity *pPhysEnt = m_player.GetEntity()->GetPhysics();

	if (pPhysEnt != NULL)
	{
		pe_status_dynamics sd;

		if (pPhysEnt->GetStatus(&sd) != 0)
		{
			m_velocity = sd.v;
		}

		pe_player_dynamics pd;
		pd.kAirControl = 1.0f;
		pd.kAirResistance = 0.0f;
		pd.gravity.zero();
		pPhysEnt->SetParams(&pd);
	}

	//--------------------
	float dashSpeedBoost = 40.0f;
	float dashDuration = 0.4f;
	float dashRechargeDuration = 0.5f;
	float dashEnergyConsumption = 0.0f;	//g_pGameCVars->pl_zeroGDashEnergyConsumption * NANOSUIT_ENERGY;
	Vec3 acceleration(ZERO);
	// Apply desired movement
	Vec3 desiredLocalNormalizedVelocity(ZERO);
	Vec3 desiredLocalVelocity(ZERO);
	Vec3 desiredWorldVelocity(ZERO);
	// Calculate desired acceleration (user input)
	{
		desiredLocalNormalizedVelocity.x = m_movement.desiredVelocity.x;
		desiredLocalNormalizedVelocity.y = m_movement.desiredVelocity.y;

		if ((m_actions & ACTION_JUMP))
		{
			desiredLocalNormalizedVelocity.z += 1.0f;
		}
		else if (m_actions & ACTION_CROUCH)
		{
			desiredLocalNormalizedVelocity.z -= 1.0f;
		}

		desiredLocalNormalizedVelocity.NormalizeSafe(ZERO);
		float backwardMultiplier = (m_movement.desiredVelocity.y < 0.0f) ? m_params.backwardMultiplier : 1.0f;
		desiredLocalNormalizedVelocity.x *= m_params.strafeMultiplier;
		desiredLocalNormalizedVelocity.y *= backwardMultiplier;
		desiredLocalNormalizedVelocity.z *= g_pGameCVars->pl_zeroGUpDown;
		float maxSpeed = g_pGameCVars->pl_zeroGBaseSpeed;

		if (m_actions & ACTION_SPRINT)
		{
			maxSpeed *= g_pGameCVars->pl_zeroGSpeedMultNormalSprint;
		}
		else
		{
			maxSpeed *= g_pGameCVars->pl_zeroGSpeedMultNormal;
		}

		if (debug)
		{
			gEnv->pRenderer->DrawLabel(entityPos - vRight * 1.5f + Vec3(0, 0, 0.4f), 1.5f, "MoveN[%1.3f, %1.3f, %1.3f]", desiredLocalNormalizedVelocity.x, desiredLocalNormalizedVelocity.y, desiredLocalNormalizedVelocity.z);
		}

		/*
				if (debug)
					gEnv->pRenderer->DrawLabel(entityPos - vRight * 1.5f + Vec3(0,0,0.8f), 1.5f, "SprintMul %1.2f", sprintMultiplier);
		*/
		/*
				float stanceMaxSpeed = m_player.GetStanceMaxSpeed(STANCE_ZEROG);
				if (debug)
					gEnv->pRenderer->DrawLabel(entityPos - vRight * 1.5f + Vec3(0,0,0.6f), 1.5f, "StanceMax %1.3f", stanceMaxSpeed);
		*/

		if (debug)
		{
			gEnv->pRenderer->DrawLabel(entityPos - vRight * 1.5f + Vec3(0, 0, 0.6f), 1.5f, "StanceMax %1.3f", maxSpeed);
		}

		desiredLocalVelocity.x = desiredLocalNormalizedVelocity.x * maxSpeed;
		desiredLocalVelocity.y = desiredLocalNormalizedVelocity.y * maxSpeed;
		desiredLocalVelocity.z = desiredLocalNormalizedVelocity.z * maxSpeed;
		// The desired movement is applied in viewspace, not in entityspace, since entity does not nessecarily pitch while swimming.
		desiredWorldVelocity += m_viewQuat.GetColumn0() * desiredLocalVelocity.x;
		desiredWorldVelocity += m_viewQuat.GetColumn1() * desiredLocalVelocity.y;
		desiredWorldVelocity += m_viewQuat.GetColumn2() * desiredLocalVelocity.z;

		if (debug)
		{
			gEnv->pRenderer->DrawLabel(entityPos - vRight * 1.5f + Vec3(0, 0, 0.2f), 1.5f, "Move[%1.3f, %1.3f, %1.3f]", desiredWorldVelocity.x, desiredWorldVelocity.y, desiredWorldVelocity.z);
		}

		float desiredAlignment = m_velocity.GetNormalizedSafe(ZERO) * desiredWorldVelocity.GetNormalizedSafe(ZERO);
		int sound = 0;
		float thrusters = desiredWorldVelocity.GetLength();

		if ((thrusters > 0.0f) && (m_thrusters == 0.0f))
		{
			sound = 1;
		}

		if ((m_thrusters > 0.0f) && (thrusters == 0.0f))
		{
			sound = -1;
		}

		m_thrusters = thrusters;
		{
			float energy = 0.0f;

			if ((m_zgDashTimer <= 0.0f) && m_zgDashWorldDir.IsZero() &&
					(m_actions & ACTION_SPRINT) && /*!desiredWorldVelocity.IsZero() && */
					(fabs(desiredLocalNormalizedVelocity.x) > 0.9f))
			{
				if (energy >= dashEnergyConsumption)
				{
					m_zgDashTimer = 0.0f;
					m_zgDashWorldDir = desiredWorldVelocity.GetNormalized();
					m_player.PlaySound(CPlayer::ESound_ThrustersDash, true);
					//m_player.PlaySound(CPlayer::ESound_ThrustersDash02, true);
				}
				else
				{
					if (m_zgDashTimer == 0.0f)
					{
						m_zgDashTimer -= 0.3f;
						m_player.PlaySound(CPlayer::ESound_ThrustersDashFail, true);
					}
					else
					{
						m_zgDashTimer += m_frameTime;

						if (m_zgDashTimer > 0.0f)
						{
							m_zgDashTimer = 0.0f;
						}
					}
				}
			}

			if (m_zgDashTimer >= dashDuration)
			{
				if (!m_zgDashWorldDir.IsZero())
				{
					m_zgDashWorldDir.zero();
					//m_player.PlaySound(CPlayer::ESound_ThrustersDash, false);
					m_player.PlaySound(CPlayer::ESound_ThrustersDashRecharged, true);
					//m_player.PlaySound(CPlayer::ESound_ThrustersDashRecharged02, true);
				}

				if ((m_zgDashTimer >= (dashDuration + dashRechargeDuration)) &&
						(!(m_actions & ACTION_SPRINT) || /*desiredWorldVelocity.IsZero()*/
						 (fabs(desiredLocalNormalizedVelocity.x) < 0.7f)))
				{
					m_zgDashTimer = 0.0f;
				}
			}

			if (!m_zgDashWorldDir.IsZero() || (m_zgDashTimer > dashDuration))
			{
				float dashTimeFraction = sqr(1.0f - CLAMP(m_zgDashTimer / dashDuration, 0.0f, 1.0f));
				acceleration += dashSpeedBoost * dashTimeFraction * m_zgDashWorldDir;
				m_zgDashTimer += m_frameTime;
			}

			if (!m_zgDashWorldDir.IsZero())
			{
				desiredWorldVelocity *= 0.5f;
			}
		}
		acceleration += desiredWorldVelocity;
	}
	Vec3 gravityStream;
	pe_params_buoyancy buoyancy;

	if (gEnv->pPhysicalWorld->CheckAreas(entityPos, gravityStream, &buoyancy))
	{
		acceleration += gravityStream;
	}

	//--------------------
	// Apply velocity dampening (framerate independent)
	Vec3 damping(ZERO);
	{
		damping.x = abs(m_velocity.x);
		damping.y = abs(m_velocity.y);
		damping.z = abs(m_velocity.z);

		//*
		if (!m_zgDashWorldDir.IsZero())
		{
			float dashFraction = CLAMP(m_zgDashTimer / dashDuration, 0.0f, 1.0f);
			damping *= 1.0f + 1.0f * CLAMP((dashFraction - 0.5f) / 0.5f, 0.0f, 1.0f);
		}

		/**/
		float stopDelay = g_pGameCVars->pl_zeroGFloatDuration;

		if (!desiredWorldVelocity.IsZero())
		{
			stopDelay = g_pGameCVars->pl_zeroGThrusterResponsiveness;
		}
		else if (!m_zgDashWorldDir.IsZero())
		{
			stopDelay = g_pGameCVars->pl_zeroGFloatDuration * 0.5f;
		}

		damping *= (m_frameTime / stopDelay);
		m_velocity.x = (abs(m_velocity.x) > damping.x) ? (m_velocity.x - sgn(m_velocity.x) * damping.x) : 0.0f;
		m_velocity.y = (abs(m_velocity.y) > damping.y) ? (m_velocity.y - sgn(m_velocity.y) * damping.y) : 0.0f;
		m_velocity.z = (abs(m_velocity.z) > damping.z) ? (m_velocity.z - sgn(m_velocity.z) * damping.z) : 0.0f;
	}
	// Apply acceleration (framerate independent)
	float accelerateDelay = g_pGameCVars->pl_zeroGThrusterResponsiveness;
	m_velocity += acceleration * (m_frameTime / accelerateDelay);
	//--------------------
	// Set request type and velocity
	m_request.type = eCMT_Fly;
	m_request.velocity = m_velocity;

	// DEBUG VELOCITY
	if (debug)
	{
		gEnv->pRenderer->DrawLabel(entityPos - vRight * 1.5f - Vec3(0, 0, 0.0f), 1.5f, "Velo[%1.3f, %1.3f, %1.3f] (%1.3f)", m_velocity.x, m_velocity.y, m_velocity.z, m_velocity.len());
		gEnv->pRenderer->DrawLabel(entityPos - vRight * 1.5f - Vec3(0, 0, 0.2f), 1.5f, " Axx[%1.3f, %1.3f, %1.3f]", acceleration.x, acceleration.y, acceleration.z);
		//gEnv->pRenderer->DrawLabel(entityPos - vRight * 1.5f - Vec3(0,0,0.4f), 1.5f, "Damp[%1.3f, %1.3f, %1.3f]", damping.x, damping.y, damping.z);
		gEnv->pRenderer->DrawLabel(entityPos - vRight * 1.5f - Vec3(0, 0, 0.6f), 1.5f, "FrameTime %1.4f", m_frameTime);
	}

	/*
		m_player.DebugGraph_AddValue("Velo", m_velocity.len());
		m_player.DebugGraph_AddValue("VeloX", m_velocity.x);
		m_player.DebugGraph_AddValue("VeloY", m_velocity.y);
		m_player.DebugGraph_AddValue("VeloZ", m_velocity.z);
	/**/
	/*
		m_player.DebugGraph_AddValue("Axx", acceleration.len());
		m_player.DebugGraph_AddValue("AxxX", acceleration.x);
		m_player.DebugGraph_AddValue("AxxY", acceleration.y);
		m_player.DebugGraph_AddValue("AxxZ", acceleration.z);
	/**/
	//m_player.DebugGraph_AddValue("ZGDashTimer", m_zgDashTimer);
	//	CryLogAlways("speed: %.2f", m_velocity.len());
}

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
void CPlayerMovement::ProcessSwimming()
{
	bool debug = (g_pGameCVars->cl_debugSwimming != 0);
	Vec3 entityPos = m_player.GetEntity()->GetWorldPos();
	Vec3 vRight(m_baseQuat.GetColumn0());

	// Don't enable sticky surface directly when entering water.
	if (m_stats.inWaterTimer < 0.5f)
	{
		m_stickySurfaceTimer = 0.0f;
	}

	IPhysicalEntity *pPhysEnt = m_player.GetEntity()->GetPhysics();

	if (pPhysEnt != NULL) // entity might have been shattered on be unphysicalized
	{
		pe_status_dynamics sd;

		if (pPhysEnt->GetStatus(&sd) != 0)
		{
			m_velocity = sd.v;
		}

		pe_player_dynamics pd;
		pd.kAirControl = 1.0f;
		pd.kAirResistance = 0.0f;
		pPhysEnt->SetParams(&pd);
	}

	{
		// Apply water flow velocity to the player
		Vec3 gravity;
		pe_params_buoyancy buoyancy;

		if (gEnv->pPhysicalWorld->CheckAreas(entityPos, gravity, &buoyancy))
		{
			m_velocity += buoyancy.waterFlow * m_frameTime;
		}
	}

	Vec3 acceleration(ZERO);

	//--------------------

	// Apply gravity when above the surface.
	if (m_swimJumping || (m_stats.relativeWaterLevel > 0.2f))
	{
		float gravityScaling = 0.5f;

		if (!m_stats.gravity.IsZero())
		{
			acceleration += m_stats.gravity * gravityScaling;
		}
		else
		{
			acceleration.z += -9.8f * gravityScaling;
		}
	}

	//--------------------

	if ((m_velocity.z < -0.5f) && (m_stats.relativeWaterLevel < -0.2f))
	{
		m_swimJumping = false;
	}

	// Apply jump impulse when below but close to the surface (if in water for long enough).
	if (!m_swimJumping && (m_actions & ACTION_JUMP) && (m_velocity.z >= -0.2f) && (m_stats.relativeWaterLevel > -0.1f) && (m_stats.relativeWaterLevel < 0.1f))
	{
		float jumpMul = 1.0f;
		m_velocity.z = max(m_velocity.z, 6.0f + 2.0f * jumpMul);
		m_swimJumping = true;
	}

	if ((m_velocity.z > 5.0f) && (m_stats.relativeWaterLevel > 0.2f))
	{
		m_swimJumping = true;
	}

	//--------------------
	/*
		// Apply automatic float up towards surface when not in conflict with desired movement (if in water for long enough).
		if ((m_velocity.z > -0.1f) && (m_velocity.z < 0.2f) && (m_stats.relativeWaterLevel < -0.1f) && (m_stats.inWaterTimer > 0.5f))
			acceleration.z += (1.0f - sqr(1.0f - CLAMP(-m_stats.relativeWaterLevel, 0.0f, 1.0f))) * 0.08f;
	*/
	//--------------------
	// Apply desired movement
	Vec3 desiredLocalNormalizedVelocity(ZERO);
	Vec3 desiredLocalVelocity(ZERO);
	Vec3 desiredWorldVelocity(ZERO);
	// Less control when jumping or otherwise above surface
	// (where bottom ray miss geometry that still keeps the collider up).
	float userControlFraction = m_swimJumping ? 0.1f : 1.0f;
	// Calculate desired acceleration (user input)
	{
		// Apply up/down control when well below surface (if in water for long enough).
		if ((m_stats.inWaterTimer > 0.5f) && !m_swimJumping)
		{
			if (m_actions & ACTION_JUMP)
			{
				desiredLocalNormalizedVelocity.z += 1.0f;
			}
			else if (m_actions & ACTION_CROUCH)
			{
				desiredLocalNormalizedVelocity.z -= 1.0f;
			}
		}

		float backwardMultiplier = (m_movement.desiredVelocity.y < 0.0f) ? g_pGameCVars->pl_swimBackSpeedMul : 1.0f;
		desiredLocalNormalizedVelocity.x = m_movement.desiredVelocity.x * g_pGameCVars->pl_swimSideSpeedMul;
		desiredLocalNormalizedVelocity.y = m_movement.desiredVelocity.y * backwardMultiplier;
		// AI can set a custom sprint value, so don't cap the movement vector
		float sprintMultiplier = 1.0f;

		if ((m_actions & ACTION_SPRINT) && (m_stats.relativeWaterLevel < 0.2f))
		{
			sprintMultiplier = g_pGameCVars->pl_swimNormalSprintSpeedMul;
			// Higher speed multiplier when sprinting while looking up, to get higher dolphin jumps.
			float upFraction = CLAMP(m_viewQuat.GetFwdZ(), 0.0f, 1.0f);
			sprintMultiplier *= LERP(1.0f, g_pGameCVars->pl_swimUpSprintSpeedMul, upFraction);
		}

		if (debug)
		{
			gEnv->pRenderer->DrawLabel(entityPos - vRight * 1.5f + Vec3(0, 0, 1.0f), 1.5f, "SprintMul %1.2f", sprintMultiplier);
		}

		//float baseSpeed = m_player.GetStanceMaxSpeed(STANCE_SWIM);
		float baseSpeed = g_pGameCVars->pl_swimBaseSpeed;

		if (debug)
		{
			gEnv->pRenderer->DrawLabel(entityPos - vRight * 1.5f + Vec3(0, 0, 0.8f), 1.5f, "BaseSpeed %1.3f", baseSpeed);
		}

		desiredLocalVelocity.x = desiredLocalNormalizedVelocity.x * sprintMultiplier * baseSpeed;
		desiredLocalVelocity.y = desiredLocalNormalizedVelocity.y * sprintMultiplier * baseSpeed;
		desiredLocalVelocity.z = desiredLocalNormalizedVelocity.z * g_pGameCVars->pl_swimVertSpeedMul * baseSpeed;
		// The desired movement is applied in viewspace, not in entityspace, since entity does not necessarily pitch while swimming.
		desiredWorldVelocity += m_viewQuat.GetColumn0() * desiredLocalVelocity.x;
		desiredWorldVelocity += m_viewQuat.GetColumn1() * desiredLocalVelocity.y;
		// though, apply up/down in world space.
		desiredWorldVelocity.z += desiredLocalVelocity.z;

		if (debug)
		{
			gEnv->pRenderer->DrawLabel(entityPos - vRight * 1.5f + Vec3(0, 0, 0.6f), 1.5f, "MoveN[%1.3f, %1.3f, %1.3f]", desiredLocalNormalizedVelocity.x, desiredLocalNormalizedVelocity.y, desiredLocalNormalizedVelocity.z);
			gEnv->pRenderer->DrawLabel(entityPos - vRight * 1.5f + Vec3(0, 0, 0.5f), 1.5f, "VeloL[%1.3f, %1.3f, %1.3f]", desiredLocalVelocity.x, desiredLocalVelocity.y, desiredLocalVelocity.z);
			gEnv->pRenderer->DrawLabel(entityPos - vRight * 1.5f + Vec3(0, 0, 0.4f), 1.5f, "VeloW[%1.3f, %1.3f, %1.3f]", desiredWorldVelocity.x, desiredWorldVelocity.y, desiredWorldVelocity.z);
		}

		/*
				//if ((m_stats.waterLevel > 0.2f) && (desiredWorldVelocity.z > 0.0f)) // WIP: related to jumping out of water
				if ((m_stats.relativeWaterLevel > -0.1f) && (desiredWorldVelocity.z > 0.0f))
					desiredWorldVelocity.z = 0.0f;
		*/

		if (debug)
		{
			gEnv->pRenderer->DrawLabel(entityPos - vRight * 1.5f + Vec3(0, 0, 0.2f), 1.5f, "Move[%1.3f, %1.3f, %1.3f]", desiredWorldVelocity.x, desiredWorldVelocity.y, desiredWorldVelocity.z);
		}

		acceleration += desiredWorldVelocity * userControlFraction;
	}
	//--------------------
	float stickySurfaceRadius = 1.0f;
	float surfaceDistanceFraction = CLAMP(abs(m_stats.relativeWaterLevel) / stickySurfaceRadius, 0.0f, 1.0f);
	float surfaceProximityInfluence = 1.0f - surfaceDistanceFraction;
	float verticalVelocityFraction = CLAMP((abs(desiredWorldVelocity.z) - 0.3f) / 0.4f, 0.0f, 1.0f);
	surfaceProximityInfluence *= 1.0f - verticalVelocityFraction;

	if (m_swimJumping)
	{
		surfaceProximityInfluence = 0.0f;
	}

	//--------------------
	// Apply acceleration (framerate independent)
	float accelerateDelay = 0.3f;
	m_velocity += acceleration * (m_frameTime / accelerateDelay);
	// Apply velocity dampening (framerate independent)
	Vec3 damping(ZERO);
	{
		if (!m_swimJumping)
		{
			damping.x = abs(m_velocity.x);
			damping.y = abs(m_velocity.y);
		}

		// Vertical damping is special, to allow jumping out of water with higher speed,
		// and also not sink too deep when falling down ito the water after jump or such.
		float zDamp = 1.0f;
		zDamp += 6.0f * CLAMP((-m_velocity.z - 1.0f) / 3.0f, 0.0f, 1.0f);
		zDamp *= 1.0f - surfaceProximityInfluence;

		if (m_swimJumping)
		{
			zDamp = 0.0f;
		}

		damping.z = abs(m_velocity.z) * zDamp;
		float stopDelay = 0.3f;
		damping *= (m_frameTime / stopDelay);
		m_velocity.x = (abs(m_velocity.x) > damping.x) ? (m_velocity.x - sgn(m_velocity.x) * damping.x) : 0.0f;
		m_velocity.y = (abs(m_velocity.y) > damping.y) ? (m_velocity.y - sgn(m_velocity.y) * damping.y) : 0.0f;
		m_velocity.z = (abs(m_velocity.z) > damping.z) ? (m_velocity.z - sgn(m_velocity.z) * damping.z) : 0.0f;
	}

	//--------------------

	if (surfaceProximityInfluence > 0.0f)
	{
		m_stickySurfaceTimer += m_frameTime * surfaceProximityInfluence;
		float stickyFraction = CLAMP(m_stickySurfaceTimer / 1.0f, 0.0f, 1.0f);
		float desiredVeloZ = m_frameTime > 0.0f ? (m_stats.worldWaterLevelDelta / m_frameTime * surfaceProximityInfluence) : 0.0f;
		m_velocity.z = LERP(m_velocity.z, desiredVeloZ, stickyFraction);
		m_velocity.z += stickyFraction * 1.0f * -sgn(m_stats.relativeWaterLevel) * powf(surfaceDistanceFraction, 1.0f);
	}
	else
	{
		m_stickySurfaceTimer = 0.0f;
	}

	//--------------------
	// Set request type and velocity
	m_request.type = eCMT_Fly;
	m_request.velocity = m_velocity;

	// DEBUG VELOCITY
	if (debug)
	{
		gEnv->pRenderer->DrawLabel(entityPos - vRight * 1.5f - Vec3(0, 0, 0.0f), 1.5f, "Velo[%1.3f, %1.3f, %1.3f]", m_velocity.x, m_velocity.y, m_velocity.z);
		gEnv->pRenderer->DrawLabel(entityPos - vRight * 1.5f - Vec3(0, 0, 0.2f), 1.5f, " Axx[%1.3f, %1.3f, %1.3f]", acceleration.x, acceleration.y, acceleration.z);
		gEnv->pRenderer->DrawLabel(entityPos - vRight * 1.5f - Vec3(0, 0, 0.4f), 1.5f, "Damp[%1.3f, %1.3f, %1.3f]", damping.x, damping.y, damping.z);
		gEnv->pRenderer->DrawLabel(entityPos - vRight * 1.5f - Vec3(0, 0, 0.6f), 1.5f, "FrameTime %1.4f", m_frameTime);

		if (m_swimJumping)
		{
			gEnv->pRenderer->DrawLabel(entityPos - vRight * 0.15f + Vec3(0, 0, 0.6f), 2.0f, "JUMP");
		}
	}

	if (m_player.m_pAnimatedCharacter != NULL)
	{
		IAnimationGraphState *pAnimGraphState = m_player.m_pAnimatedCharacter->GetAnimationGraphState();

		if (pAnimGraphState != NULL)
		{
			IAnimationGraph::InputID inputSwimControlX = pAnimGraphState->GetInputId("SwimControlX");
			IAnimationGraph::InputID inputSwimControlY = pAnimGraphState->GetInputId("SwimControlY");
			IAnimationGraph::InputID inputSwimControlZ = pAnimGraphState->GetInputId("SwimControlZ");
			pAnimGraphState->SetInput(inputSwimControlX, desiredLocalVelocity.x);
			pAnimGraphState->SetInput(inputSwimControlY, desiredLocalVelocity.y);
			pAnimGraphState->SetInput(inputSwimControlZ, desiredWorldVelocity.z);
		}
	}

	return;
}

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
void CPlayerMovement::ProcessParachute()
{
	//Vec3 desiredVelocity(m_stats.velocity);
	float desiredZ(-1.5f + ((m_actions & ACTION_JUMP) ? 3.0f : 0.0f));
	//desiredVelocity.z += (desiredZ - desiredVelocity.z)*min(1.0f,m_frameTime*1.5f);
	m_request.type = eCMT_Impulse;//eCMT_Fly;
	m_request.velocity = (Vec3(0, 0, desiredZ) - m_stats.velocity) * m_stats.mass/* * m_frameTime*/; //desiredVelocity;
	Vec3 forwardComp(m_baseQuat.GetColumn1() * 10.0f);
	forwardComp.z = 0.0f;
	m_request.velocity += forwardComp * m_stats.mass;// * m_frameTime;//desiredVelocity;
}

void inline SinCos( float radians, float *sine, float *cosine )
{
	_asm
	{
		fld DWORD PTR [radians]
		fsincos

		mov edx, DWORD PTR [cosine]
		mov eax, DWORD PTR [sine]

		fstp DWORD PTR [edx]
		fstp DWORD PTR [eax]
	}
}

void AngleVectors( const Ang3 &angles, Vec3 *forward, Vec3 *right, Vec3 *up )
{
	float sr, sp, sy, cr, cp, cy;

    SinCos( DEG2RAD( angles.x ), &sy, &cy );
    SinCos( DEG2RAD( angles.y ), &sp, &cp );
    SinCos( DEG2RAD( angles.z ), &sr, &cr );

    if (forward)
    {
        forward->x = cp*cy;
        forward->y = cp*sy;
        forward->z = -sp;
    }

    if (right)
    {
        right->x = (-1*sr*sp*cy+-1*cr*-sy);
        right->y = (-1*sr*sp*sy+-1*cr*cy);
        right->z = -1*sr*cp;
    }

    if (up)
    {
        up->x = (cr*sp*cy+-sr*-sy);
        up->y = (cr*sp*sy+-sr*cy);
        up->z = cr*cp;
    }
}


#define SV_MAXSPEED 10000
#define SV_ACCELLERATE 10
#define SURFACE_FRICTION 1
#define STEP_SIZE 16.92
#define COORD_RESOLUTION 0.03125
#define ALLOW_AUTO_MOVEMENT true
#define DIST_EPSILON 0.03125
#define STOP_EPSILON        0.1
#define MAX_CLIP_PLANES     5
#define PLAYER_MAX_SAFE_FALL_SPEED   580
#define SV_BOUNCE 1

void SetPlayerPos(CPlayer *player, Vec3 pos)
{
	CPlayer::MoveParams params(pos, player->GetViewRotation());
	player->GetEntity()->SetWorldTM(Matrix34::Create(Vec3(1,1,1), params.rot, params.pos));
}

int TracePlayer(CPlayer *player, Vec3 start, Vec3 end, ray_hit *trace)
{
	return gEnv->pPhysicalWorld->RayTraceEntity(gEnv->pPhysicalWorld->GetPhysicalEntityById(player->GetEntityId()), start, end - start, trace);
}

void Accelerate(SCharacterMoveRequest *mv, CPlayer *player, Vec3& wishdir, float wishspeed, float accel )
{
    int i;
    float addspeed, accelspeed, currentspeed;

    // See if we are changing direction a bit
    currentspeed = mv->velocity.Dot(wishdir);

    // Reduce wishspeed by the amount of veer.
    addspeed = wishspeed - currentspeed;

    // If not going to add any speed, done.
    if (addspeed <= 0)
        return;

    // Determine amount of accleration.
	accelspeed = accel * gEnv->pTimer->GetFrameTime() * wishspeed * SURFACE_FRICTION;

    // Cap at addspeed
    if (accelspeed > addspeed)
        accelspeed = addspeed;
    
    // Adjust velocity.
    for (i=0 ; i<3 ; i++)
    {
		mv->velocity[i] += accelspeed * wishdir[i];    
    }
}

//-----------------------------------------------------------------------------
void StayOnGround( CPlayer *player, Vec3 origin )
{
	ray_hit trace;
	Vec3 start( origin );
	Vec3 end( origin );
	start.z += 2;
	end.z -= STEP_SIZE;

	// See how far up we can go without getting stuck

	auto numhits = TracePlayer(player, start, origin, &trace);
	start = trace.pt;

	// using trace.startsolid is unreliable here, it doesn't get set when
	// tracing bounding box vs. terrain

	// Now trace down from a known safe position
	numhits = TracePlayer(player, start, end, &trace);
	if
	(
		numhits > 0 && // must go somewhere
		trace.n[2] >= 0.7 // can't hit a steep slope that we can't stand on anyway
	)
	{
		float flDelta = fabs(origin.z - trace.pt.z);

		//This is incredibly hacky. The real problem is that trace returning that strange value we can't network over.
		if ( flDelta > 0.5f * COORD_RESOLUTION )
		{
			SetPlayerPos(player, trace.pt);
		}
	}
}

void VectorCopy(Vec3 from, Vec3 to)
{
	to.Set(from.x, from.y, from.z);
}

void VectorMA( const Vec3 &start, float scale, const Vec3 &direction, Vec3 &dest )
{
	dest.x = start.x + scale * direction.x;
	dest.y = start.y + scale * direction.y;
	dest.z = start.z + scale * direction.z;
}

void CrossProduct (const float* v1, const float* v2, float* cross)
{
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}

int ClipVelocity( Vec3& in, Vec3& normal, Vec3& out, float overbounce )
{
	float	backoff;
	float	change;
	float angle;
	int		i, blocked;
	
	angle = normal[ 2 ];

	blocked = 0x00;         // Assume unblocked.
	if (angle > 0)			// If the plane that is blocking us has a positive z component, then assume it's a floor.
		blocked |= 0x01;	// 
	if (!angle)				// If the plane has no Z, it is vertical (wall/step)
		blocked |= 0x02;	// 
	

	// Determine how far along plane to slide based on incoming direction.
	backoff = in.Dot(normal) * overbounce;

	for (i=0 ; i<3 ; i++)
	{
		change = normal[i]*backoff;
		out[i] = in[i] - change; 
	}
	
	// iterate once to make sure we aren't still moving through the plane
	float adjust = out.Dot(normal );
	if( adjust < 0.0f )
	{
		out -= ( normal * adjust );
//		Msg( "Adjustment = %lf\n", adjust );
	}

	// Return blocking flags.
	return blocked;
}

int TryPlayerMove( CPlayer *player, SCharacterMoveRequest *mv, Vec3 *pFirstDest = 0, ray_hit *pFirstTrace = 0 )
{
	int			bumpcount, numbumps;
	Vec3		dir;
	float		d;
	int			numplanes;
	Vec3		planes[MAX_CLIP_PLANES];
	Vec3		primal_velocity, original_velocity;
	Vec3        new_velocity;
	int			i, j;
	ray_hit 	pm;
	Vec3		end;
	float		time_left, allFraction;
	int			blocked;		
	
	numbumps  = 4;           // Bump up to four times
	
	blocked   = 0;           // Assume not blocked
	numplanes = 0;           //  and not sliding along any planes

	VectorCopy (mv->velocity, original_velocity);  // Store original velocity
	VectorCopy (mv->velocity, primal_velocity);
	
	allFraction = 0;
	time_left = gEnv->pTimer->GetFrameTime();   // Total time for this movement operation.

	new_velocity.zero();

	for (bumpcount=0 ; bumpcount < numbumps; bumpcount++)
	{
		if ( mv->velocity.GetLength() == 0.0 )
			break;

		// Assume we can move all the way from the current origin to the
		//  end point.
		VectorMA( player->GetEntity()->GetWorldPos(), time_left, mv->velocity, end );

		int numhits = 0;

		// See if we can make it from origin to end point.
		if ( true )
		{
			// If their velocity Z is 0, then we can avoid an extra trace here during WalkMove.
			if ( pFirstDest && end == *pFirstDest )
				pm = *pFirstTrace;
			else
			{
				numhits = TracePlayer( player, player->GetEntity()->GetWorldPos(), end, &pm );
			}
		}
		else
		{
			numhits = TracePlayer( player, player->GetEntity()->GetWorldPos(), end, &pm );
		}

		auto fraction = pm.dist / player->GetEntity()->GetWorldPos().GetDistance(pm.pt);

		allFraction += fraction;

		// If we started in a solid object, or we were in solid space
		//  the whole way, zero out our velocity and return that we
		//  are blocked by floor and wall.
		if (numhits)
		{	
			// entity is trapped in another solid
			VectorCopy (Vec3(), mv->velocity);
			return 4;
		}

		// If we moved some portion of the total distance, then
		//  copy the end position into the pmove.origin and 
		//  zero the plane counter.
		if( fraction > 0 )
		{	
			if ( numbumps > 0 && fraction == 1 )
			{
				// There's a precision issue with terrain tracing that can cause a swept box to successfully trace
				// when the end position is stuck in the triangle.  Re-run the test with an uswept box to catch that
				// case until the bug is fixed.
				// If we detect getting stuck, don't allow the movement
				ray_hit stuck;
				auto numhits = TracePlayer(player, pm.pt, pm.pt, &stuck );
				if ( numhits )
				{
					//Msg( "Player will become stuck!!!\n" );
					VectorCopy (Vec3(), mv->velocity);
					break;
				}
			}

			// actually covered some distance
			SetPlayerPos(player, pm.pt);
			VectorCopy (mv->velocity, original_velocity);
			numplanes = 0;
		}

		// If we covered the entire distance, we are done
		//  and can return.
		if (fraction == 1)
		{
			 break;		// moved the entire distance
		}

		// If the plane we hit has a high z component in the normal, then
		//  it's probably a floor
		if (pm.n[2] > 0.7)
		{
			blocked |= 1;		// floor
		}
		// If the plane has a zero z component in the normal, then it's a 
		//  step or wall
		if (!pm.n[2])
		{
			blocked |= 2;		// step / wall
		}

		// Reduce amount of m_flFrameTime left by total time left * fraction
		//  that we covered.
		time_left -= time_left * fraction;

		// Did we run out of planes to clip against?
		if (numplanes >= MAX_CLIP_PLANES)
		{	
			// this shouldn't really happen
			//  Stop our movement if so.
			VectorCopy (Vec3(), mv->velocity);
			//Con_DPrintf("Too many planes 4\n");

			break;
		}

		// Set up next clipping plane
		VectorCopy (pm.n, planes[numplanes]);
		numplanes++;

		auto stats = player->GetActorStats();

		// modify original_velocity so it parallels all of the clip planes
		//

		// reflect player velocity 
		// Only give this a try for first impact plane because you can get yourself stuck in an acute corner by jumping in place
		//  and pressing forward and nobody was really using this bounce/reflection feature anyway...
		if ( numplanes == 1 &&
			stats->inAir)	
		{
			for ( i = 0; i < numplanes; i++ )
			{
				if ( planes[i][2] > 0.7  )
				{
					// floor or slope
					ClipVelocity( original_velocity, planes[i], new_velocity, 1 );
					VectorCopy( new_velocity, original_velocity );
				}
				else
				{
					ClipVelocity( original_velocity, planes[i], new_velocity, 1.0 + SV_BOUNCE * (1 - SURFACE_FRICTION) );
				}
			}

			VectorCopy( new_velocity, mv->velocity );
			VectorCopy( new_velocity, original_velocity );
		}
		else
		{
			for (i=0 ; i < numplanes ; i++)
			{
				ClipVelocity (
					original_velocity,
					planes[i],
					mv->velocity,
					1);

				for (j=0 ; j<numplanes ; j++)
					if (j != i)
					{
						// Are we now moving against this plane?
						if (mv->velocity.Dot(planes[j]) < 0)
							break;	// not ok
					}
				if (j == numplanes)  // Didn't have to clip, so we're ok
					break;
			}
			
			// Did we go all the way through plane set
			if (i != numplanes)
			{	// go along this plane
				// pmove.velocity is set in clipping call, no need to set again.
				;  
			}
			else
			{	// go along the crease
				if (numplanes != 2)
				{
					VectorCopy (Vec3(), mv->velocity);
					break;
				}
				CrossProduct (planes[0], planes[1], dir);
				dir.Normalize();
				d = dir.Dot(mv->velocity);

				auto scaled = dir * d;

				mv->velocity.Set(scaled.x, scaled.y, scaled.z);
			}

			//
			// if original velocity is against the original velocity, stop dead
			// to avoid tiny occilations in sloping corners
			//
			d = mv->velocity.Dot(primal_velocity);
			if (d <= 0)
			{
				//Con_DPrintf("Back\n");
				VectorCopy (Vec3(), mv->velocity);
				break;
			}
		}
	}

	if ( allFraction == 0 )
	{
		VectorCopy (Vec3(), mv->velocity);
	}

	// Check if they slammed into a wall
	float fSlamVol = 0.0f;

	float fLateralStoppingAmount = primal_velocity.len2() - mv->velocity.len2();
	if ( fLateralStoppingAmount > PLAYER_MAX_SAFE_FALL_SPEED * 2.0f )
	{
		fSlamVol = 1.0f;
	}
	else if ( fLateralStoppingAmount > PLAYER_MAX_SAFE_FALL_SPEED )
	{
		fSlamVol = 0.85f;
	}
	
	/////// fall effects here ///////
	/////// fall effects here ///////
	/////// fall effects here ///////
	/////// fall effects here ///////

	return blocked;
}

void StepMove(CPlayer *player, SCharacterMoveRequest *mv, Vec3 origin, Vec3 vecDestination, ray_hit trace)
{
	Vec3 vecEndPos(vecDestination);

	// Try sliding forward both on ground and up 16 pixels
	//  take the move that goes farthest
	Vec3 vecPos(origin);
	Vec3 vecVel(mv->velocity);
	
	// Slide move down.
	TryPlayerMove( player, mv, &vecEndPos, &trace );
	
	// Down results.
	Vec3 vecDownPos(origin);
	Vec3 vecDownVel(mv->velocity);

	// Reset original values.
	SetPlayerPos(player, vecPos);
	vecVel.Set(mv->velocity.x, mv->velocity.y, mv->velocity.z);

	
	// Move up a stair height.
	VectorCopy(player->GetEntity()->GetWorldPos(), vecEndPos);

	if (ALLOW_AUTO_MOVEMENT)
	{
		vecEndPos.z += STEP_SIZE + DIST_EPSILON;
	}

	auto numhit = TracePlayer(player, origin, vecEndPos, &trace);

	if ( numhit )
	{
		SetPlayerPos(player, trace.pt);
	}

	// Slide move up.
	TryPlayerMove(player, mv);
		
	// Move down a stair (attempt to).
	VectorCopy(player->GetEntity()->GetWorldPos(), vecEndPos);

	if (ALLOW_AUTO_MOVEMENT)
	{
		vecEndPos.z -= STEP_SIZE + DIST_EPSILON;
	}
		
	numhit = TracePlayer(player, origin, vecEndPos, &trace);

	// If we are not on the ground any more then use the original movement attempt.
	if ( trace.n[2] < 0.7 )
	{
		SetPlayerPos(player, vecDownPos);

		vecDownVel.Set(mv->velocity.x, mv->velocity.y, mv->velocity.z);

		float flStepDist = origin.z - vecPos.z;
		if ( flStepDist > 0.0f )
		{
			//mv->m_outStepHeight += flStepDist; uumm
		}
		return;
	}

	// If the trace ended up in empty space, copy the end over to the origin.
	if ( !numhit )
	{
		SetPlayerPos(player, trace.pt);
	}
	
	// Copy this origin to up.
	Vec3 vecUpPos(player->GetEntity()->GetWorldPos());
	
	// decide which one went farther
	float flDownDist = ( vecDownPos.x - vecPos.x ) * ( vecDownPos.x - vecPos.x ) + ( vecDownPos.y - vecPos.y ) * ( vecDownPos.y - vecPos.y );
	float flUpDist = ( vecUpPos.x - vecPos.x ) * ( vecUpPos.x - vecPos.x ) + ( vecUpPos.y - vecPos.y ) * ( vecUpPos.y - vecPos.y );
	if ( flDownDist > flUpDist )
	{
		SetPlayerPos(player, vecDownPos );
		VectorCopy( vecDownVel, mv->velocity );
	}
	else 
	{
		// copy z value from slide move
		mv->velocity.z = vecDownVel.z;
	}
	
	float flStepDist = player->GetEntity()->GetWorldPos().z - vecPos.z;
	if ( flStepDist > 0 )
	{
		//mv->m_outStepHeight += flStepDist; uumm
	}
	
}


void LOL(CPlayer *player, SCharacterMoveRequest *mv)
{
	// cryengine helpers

	SMovementState params;
	player->GetMovementController()->GetMovementState(params);

	auto stats = player->GetActorStats();
	auto origin = player->GetEntity()->GetWorldPos();
	auto ft = gEnv->pTimer->GetFrameTime();

	// source

	int i;

	Vec3 wishvel;
	float spd;
	float fmove, smove;
	Vec3 wishdir;
	float wishspeed;

	Vec3 dest;
	ray_hit pm;
	Vec3 forward, right, up;

	AngleVectors(Ang3(player->GetViewRotation()), &forward, &right, &up);

	auto oldground = stats->onGround;

	// this is the speed in inches (500, 0, will make us forward, -500, 0 will make us run backwards etc)
	fmove = player->GetMovementController()->GetDesiredMoveDir().x; 
	smove = player->GetMovementController()->GetDesiredMoveDir().y; 

	// Zero out z components of movement vectors
	if ( forward[2] != 0 )
	{
		forward[2] = 0;
		forward.Normalize();
	}

	if ( right[2] != 0 )
	{
		right[2] = 0;
		right.Normalize();
	}
	
	// Determine x and y parts of velocity
	for (i=0 ; i<2 ; i++)
		wishvel[i] = forward[i]*fmove + right[i]*smove;

	// Zero out z part of velocity
	wishvel[2] = 0;
	
	// Determine maginitude of speed of move
	wishspeed = wishvel.GetLength();

	// Clamp to server defined max speed
	if ((wishspeed != 0.0f) && (wishspeed > SV_MAXSPEED))
	{
		wishvel.scale(SV_MAXSPEED / wishspeed);
		wishspeed = SV_MAXSPEED;
	}

	// Set pmove velocity
	mv->velocity[2] = 0;
	Accelerate(mv, player, wishdir, wishspeed, SV_ACCELLERATE);
	mv->velocity[2] = 0;
	
	// Add in any base velocity to the current velocity.
	mv->velocity += stats->velocity;

	spd = mv->velocity.GetLength();

	if ( spd < 1.0f )
	{
		mv->velocity.zero();

		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		mv->velocity -= stats->velocity;
		return;
	}

	// first try just moving to the destination	
	dest[0] = origin[0] + mv->velocity[0]*ft;
	dest[1] = origin[1] + mv->velocity[1]*ft;
	dest[2] = origin[2];
	
	mv->velocity += wishdir * wishspeed;

	auto numhits = TracePlayer(player, origin, dest, &pm);
	
	// If we made it all the way, then copy trace end as new player position.

	if (!numhits)
	{
		SetPlayerPos(player, pm.pt);

		// Now pull the base velocity back out.   Base velocity is set if you are on a moving object, like a conveyor (or maybe another monster?)
		mv->velocity -= stats->velocity;

		StayOnGround(player, origin);

		return;
	}


	StepMove(player, mv, origin, dest, pm);

	StayOnGround(player, origin);
}


//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
void CPlayerMovement::ProcessOnGroundOrJumping(CPlayer &player)
{

	//auto phys = m_player.GetEntity()->GetPhysics();

	//LOL(&player, &m_request);

	//{return;}

	//process movement
	Vec3 move(0, 0, 0);
	/*
		m_player.DebugGraph_AddValue("InputMoveX", m_movement.desiredVelocity.x);
		m_player.DebugGraph_AddValue("InputMoveY", m_movement.desiredVelocity.y);
	/**/
	IPhysicalEntity *pPhysEnt = m_player.GetEntity()->GetPhysics();

	if (pPhysEnt != NULL)
	{
		pe_player_dynamics pd;
		pd.kAirControl = m_player.GetAirControl();
		pd.kAirResistance = m_player.GetAirResistance();

		// PLAYERPREDICTION
		if(player.IsRemote() && (g_pGameCVars->pl_velocityInterpAirControlScale > 0))
		{
			pd.kAirControl = g_pGameCVars->pl_velocityInterpAirControlScale;
		}

		// ~PLAYERPREDICTION

		if (m_player.m_pAnimatedCharacter->GetMCMV() == eMCM_Animation)
		{
			pd.gravity.zero();
			pd.kInertia = 0;
			pd.kInertiaAccel = 0;
		}
		else
		{
			pd.gravity = m_stats.gravity;
		}

		pPhysEnt->SetParams(&pd);
	}

	float paramsSprintMul = m_params.sprintMultiplier;
	Vec3 desiredVelocityClamped = m_movement.desiredVelocity;
	const SStanceInfo *pPlayerStanceInfo = m_player.GetStanceInfo(m_player.GetStance());
	float customScale = 0.0f;

	if (m_movement.desiredVelocity.x || m_movement.desiredVelocity.y)
	{
		float desiredVelocityMag = desiredVelocityClamped.GetLength();

		if (desiredVelocityMag > 1.0f)
		{
			desiredVelocityClamped /= desiredVelocityMag;
		}

		float strafeMul = (pPlayerStanceInfo->customStrafeMultiplier > 0.0f) ? pPlayerStanceInfo->customStrafeMultiplier : m_params.strafeMultiplier;
		float paramsBackwardMul = (pPlayerStanceInfo->customBackwardsMultiplier > 0.0f) ? pPlayerStanceInfo->customBackwardsMultiplier : m_params.backwardMultiplier;
		float backwardMul = 1.0f;

		//going back?
		if (m_player.IsPlayer())	//[Mikko] Do not limit backwards movement when controlling AI.
		{
			if (desiredVelocityClamped.y < 0.0f)
			{
				backwardMul = LERP(backwardMul, paramsBackwardMul, -desiredVelocityClamped.y);
			}
		}

		// Custom walk, run or sprint speeds set up for this stance (in lua file)?
		// --> Use these fixed speeds instead of speed being linearly interpolated
		float velocityLength = desiredVelocityClamped.GetLength2D();

		if ((velocityLength > 0.0f && velocityLength < 0.7f) && pPlayerStanceInfo->walkSpeed > 0.0f)
		{
			// Walk?
			customScale = pPlayerStanceInfo->walkSpeed;
		}
		else if (pPlayerStanceInfo->runSpeed > 0.0f && !(m_actions & ACTION_SPRINT))
		{
			// Run?
			customScale = pPlayerStanceInfo->runSpeed;
		}
		else if (pPlayerStanceInfo->sprintSpeed > 0.0f && pPlayerStanceInfo->runSpeed > 0.0f && (m_actions & ACTION_SPRINT))
		{
			// Sprint?
			customScale = pPlayerStanceInfo->runSpeed;
			// modify the sprint multiplier instead of setting the speed directly
			// this leaves the decision of whether or not to use sprinting with the original code
			paramsSprintMul = pPlayerStanceInfo->sprintSpeed / pPlayerStanceInfo->runSpeed;
		}
		if (customScale > 0.0f)
		{
			desiredVelocityClamped.Normalize();
		}

		move += m_baseQuat.GetColumn0() * desiredVelocityClamped.x * strafeMul * backwardMul;
		move += m_baseQuat.GetColumn1() * desiredVelocityClamped.y * backwardMul;
	}

	float sprintMult = 1.0f;

	//ai can set a custom sprint value, so don't cap the movement vector
	if (m_movement.sprint <= 0.0f)
	{
		//cap the movement vector to max 1
		float moveModule(move.len());

		//[Mikko] Do not limit backwards movement when controlling AI, otherwise it will disable sprinting.
		if (m_player.IsPlayer())
		{
			//^^[Stas] Added this hack, other clients are not AIs
			if ( moveModule > 1.0f)
			{
				move /= moveModule;
			}
		}

		//move *= m_animParams.runSpeed/GetStanceMaxSpeed(m_stance);
		bool speedMode = false;

		if (gEnv->bMultiplayer)
		{
			if ((m_actions & ACTION_SPRINT) && !m_player.m_stats.bIgnoreSprinting)
			{
				move *= paramsSprintMul * sprintMult ;
			}
		}
		else
		{
			if (m_actions & ACTION_SPRINT && (!speedMode || sprintMult > 1.0f) && !m_player.m_stats.bIgnoreSprinting)// && m_player.GetStance() == STANCE_STAND)
			{
				move *= paramsSprintMul * sprintMult ;
			}
		}
	}

	//player movement don't need the m_frameTime, its handled already in the physics
	float scale = pPlayerStanceInfo->maxSpeed; //m_player.GetStanceMaxSpeed(m_player.GetStance());

	if (m_player.IsClient() && !gEnv->bMultiplayer)
	{
		if (customScale > 0.0f)
		{
			scale = customScale;
			move *= scale;
		}
		else
		{
			move *= scale * 0.75f;
		}
	}
	else
	{
		move *= scale;
	}

	//when using gravity boots speed can be slowed down
	if (m_player.GravityBootsOn())
	{
		move *= m_params.gravityBootsMultipler;
	}

	// Danny todo: This is a temporary workaround (but generally better than nothing I think)
	// to stop the AI movement from getting changed beyond recognition under normal circumstances.
	// If the movement request gets modified then it invalidates the prediciton made by AI, and thus
	// the choice of animation/parameters.
	//-> please adjust the prediction
	if (m_player.IsPlayer())
	{
		AdjustMovementForEnvironment( move, (m_actions & ACTION_SPRINT) != 0 );
	}

	//adjust prone movement for slopes
	if (m_player.GetStance() == STANCE_PRONE && move.len2() > 0.01f)
	{
		float slopeRatio(1.0f - m_stats.groundNormal.z * m_stats.groundNormal.z);
		slopeRatio *= slopeRatio;
		Vec3 terrainTangent((Vec3(0, 0, 1) % m_stats.groundNormal) % m_stats.groundNormal);

		if(slopeRatio > 0.5f && move.z > 0.0f)	//emergence stop when going up extreme walls
		{
			if(slopeRatio > 0.7f)
			{
				move *= 0.0f;
			}
			else
			{
				move *= ((0.7f - slopeRatio) * 5.0f);
			}
		}
		else
		{
			move *= 1.0f - min(1.0f, m_params.slopeSlowdown * slopeRatio * max(0.0f, -(terrainTangent * move.GetNormalizedSafe(ZERO))));
			//
			move += terrainTangent * slopeRatio * m_player.GetStanceMaxSpeed(m_player.GetStance());
		}
	}

	//only the Z component of the basematrix, handy with flat speeds,jump and gravity
	Matrix33 baseMtxZ(Matrix33(m_baseQuat) * Matrix33::CreateScale(Vec3(0, 0, 1)));
	m_request.type = eCMT_Normal;
	Vec3 jumpVec(0, 0, 0);
	//jump?
	//FIXME: I think in zeroG should be possible to jump to detach from the ground, for now its like this since its the easiest fix
	bool allowJump = !m_jumped || m_movement.strengthJump;
	bool dt_jumpCondition = m_movement.strengthJump;
	bool isRemoteClient = !gEnv->bServer && !m_player.IsClient();
	/*
			// TODO: Graph has broken, appearantly, so we can't use this atm.
			// Also, there's already a delay between jumps.
			const char* AGAllowJump = player.GetAnimationGraphState()->QueryOutput("AllowJump");
			if (strcmp(AGAllowJump, "1") != 0)
				allowJump = false;
	*/
	bool debugJumping = (g_pGameCVars->pl_debug_jumping != 0);

	if (m_movement.jump && allowJump)
	{
		if ((m_stats.onGround > 0.2f || dt_jumpCondition) && m_player.GetStance() != STANCE_PRONE)
		{
			//float verticalMult(max(0.75f,1.0f-min(1.0f,m_stats.flatSpeed / GetStanceMaxSpeed(STANCE_STAND) * m_params.sprintMultiplier)));
			//mul * gravity * jump height
			float mult = 1.0f;
			//this is used to easily find steep ground
			float slopeDelta = (m_stats.inZeroG) ? 0.0f : (m_stats.upVector - m_stats.groundNormal).len();
			/*
						if(m_stats.inZeroG)
							m_request.type = eCMT_Impulse;//eCMT_JumpAccumulate;
						else
			*/
			{
				m_request.type = eCMT_JumpAccumulate;//eCMT_Fly;
				float g = m_stats.gravity.len();
				float t = 0.0f;

				if (g > 0.0f)
				{
					t = cry_sqrtf( 2.0f * g * m_params.jumpHeight * mult) / g - m_stats.inAir * 0.5f;
				}

				jumpVec += m_baseQuat.GetColumn2() * g * t;// * verticalMult;

				if (m_stats.groundNormal.len2() > 0.0f)
				{
					float vertical = CLAMP((m_stats.groundNormal.z - 0.25f) / 0.5f, 0.0f, 1.0f);
					Vec3 modifiedJumpDirection = LERP(m_stats.groundNormal, Vec3(0, 0, 1), vertical);
					jumpVec = modifiedJumpDirection * jumpVec.len();
				}
			}
			// Don't speed up...
			//move = m_stats.velocityUnconstrained * 1.0f;
			move -= move * baseMtxZ;

			//with gravity boots on, jumping will disable them for a bit.
			if (m_onGroundWBoots && m_player.GravityBootsOn())
			{
				m_onGroundWBoots = -0.5f;
				jumpVec += m_baseQuat.GetColumn2() * cry_sqrtf( 2.0f * 9.81f * m_params.jumpHeight );
			}
			else
			{
				m_jumped = true;
			}

			player.GetAnimationGraphState()->SetInput(player.m_inputAction, "jumpMP" );
			CPlayer *pPlayer = const_cast<CPlayer *>(&m_player);
			pPlayer->PlaySound(CPlayer::ESound_Jump);
			CALL_PLAYER_EVENT_LISTENERS(OnSpecialMove(pPlayer, IPlayerEventListener::eSM_Jump));

			if (m_jumped)
			{
				m_hasJumped = true;
			}

			// PLAYERPREDICTION
			if (m_player.IsClient())
			{
				m_player.HasJumped(jumpVec);
			}

			// ~PLAYERPREDICTION
		}
	}

	if(m_player.IsClient() && !gEnv->bMultiplayer)
	{
		move *= g_pGameCVars->g_walkMultiplier;    //global speed adjuster used by level design
	}

	//CryLogAlways("%s speed: %.1f  stanceMaxSpeed: %.1f  sprintMult: %.1f  suitSprintMult: %.1f", m_player.GetEntity()->GetName(), move.len(), scale, m_params.sprintMultiplier, sprintMult);
	//apply movement
	Vec3 desiredVel(0, 0, 0);
	Vec3 entityPos = m_player.GetEntity()->GetWorldPos();
	Vec3 entityRight(m_baseQuat.GetColumn0());

	if (m_stats.onGround || dt_jumpCondition)
	{
		desiredVel = move;
		{
			// Shallow water speed slowdown
			float shallowWaterMultiplier = 1.0f;

			if (player.IsPlayer())
			{
				shallowWaterMultiplier = g_pGameCVars->cl_shallowWaterSpeedMulPlayer;
			}
			else
			{
				shallowWaterMultiplier = g_pGameCVars->cl_shallowWaterSpeedMulAI;
			}

			shallowWaterMultiplier = CLAMP(shallowWaterMultiplier, 0.1f, 1.0f);

			if ((shallowWaterMultiplier < 1.0f) && (m_stats.relativeBottomDepth > 0.0f) && (m_stats.worldWaterLevel > entityPos.z))
			{
				float shallowWaterDepthSpan = (g_pGameCVars->cl_shallowWaterDepthHi - g_pGameCVars->cl_shallowWaterDepthLo);
				shallowWaterDepthSpan = max(0.1f, shallowWaterDepthSpan);
				float slowdownFraction = (m_stats.relativeBottomDepth - g_pGameCVars->cl_shallowWaterDepthLo) / shallowWaterDepthSpan;
				slowdownFraction = CLAMP(slowdownFraction, 0.0f, 1.0f);
				shallowWaterMultiplier = LERP(1.0f, shallowWaterMultiplier, slowdownFraction);
				desiredVel *= shallowWaterMultiplier;
			}
		}
	}
	else if (move.len2() > 0.01f) //"passive" air control, the player can air control as long as it is to decelerate
	{
		Vec3 currVelFlat(m_stats.velocity - m_stats.velocity * baseMtxZ);
		Vec3 moveFlat(move - move * baseMtxZ);
		float dot(currVelFlat.GetNormalizedSafe(ZERO) * moveFlat.GetNormalizedSafe(ZERO));

		if (dot < -0.001f)
		{
			desiredVel = (moveFlat - currVelFlat) * max(abs(dot) * 0.3f, 0.1f);
		}
		else
		{
			desiredVel = moveFlat * max(0.5f, 1.0f - dot);
		}

		float currVelModSq(currVelFlat.len2());
		float desiredVelModSq(desiredVel.len2());

		if (desiredVelModSq > currVelModSq)
		{
			desiredVel.Normalize();
			desiredVel *= max(1.5f, sqrtf(currVelModSq));
		}
	}

	// Slow down on sloped terrain, simply proportional to the slope.
	desiredVel *= m_stats.groundNormal.z;
	//be sure desired velocity is flat to the ground
	desiredVel -= desiredVel * baseMtxZ;
	Vec3 modifiedSlopeNormal = m_stats.groundNormal;

	if (m_player.IsPlayer())
	{
		float h = Vec2(modifiedSlopeNormal.x, modifiedSlopeNormal.y).GetLength(); // TODO: OPT: sqrt(x*x+y*y)
		float v = modifiedSlopeNormal.z;
		float slopeAngleCur = RAD2DEG(cry_atan2f(h, v));
		float slopeAngleHor = 10.0f;
		float slopeAngleVer = 50.0f;
		float slopeAngleFraction = CLAMP((slopeAngleCur - slopeAngleHor) / (slopeAngleVer - slopeAngleHor), 0.0f, 1.0f);
		slopeAngleFraction = cry_powf(slopeAngleFraction, 3.0f); // TODO: OPT: x*x*x
		float slopeAngleMod = LERP(0.0f, 90.0f, slopeAngleFraction);
		float c = cry_cosf(DEG2RAD(slopeAngleMod)); // TODO: OPT: sincos?
		float s = cry_sinf(DEG2RAD(slopeAngleMod));

		if (h > 0.0f)
		{
			modifiedSlopeNormal.x *= s / h;
			modifiedSlopeNormal.y *= s / h;
		}

		if (v > 0.0f)
		{
			modifiedSlopeNormal.z *= c / v;
		}

		if(!modifiedSlopeNormal.IsZero())
		{
			modifiedSlopeNormal.Normalize();
		}

		float alignment = modifiedSlopeNormal * desiredVel;
		alignment = min(0.0f, alignment);

		// Also affect air control (but not as much), to prevent jumping up against steep slopes.
		if (m_stats.onGround == 0.0f)
		{
			float noControlBlend = 1.0f - CLAMP(modifiedSlopeNormal.z / 0.01f, 0.0f, 1.0f);
			alignment *= LERP(0.7f, 1.0f, noControlBlend);
		}

		desiredVel -= modifiedSlopeNormal * alignment;

		/*
				float unconstrainedFallBlend = 1.0f - CLAMP(modifiedSlopeNormal.z / 0.05f, 0.0f, 1.0f);
				desiredVel = LERP(desiredVel, m_stats.velocityUnconstrained, unconstrainedFallBlend);
		/**/

		if (debugJumping)
		{
			m_player.DebugGraph_AddValue("GroundSlope", slopeAngleCur);
			m_player.DebugGraph_AddValue("GroundSlopeMod", slopeAngleMod);
		}
	}

	m_request.velocity = desiredVel + jumpVec;

	if(!m_stats.inZeroG && (m_movement.jump && (g_pGameCVars->dt_enable && m_stats.inAir > 0.3f)) && m_request.velocity.len() > 22.0f)	//cap maximum velocity when jumping (limits speed jump length)
	{
		m_request.velocity = m_request.velocity.normalized() * 22.0f;
	}

	if (debugJumping)
	{
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(entityPos, ColorB(255, 255, 255, 255), entityPos + modifiedSlopeNormal, ColorB(255, 255, 0, 255), 2.0f);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(entityPos + Vec3(0, 0, 2), ColorB(255, 255, 255, 255), entityPos + Vec3(0, 0, 2) + desiredVel, ColorB(0, 255, 0, 255), 2.0f);
	}

	if (debugJumping)
	{
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(entityPos, ColorB(255, 255, 255, 255), entityPos + jumpVec, ColorB(0, 255, 255, 255), 2.0f);
	}

	if (debugJumping)
	{
		gEnv->pRenderer->DrawLabel(entityPos - entityRight * 1.0f + Vec3(0, 0, 3.0f), 1.5f, "Velo[%2.3f = %2.3f, %2.3f, %2.3f]", m_request.velocity.len(), m_request.velocity.x, m_request.velocity.y, m_request.velocity.z);
	}

	m_velocity.Set(0, 0, 0);
}

void CPlayerMovement::AdjustMovementForEnvironment( Vec3 &move, bool sprinting )
{
	//player is slowed down by carrying heavy objects (max. 33%)
	float massFactor = m_player.GetMassFactor();
	move *= massFactor;
}

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
void CPlayerMovement::ProcessTurning()
{
	if (m_stats.isRagDoll || (m_player.m_stats.isFrozen.Value() || m_stats.isOnLadder/*&& !m_player.IsPlayer()*/))
	{
		return;
	}

	const bool ROTATION_AFFECTS_THIRD_PERSON_MODEL = true;
	//	static const float ROTATION_SPEED = 23.3f;
	Quat entityRot = m_player.GetEntity()->GetWorldRotation().GetNormalized();
	Quat inverseEntityRot = entityRot.GetInverted();

	// TODO: figure out a way to unify this
	if (m_player.IsClient())
	{
		Vec3 right = m_turnTarget.GetColumn0();
		Vec3 up = m_upVector.GetNormalized();
		Vec3 forward = (up % right).GetNormalized();
		m_turnTarget = Quat( Matrix33::CreateFromVectors(forward % up, forward, up) );

		if (ROTATION_AFFECTS_THIRD_PERSON_MODEL)
		{
			if (m_stats.inAir && m_stats.inZeroG)
			{
				m_turnTarget = m_baseQuat;
				m_request.rotation = inverseEntityRot * m_turnTarget;
			}
			else
			{
				/*float turn = m_movement.deltaAngles.z;
				m_turnTarget = Quat::CreateRotationZ(turn) * m_turnTarget;
				Quat turnQuat = inverseEntityRot * m_turnTarget;

				Vec3 epos(m_player.GetEntity()->GetWorldPos()+Vec3(1,0,1));
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(epos, ColorB(255,255,0,100), epos + m_turnTarget.GetColumn1()*2.0f, ColorB(255,0,255,100));*/
				/*
				float turnAngle = cry_acosf( CLAMP( FORWARD_DIRECTION.Dot(turn * FORWARD_DIRECTION), -1.0f, 1.0f ) );
				float turnRatio = turnAngle / (ROTATION_SPEED * m_frameTime);
				if (turnRatio > 1)
					turnQuat = Quat::CreateSlerp( Quat::CreateIdentity(), turnQuat, 1.0f/turnRatio );
				*/
				//float white[4] = {1,1,1,1};
				//gEnv->pRenderer->Draw2dLabel( 100, 50, 4, white, false, "%f %f %f %f", turnQuat.w, turnQuat.v.x, turnQuat.v.y, turnQuat.v.z );
				//m_request.turn = turnQuat;
				m_request.rotation = inverseEntityRot * m_baseQuat;
				m_request.rotation.Normalize();
			}
		}
	}
	else
	{
		/*Vec3 right = entityRot.GetColumn0();
		Vec3 up = m_upVector.GetNormalized();
		Vec3 forward = (up % right).GetNormalized();
		m_turnTarget = GetQuatFromMat33( Matrix33::CreateFromVectors(forward%up, forward, up) );
		float turn = m_movement.deltaAngles.z;
		if (fabsf(turn) > ROTATION_SPEED * m_frameTime)
			turn = ROTATION_SPEED * m_frameTime * (turn > 0.0f? 1.0f : -1.0f);*/
		if (1)//m_stats.speedFlat>0.5f)
		{
			m_request.rotation = inverseEntityRot * m_baseQuat;//(m_turnTarget * Quat::CreateRotationZ(turn));
			m_request.rotation.Normalize();
		}
		else
		{
			m_request.rotation = inverseEntityRot * Quat::CreateRotationZ(gf_PI);
		}
	}

	if (m_player.IsPlayer() && (g_pGameCVars->ca_GameControlledStrafingPtr->GetIVal() != 0) && (g_pGameCVars->ac_enableProceduralLeaning == 0.0f))
	{
		float turningSpeed = m_frameTime > 0.0f ? (fabs(RAD2DEG(m_request.rotation.GetRotZ())) / m_frameTime) : 0.0f;
		float turningSpeedMin = 30.0f;
		float turningSpeedMax = 180.0f;
		float turningSpeedFraction = CLAMP((turningSpeed - turningSpeedMin) / (turningSpeedMax - turningSpeedMin), 0.0f, 1.0f);
		float travelSpeedScale = LERP(1.0f, CLAMP(g_pGameCVars->pl_curvingSlowdownSpeedScale, 0.0f, 1.0f), turningSpeedFraction);
		m_request.velocity *= travelSpeedScale;
	}

	// MERGE : (MATT) This is a little convoluted - might be better done with default cvar setting of 0.8 {2008/01/25:18:48:09}
	m_request.proceduralLeaning = (g_pGameCVars->ac_enableProceduralLeaning > 0.0f ? 0.8f : 0.0f);
	/*
		Vec3 pos = m_player.GetEntity()->GetWorldPos();
		Vec3 curDir = entityRot.GetColumn1();
		Vec3 wantDir = m_baseQuat.GetColumn1();
		Vec3 lftDir = entityRot.GetColumn0();
		float rot = m_request.rotation.GetRotZ();
		gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags( e_Def3DPublicRenderflags );
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(pos, ColorB(255,255,0,255), pos+curDir, ColorB(255,255,0,255), 2.0f);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(pos, ColorB(255,0,255,255), pos+wantDir, ColorB(255,0,255,255), 2.0f);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(pos+curDir, ColorB(0,255,255,255), pos+curDir+lftDir*rot, ColorB(0,255,255,255), 2.0f);
	*/
}

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------

void CPlayerMovement::ProcessMovementOnLadder(CPlayer &player)
{
	if(m_stats.isExitingLadder)
	{
		if(gEnv->IsClient())
		{
			IAnimatedCharacter *pAC = m_player.GetAnimatedCharacter();
			IAnimationGraphState *pAGState = pAC ? pAC->GetAnimationGraphState() : NULL;
			const char *agOutputOnLadder = pAGState ? pAGState->QueryOutput("OnLadder") : NULL;
			bool onLadder = agOutputOnLadder && agOutputOnLadder[0] == '1';

			if(!onLadder)
			{
				player.RequestLeaveLadder(CPlayer::eLAT_ReachedEnd);
			}
		}

		return;
	}

	float bottomDist = (m_stats.ladderBottom - player.GetEntity()->GetWorldPos()).len();
	bottomDist += g_pGameCVars->pl_ladder_animOffset; // -0.08f; //0.012f;   //Offset to make the anim match better...		(depends on animation and ladder, they should be all exactly the same)
	CPlayer::ELadderDirection eLDir = (m_movement.desiredVelocity.y >= 0.0f ? CPlayer::eLDIR_Up : CPlayer::eLDIR_Down);

	if (player.IsEnteringLadder())
	{
		// slide, instead of teleporting the character, but already update the animation
		// switch over to climb once done
		const float moveSpeed = 3.0f; // 3 meters per second, 0.33 seconds for 1 meter distance
		// what needs to be updated this frame?
		bool updatePos(true);
		// calculate the distance between player position and target location, and rotation
		Vec3 vecToTarget = m_stats.ladderEnterLocation.t - player.GetEntity()->GetWorldPos();
		Quat rotationToTarget = m_stats.ladderEnterLocation.q * !(player.GetEntity()->GetWorldRotation());
		rotationToTarget.NormalizeSafe();
		// is both close enough (with regards to frame time) --> set final pos and finish
		float distanceToTarget = vecToTarget.GetLength();

		if (distanceToTarget < (moveSpeed * m_frameTime))
		{
			player.GetEntity()->SetPos(m_stats.ladderEnterLocation.t);
			player.GetEntity()->SetRotation(m_stats.ladderEnterLocation.q);
			updatePos = false;
		}

		// calculate the movement for the next frame as needed
		if (updatePos)
		{
			Vec3 newPos = vecToTarget.GetNormalized() * moveSpeed * m_frameTime;
			float perc = 1.0f;

			if (distanceToTarget != 0.0f)
			{
				perc = newPos.GetLength() / distanceToTarget;
			}

			newPos += player.GetEntity()->GetWorldPos();
			player.GetEntity()->SetPos(newPos);
			//m_request.type = eCMT_Fly;
			//m_request.velocity = newPos;
			// Rotation Amount is calculated relative to the movement, so that both finish at the same time
			// perc is a percentage value (0-1) of how much of the remaining distance was covered this frame, which
			// is dependent on the overall distance and the movement speed (moveSpeed).
			// So rotation will cover the same percentage of the remaining rotation this frame.
			Quat newRot = Quat(IDENTITY); // player.GetEntity()->GetWorldRotation();
			newRot =  Quat::CreateSlerp(player.GetEntity()->GetWorldRotation(), m_stats.ladderEnterLocation.q, perc);
			newRot.NormalizeSafe();
			player.GetEntity()->SetRotation(newRot);
		}

		// Tell the player we are done entering the ladder IF we are
		if (!updatePos)
		{
			player.FinishedLadderEntering();
		}

		m_velocity.Set(0, 0, 0);
		bottomDist = (m_stats.ladderBottom - m_stats.ladderEnterLocation.t).len();
		bottomDist += g_pGameCVars->pl_ladder_animOffset; //Offset to make the anim match better...
		float animTime = bottomDist - cry_floorf(bottomDist);
		// stationary so that the ag input isn't changed
		player.UpdateLadderAnimation(CPlayer::eLS_Climb, CPlayer::eLDIR_Stationary /*eLDir*/, animTime);
		return;
	}

	Vec3 move(m_stats.ladderTop - m_stats.ladderBottom);
	move.NormalizeSafe();
	float topDist = (m_stats.ladderTop - player.GetEntity()->GetWorldPos()).len();
	bool updateAnimation(true);
	AdjustPlayerPositionOnLadder(player);

	//Just another check
	if(player.IsClient() && m_movement.desiredVelocity.y < -0.01f)
	{
		// check collision with terrain/static objects when moving down, some ladders are very badly placed ;(
		// player will need to detach
		ray_hit hit;
		static const int obj_types = ent_static | ent_terrain;
		static const unsigned int flags = rwi_stop_at_pierceable | rwi_colltype_any;
		Vec3 currentPos = player.GetEntity()->GetWorldPos();
		currentPos.z += 0.15f;
		int rayHits = gEnv->pPhysicalWorld->RayWorldIntersection( currentPos, m_stats.ladderUpDir * -0.3f, obj_types, flags, &hit, 1, player.GetEntity()->GetPhysics() );

		if(rayHits != 0)
		{
			player.RequestLeaveLadder(CPlayer::eLAT_None);
			return;
		}
	}

	if (player.IsClient() && ((topDist < LADDER_TOP_DISTANCE && m_movement.desiredVelocity.y > 0.01f) || (bottomDist < 0.1f && m_movement.desiredVelocity.y < -0.01f)))
	{
		if(m_movement.desiredVelocity.y > 0.01f)
		{
			// check if player can move forward from top of ladder before getting off. If they can't,
			//	they'll need to strafe / jump off.
			ray_hit hit;
			static const int obj_types = ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid;
			static const unsigned int flags = rwi_stop_at_pierceable | rwi_colltype_any;
			static float backDist = 0.15f;
			Vec3 currentPos = player.m_stats.ladderTop + backDist * player.m_stats.ladderOrientation;
			Vec3 newPos = player.m_stats.ladderTop - player.m_stats.ladderOrientation;
			currentPos.z += 0.35f;
			newPos.z += 0.35f;

			if(g_pGameCVars->pl_debug_ladders != 0)
			{
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(currentPos, ColorF(1, 1, 1, 1), newPos, ColorF(1, 1, 1, 1));
			}

			bool rayHitAny = 0 != gEnv->pPhysicalWorld->RayWorldIntersection( currentPos, newPos - currentPos, obj_types, flags, &hit, 1, player.GetEntity()->GetPhysics() );

			if (!rayHitAny /*|| abs(hit.n.z) > 0.1f*/)
			{
				player.RequestLeaveLadder(CPlayer::eLAT_ExitTop);
				return;
			}
			else
			{
				m_movement.desiredVelocity.y = 0.0f;

				if(g_pGameCVars->pl_debug_ladders != 0)
				{
					float white[] = {1, 1, 1, 1};
					gEnv->pRenderer->Draw2dLabel(50, 125, 2.0f, white, false, "CLAMPING");
				}
			}
		}
		else
		{
			player.RequestLeaveLadder(CPlayer::eLAT_None);
			return;
		}
	}

	//Strafe
	if(m_stats.ladderAction == CPlayer::eLAT_StrafeRight || m_stats.ladderAction == CPlayer::eLAT_StrafeLeft)
	{
		player.RequestLeaveLadder(static_cast<CPlayer::ELadderActionType>(m_stats.ladderAction));
		return;
	}

	if(g_pGameCVars->pl_debug_ladders != 0)
	{
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(m_stats.ladderBottom, 0.12f, ColorB(0, 255, 0, 100) );
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(m_stats.ladderTop, 0.12f, ColorB(0, 255, 0, 100) );
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(player.GetEntity()->GetWorldPos(), 0.12f, ColorB(255, 0, 0, 100) );
		float white[4] = {1, 1, 1, 1};
		gEnv->pRenderer->Draw2dLabel(50, 50, 2.0f, white, false, "Top Dist: %f - Bottom Dist: %f - Desired Vel: %f", topDist, bottomDist, m_movement.desiredVelocity.y);
		gEnv->pRenderer->Draw2dLabel(50, 75, 2.0f, white, false, "Ladder Orientation (%f, %f, %f) - Ladder Up Direction (%f, %f, %f)", m_stats.ladderOrientation.x, m_stats.ladderOrientation.y, m_stats.ladderOrientation.z, m_stats.ladderUpDir.x, m_stats.ladderUpDir.y, m_stats.ladderUpDir.z);
	}

	move *= m_movement.desiredVelocity.y * 0.5f; // * (dirDot>0.0f?1.0f:-1.0f) * min(1.0f,fabs(dirDot)*5);
	//cap the movement vector to max 1
	float moveModule(move.len());

	if (moveModule > 1.0f)
	{
		move /= moveModule;
	}

	move *= m_player.GetStanceMaxSpeed(STANCE_STAND) * 0.5f;

	//player.m_stats.bSprinting = false;		//Manual update here (if not suit doensn't decrease energy and so on...)
	if (m_actions & ACTION_SPRINT)
	{
		if((move.len2() > 0.1f))
		{
			move *= 1.2f;
			//player.m_stats.bSprinting = true;
		}
	}

	if(m_actions & ACTION_JUMP)
	{
		player.RequestLeaveLadder(CPlayer::eLAT_Jump);
		updateAnimation = false;
		move += Vec3(0.0f, 0.0f, 3.0f);
	}

	if(g_pGameCVars->pl_debug_ladders != 0)
	{
		float white[] = {1, 1, 1, 1};
		gEnv->pRenderer->Draw2dLabel(50, 100, 2.0f, white, false, "Move (%.2f, %.2f, %.2f)", move.x, move.y, move.z);
	}

	m_request.type = eCMT_Fly;
	m_request.velocity = move;
	//Animation and movement synch
	bottomDist += move.z * m_frameTime; // predict the position after the update
	float animTime = bottomDist - cry_floorf(bottomDist);

	if (updateAnimation)
		if(!player.UpdateLadderAnimation(CPlayer::eLS_Climb, eLDir, animTime))
		{
			return;
		}

	m_velocity.Set(0, 0, 0);
}

//---------------------------------------------------------
void CPlayerMovement::AdjustPlayerPositionOnLadder(CPlayer &player)
{
	IEntity *pEntity = player.GetEntity();

	if(pEntity)
	{
		//In some cases the rotation is not correct, force it if neccessary
		if(!Quat::IsEquivalent(pEntity->GetRotation(), m_stats.playerRotation))
		{
			pEntity->SetRotation(Quat(Matrix33::CreateOrientation(-m_stats.ladderOrientation, m_stats.ladderUpDir, gf_PI)));
		}

		Vec3 projected = ProjectPointToLine(pEntity->GetWorldPos(), m_stats.ladderBottom, m_stats.ladderTop);

		//Same problem with the position
		if(!pEntity->GetWorldPos().IsEquivalent(projected))
		{
			Matrix34 finalPos = pEntity->GetWorldTM();
			finalPos.SetTranslation(projected);
			pEntity->SetWorldTM(finalPos);
		}
	}
}
