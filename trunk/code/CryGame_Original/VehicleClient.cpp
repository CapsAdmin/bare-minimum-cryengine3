/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a class which handle client actions on vehicles.

-------------------------------------------------------------------------
History:
- 17:10:2006: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "IGame.h"
#include "IActorSystem.h"
#include "IVehicleSystem.h"
#include "VehicleClient.h"
#include "GameCVars.h"
#include "Game.h"
#include "Weapon.h"
#include "Player.h"

//------------------------------------------------------------------------
bool CVehicleClient::Init()
{
	m_actionNameIds.clear();
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_exit", eVAI_Exit));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_changeseat", eVAI_ChangeSeat));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_changeseat1", eVAI_ChangeSeat1));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_changeseat2", eVAI_ChangeSeat2));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_changeseat3", eVAI_ChangeSeat3));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_changeseat4", eVAI_ChangeSeat4));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_changeseat5", eVAI_ChangeSeat5));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_changeview", eVAI_ChangeView));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_rearview", eVAI_RearView));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_viewoption", eVAI_ViewOption));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_zoom_in", eVAI_ZoomIn));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_zoom_out", eVAI_ZoomOut));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_attack1", eVAI_Attack1));
	m_actionNameIds.insert(TActionNameIdMap::value_type("zoom", eVAI_Attack2));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_attack2", eVAI_Attack2));
	m_actionNameIds.insert(TActionNameIdMap::value_type("xi_zoom", eVAI_Attack2));
	m_actionNameIds.insert(TActionNameIdMap::value_type("xi_v_attack1", eVAI_Attack1));
	m_actionNameIds.insert(TActionNameIdMap::value_type("xi_v_attack2", eVAI_Attack2));
	m_actionNameIds.insert(TActionNameIdMap::value_type("firemode", eVAI_FireMode));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_lights", eVAI_ToggleLights));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_horn", eVAI_Horn));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_rotateyaw", eVAI_RotateYaw));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_rotatepitch", eVAI_RotatePitch));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_moveforward", eVAI_MoveForward));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_moveback", eVAI_MoveBack));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_moveup", eVAI_MoveUp));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_movedown", eVAI_MoveDown));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_turnleft", eVAI_TurnLeft));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_turnright", eVAI_TurnRight));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_strafeleft", eVAI_StrafeLeft));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_straferight", eVAI_StrafeRight));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_rollleft", eVAI_RollLeft));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_rollright", eVAI_RollRight));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_rotateroll", eVAI_RotateRoll));
	m_actionNameIds.insert(TActionNameIdMap::value_type("xi_v_rotateyaw", eVAI_XIRotateYaw));
	m_actionNameIds.insert(TActionNameIdMap::value_type("xi_v_rotatepitch", eVAI_XIRotatePitch));
	m_actionNameIds.insert(TActionNameIdMap::value_type("xi_v_movey", eVAI_XIMoveY));
	m_actionNameIds.insert(TActionNameIdMap::value_type("xi_v_movex", eVAI_XIMoveX));
	m_actionNameIds.insert(TActionNameIdMap::value_type("xi_v_rotateroll", eVAI_XIRotateRoll));
	m_actionNameIds.insert(TActionNameIdMap::value_type("xi_v_accelerate", eVAI_XIAccelerate));
	m_actionNameIds.insert(TActionNameIdMap::value_type("xi_v_deccelerate", eVAI_XIDeccelerate));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_pitchup", eVAI_PitchUp));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_pitchdown", eVAI_PitchDown));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_brake", eVAI_Brake));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_afterburner", eVAI_AfterBurner));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_boost", eVAI_Boost));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_debug_1", eVAI_Debug_1));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_debug_2", eVAI_Debug_2));
	m_xiRotation.Set(0, 0, 0);
	m_bMovementFlagForward = false;
	m_bMovementFlagBack = false;
	m_bMovementFlagRight = false;
	m_bMovementFlagLeft = false;
	m_fLeftRight = 0.f;
	m_fForwardBackward = 0.f;
	m_tp = false;
	return true;
}

//------------------------------------------------------------------------
void CVehicleClient::Reset()
{
	m_tp = false;
}

//------------------------------------------------------------------------
void CVehicleClient::OnAction(IVehicle *pVehicle, EntityId actorId, const ActionId &actionId, int activationMode, float value)
{
	assert(pVehicle);

	if (!pVehicle)
	{
		return;
	}

	TActionNameIdMap::const_iterator ite = m_actionNameIds.find(actionId);

	if (ite == m_actionNameIds.end())
	{
		return;
	}

	IVehicleMovement *pMovement = pVehicle->GetMovement();
	const bool isAir = pMovement && pMovement->GetMovementType() == IVehicleMovement::eVMT_Air;
	bool isThirdPerson = true;
	bool isDriver = true;

	if (IVehicleSeat *pCurrentSeat = pVehicle->GetSeatForPassenger(actorId))
	{
		isDriver = pCurrentSeat->IsDriver();

		if (IVehicleView *pCurrentSeatView = pCurrentSeat->GetView(pCurrentSeat->GetCurrentView()))
		{
			isThirdPerson = pCurrentSeatView->IsThirdPerson();
		}
	}

	switch (ite->second)
	{
		case (eVAI_XIMoveX):
			{
				if(pMovement && pMovement->GetMovementType() == IVehicleMovement::eVMT_Air)
				{
					//strafe instead of turning for air vehicles
					if(value > 0.f)
					{
						pVehicle->OnAction(eVAI_StrafeRight, eAAM_OnPress, value, actorId);
						m_bMovementFlagRight = true;
					}
					else if(value == 0.f)
					{
						if(m_bMovementFlagRight)
						{
							pVehicle->OnAction(eVAI_StrafeRight, eAAM_OnRelease, 0.f, actorId);
							m_bMovementFlagRight = false;
						}
						else if(m_bMovementFlagLeft)
						{
							pVehicle->OnAction(eVAI_StrafeLeft, eAAM_OnRelease, 0.f, actorId);
							m_bMovementFlagLeft = false;
						}
					}
					else//value<0
					{
						pVehicle->OnAction(eVAI_StrafeLeft, eAAM_OnPress, -value, actorId);
						m_bMovementFlagLeft = true;
					}
				}
				else
				{
					if(value > 0.f)
					{
						pVehicle->OnAction(eVAI_TurnRight, eAAM_OnPress, value, actorId);
						m_bMovementFlagRight = true;
					}
					else if(value == 0.f)
					{
						if(m_bMovementFlagRight)
						{
							pVehicle->OnAction(eVAI_TurnRight, eAAM_OnRelease, 0.f, actorId);
							m_bMovementFlagRight = false;
						}
						else if(m_bMovementFlagLeft)
						{
							pVehicle->OnAction(eVAI_TurnLeft, eAAM_OnRelease, 0.f, actorId);
							m_bMovementFlagLeft = false;
						}
					}
					else//value<0
					{
						pVehicle->OnAction(eVAI_TurnLeft, eAAM_OnPress, -value, actorId);
						m_bMovementFlagLeft = true;
					}
				}

				break;
			}

		case (eVAI_XIMoveY):
			{
				EVehicleActionIds eForward = eVAI_MoveForward;
				EVehicleActionIds eBack = eVAI_MoveBack;

				if(!strcmp("Asian_helicopter", pVehicle->GetEntity()->GetClass()->GetName()))
				{
					eForward = eVAI_MoveUp;
					eBack = eVAI_MoveDown;
				}

				if(value > 0.f)
				{
					pVehicle->OnAction(eForward, eAAM_OnPress, value, actorId);
					m_bMovementFlagForward = true;
				}
				else if(value == 0.f)
				{
					if(m_bMovementFlagForward)
					{
						pVehicle->OnAction(eForward, eAAM_OnRelease, 0.f, actorId);
						m_bMovementFlagForward = false;
					}
					else if(m_bMovementFlagBack)
					{
						pVehicle->OnAction(eBack, eAAM_OnRelease, 0.f, actorId);
						m_bMovementFlagBack = false;
					}
				}
				else//value<0.f
				{
					pVehicle->OnAction(eBack, eAAM_OnPress, -value, actorId);
					m_bMovementFlagBack = true;
				}

				break;
			}

		case (eVAI_XIRotateRoll):
			{
				pVehicle->OnAction(eVAI_RotateRoll, eAAM_OnPress, value / 5.f, actorId);
				break;
			}

		case (eVAI_XIRotateYaw):
			{
				if (isAir && (isDriver || isThirdPerson))
				{
					pVehicle->OnAction(eVAI_RotateYaw, eAAM_OnPress, value / 5.f, actorId);
				}
				else
				{
					// action is sent to vehicle in PreUpdate, so it is repeated every frame.
					m_xiRotation.x = (value * value * value);
				}

				break;
			}

		case (eVAI_XIRotatePitch):
			{
				if (isAir && (isDriver || isThirdPerson))
				{
					pVehicle->OnAction(eVAI_RotatePitch, eAAM_OnPress, value / 5.f, actorId);
				}
				else
				{
					// action is sent to vehicle in PreUpdate, so it is repeated every frame
					m_xiRotation.y = -(value * value * value);

					if(g_pGameCVars->cl_invertController)
					{
						m_xiRotation.y *= -1;
					}
				}

				break;
			}

		case (eVAI_RotateYaw):
			{
				// attempt to normalise the input somewhat (to bring it in line with controller input).
				const float	scale = 0.75f, limit = 6.0f;
				value = clamp_tpl<float>(scale * value * gEnv->pTimer->GetFrameTime(), -limit, limit);

				if (isAir || isThirdPerson)
				{
					value *= 10.f;
				}

				pVehicle->OnAction(ite->second, activationMode, value, actorId);
				break;
			}

		case (eVAI_RotatePitch):
			{
				// attempt to normalise the input somewhat (to bring it in line with controller input).
				const float	scale = 0.75f, limit = 6.0f;
				value = clamp_tpl<float>(scale * value * gEnv->pTimer->GetFrameTime(), -limit, limit);

				if (g_pGameCVars->cl_invertMouse)
				{
					value *= -1.f;
				}

				if (isAir || isThirdPerson)
				{
					value *= 10.f;
				}

				pVehicle->OnAction(ite->second, activationMode, value, actorId);
				break;
			}

		case (eVAI_TurnLeft):
			{
				if (activationMode == eAAM_OnPress || activationMode == eAAM_OnRelease)
				{
					m_fLeftRight -= value * 2.f - 1.f;
				}

				m_fLeftRight = CLAMP(m_fLeftRight, -1.0f, 1.0f);
				pVehicle->OnAction(ite->second, activationMode, -m_fLeftRight, actorId);
				break;
			}

		case (eVAI_TurnRight):
			{
				if (activationMode == eAAM_OnPress || activationMode == eAAM_OnRelease)
				{
					m_fLeftRight += value * 2.f - 1.f;
				}

				m_fLeftRight = CLAMP(m_fLeftRight, -1.0f, 1.0f);
				pVehicle->OnAction(ite->second, activationMode, m_fLeftRight, actorId);
				break;
			}

		case (eVAI_MoveForward):
			{
				if (activationMode == eAAM_OnPress || activationMode == eAAM_OnRelease)
				{
					m_fForwardBackward += value * 2.f - 1.f;
				}
				else
				{
					m_fForwardBackward = value;
				}

				if(activationMode == eAAM_OnRelease)
				{
					m_fForwardBackward = CLAMP(m_fForwardBackward, 0.0f, 1.0f);
				}
				else
				{
					m_fForwardBackward = CLAMP(m_fForwardBackward, -1.0f, 1.0f);
				}

				pVehicle->OnAction(ite->second, activationMode, m_fForwardBackward, actorId);
				break;
			}

		case (eVAI_MoveBack):
			{
				if (activationMode == eAAM_OnPress || activationMode == eAAM_OnRelease)
				{
					m_fForwardBackward -= value * 2.f - 1.f;
				}
				else
				{
					m_fForwardBackward = -value;
				}

				if(activationMode == eAAM_OnRelease)
				{
					m_fForwardBackward = CLAMP(m_fForwardBackward, -1.0f, 0.0f);
				}
				else
				{
					m_fForwardBackward = CLAMP(m_fForwardBackward, -1.0f, 1.0f);
				}

				pVehicle->OnAction(ite->second, activationMode, -m_fForwardBackward, actorId);
				break;
			}

		case (eVAI_ZoomIn):
		case (eVAI_ZoomOut):
			break;

		default:
			pVehicle->OnAction(ite->second, activationMode, value, actorId);
			break;
	}
}

//------------------------------------------------------------------------

void CVehicleClient::PreUpdate(IVehicle *pVehicle, EntityId actorId, float frameTime)
{
	if(fabsf(m_xiRotation.x) > 0.001)
	{
		pVehicle->OnAction(eVAI_RotateYaw, eAAM_OnPress, m_xiRotation.x, actorId);
	}

	if(fabsf(m_xiRotation.y) > 0.001)
	{
		pVehicle->OnAction(eVAI_RotatePitch, eAAM_OnPress, m_xiRotation.y, actorId);
	}
}

//------------------------------------------------------------------------
void CVehicleClient::OnEnterVehicleSeat(IVehicleSeat *pSeat)
{
	m_bMovementFlagRight = m_bMovementFlagLeft = m_bMovementFlagForward = m_bMovementFlagBack = false;
	m_fLeftRight = m_fForwardBackward = 0.f;
	IVehicle *pVehicle = pSeat->GetVehicle();
	assert(pVehicle);
	IActorSystem *pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	assert(pActorSystem);
	IActor *pActor = pActorSystem->GetActor(pSeat->GetPassenger());

	if (pActor)
	{
		bool isThirdPerson = pActor->IsThirdPerson() || m_tp;
		TVehicleViewId viewId = InvalidVehicleViewId;
		TVehicleViewId firstViewId = InvalidVehicleViewId;

		while (viewId = pSeat->GetNextView(viewId))
		{
			if (viewId == firstViewId)
			{
				break;
			}

			if (firstViewId == InvalidVehicleViewId)
			{
				firstViewId = viewId;
			}

			if (IVehicleView *pView = pSeat->GetView(viewId))
			{
				if (pView->IsThirdPerson() == isThirdPerson)
				{
					break;
				}
			}
		}

		if (viewId != InvalidVehicleViewId)
		{
			pSeat->SetView(viewId);
		}

		// Set custom draw near z range for camera motion blur
		const bool bForceValue = true;
		const float customDrawNearZRange = 0.05f;
		gEnv->p3DEngine->SetPostEffectParam( "MotionBlur_DrawNearZRangeOverride", customDrawNearZRange, bForceValue );

		if(IActor *pPassengerActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pSeat->GetPassenger()))
		{
			if(pPassengerActor->IsPlayer())
			{
				EnableActionMaps(pSeat, true);
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleClient::OnExitVehicleSeat(IVehicleSeat *pSeat)
{
	if(pSeat && pSeat->IsDriver())
	{
		m_bMovementFlagRight = m_bMovementFlagLeft = m_bMovementFlagForward = m_bMovementFlagBack = false;
		m_fLeftRight = m_fForwardBackward = 0.f;
	}

	TVehicleViewId viewId = pSeat->GetCurrentView();

	if (viewId != InvalidVehicleViewId)
	{
		if (IVehicleView *pView = pSeat->GetView(viewId))
		{
			m_tp = pView->IsThirdPerson();
		}

		pSeat->SetView(InvalidVehicleViewId);
	}

	if(IActor *pPassengerActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pSeat->GetPassenger()))
	{
		if(pPassengerActor->IsPlayer())
		{
			EnableActionMaps(pSeat, false);
		}
	}

	// Reset custom draw near z range for camera motion blur
	const bool bForceValue = true;
	gEnv->p3DEngine->SetPostEffectParam( "MotionBlur_DrawNearZRangeOverride", 0.0f, bForceValue );
}


//------------------------------------------------------------------------
CVehicleClient::SVehicleClientInfo &CVehicleClient::GetVehicleClientInfo(IVehicle *pVehicle)
{
	IEntityClass *pClass = pVehicle->GetEntity()->GetClass();
	TVehicleClientInfoMap::iterator ite = m_vehiclesInfo.find(pClass);

	if (ite == m_vehiclesInfo.end())
	{
		// we need to add this class in our list
		SVehicleClientInfo clientInfo;
		clientInfo.seats.resize(pVehicle->GetSeatCount());
		TVehicleSeatClientInfoVector::iterator seatInfoIte = clientInfo.seats.begin();
		TVehicleSeatClientInfoVector::iterator seatInfoEnd = clientInfo.seats.end();
		TVehicleSeatId seatId = InvalidVehicleSeatId;

		for (; seatInfoIte != seatInfoEnd; ++seatInfoIte)
		{
			seatId++;
			SVehicleSeatClientInfo &seatInfo = *seatInfoIte;
			seatInfo.seatId = seatId;
			seatInfo.viewId = InvalidVehicleViewId;
		}

		m_vehiclesInfo.insert(TVehicleClientInfoMap::value_type(pClass, clientInfo));
		ite = m_vehiclesInfo.find(pClass);
	}

	// this will never happen
	assert(ite != m_vehiclesInfo.end());
	return ite->second;
}

//------------------------------------------------------------------------
CVehicleClient::SVehicleSeatClientInfo &
CVehicleClient::GetVehicleSeatClientInfo(SVehicleClientInfo &vehicleClientInfo, TVehicleSeatId seatId)
{
	TVehicleSeatClientInfoVector::iterator seatInfoIte = vehicleClientInfo.seats.begin();
	TVehicleSeatClientInfoVector::iterator seatInfoEnd = vehicleClientInfo.seats.end();

	for (; seatInfoIte != seatInfoEnd; ++seatInfoIte)
	{
		SVehicleSeatClientInfo &seatClientInfo = *seatInfoIte;

		if (seatClientInfo.seatId == seatId)
		{
			return *seatInfoIte;
		}
	}

	// will never happen, unless the vehicle has new seat created after the
	// game started
	assert(0);
	return vehicleClientInfo.seats[0];
}

void CVehicleClient::EnableActionMaps(IVehicleSeat *pSeat, bool enable)
{
	assert(pSeat);

	// illegal to change action maps in demo playback - Lin
	if (!IsDemoPlayback() && pSeat)
	{
		IActionMapManager *pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();
		CRY_ASSERT(pActionMapMan);
		pActionMapMan->EnableActionMap("player", !enable);
		EntityId passengerID = pSeat->GetPassenger();
		static ICVar *pVehicleGeneralActionMapCVar = gEnv->pConsole->GetCVar("g_vehicle_general_action_map");
		const char *szVehicleGeneralActionMap = pVehicleGeneralActionMapCVar
												? pVehicleGeneralActionMapCVar->GetString()
												: "vehicle_general";

		// now the general vehicle controls
		if (IActionMap *pActionMap = pActionMapMan->GetActionMap(szVehicleGeneralActionMap))
		{
			pActionMap->SetActionListener(enable ? passengerID : 0);
			pActionMapMan->EnableActionMap(szVehicleGeneralActionMap, enable);
		}

		// todo: if necessary enable the vehicle-specific action map here
		// specific controls for this vehicle seat
		const char *actionMap = pSeat->GetActionMap();

		if (actionMap && actionMap[0])
		{
			if (IActionMap *pActionMap = pActionMapMan->GetActionMap(actionMap))
			{
				pActionMap->SetActionListener(enable ? passengerID : 0);
				pActionMapMan->EnableActionMap(actionMap, enable);
			}
		}
	}
}

#include UNIQUE_VIRTUAL_WRAPPER(IVehicleClient)