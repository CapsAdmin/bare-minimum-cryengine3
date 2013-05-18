/*************************************************************************
Crytek Source File.
Copyright (C), Crytek GmbH, 2001-2012.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 25:6:2012   18:32 : Created by Mathieu Pinard based on C2 GameRules

*************************************************************************/
#include "StdAfx.h"
#include "GameRules.h"
#include "Player.h"
#include "IDeferredCollisionEvent.h"
#include "GameCVars.h"
#include "PlayerMovementController.h"

#define CCCPOINT(x) (void)(0)
#define CCCPOINT_IF(check, x) (void)(0)

//------------------------------------------------------------------------
void CGameRules::ClearAllMigratingPlayers(void)
{
	for (uint32 index = 0; index < m_migratingPlayerMaxCount; ++index)
	{
		m_pMigratingPlayerInfo[index].Reset();
	}
}

//------------------------------------------------------------------------
EntityId CGameRules::SetChannelForMigratingPlayer(const char *name, uint16 channelID)
{
	CryLog("CGameRules::SetChannelForMigratingPlayer, channel=%i, name=%s", channelID, name);

	for (uint32 index = 0; index < m_migratingPlayerMaxCount; ++index)
	{
		if (m_pMigratingPlayerInfo[index].InUse() && stricmp(m_pMigratingPlayerInfo[index].m_originalName, name) == 0)
		{
			m_pMigratingPlayerInfo[index].SetChannelID(channelID);
			return m_pMigratingPlayerInfo[index].m_originalEntityId;
		}
	}

	return 0;
}

//------------------------------------------------------------------------
void CGameRules::StoreMigratingPlayer(IActor *pActor)
{
	if (pActor == NULL)
	{
		GameWarning("Invalid data for migrating player");
		return;
	}

	IEntity *pEntity = pActor->GetEntity();
	EntityId id = pEntity->GetId();
	bool registered = false;
	uint16 channelId = pActor->GetChannelId();
	CRY_ASSERT(channelId);
	bool bShouldAdd = true;

	if (bShouldAdd && (!m_hostMigrationCachedEntities.empty()))
	{
		if (!stl::find(m_hostMigrationCachedEntities, pActor->GetEntityId()))
		{
			bShouldAdd = false;
		}
	}

	if (bShouldAdd)
	{
		for (uint32 index = 0; index < m_migratingPlayerMaxCount; ++index)
		{
			if (!m_pMigratingPlayerInfo[index].InUse())
			{
				m_pMigratingPlayerInfo[index].SetData(pEntity->GetName(), id, GetTeam(id), pEntity->GetWorldPos(), pEntity->GetWorldAngles(), pActor->GetHealth());
				m_pMigratingPlayerInfo[index].SetChannelID(channelId);
				registered = true;
				break;
			}
		}
	}

	pEntity->Hide(true);		// Hide the player, they will be unhidden when they rejoin

	if (!registered && bShouldAdd)
	{
		GameWarning("Too many migrating players!");
	}
}

//------------------------------------------------------------------------
void CGameRules::OnHostMigrationGotLocalPlayer(CPlayer *pPlayer)
{
	if (m_pHostMigrationParams)
	{
		pPlayer->SetMigrating(true);
	}
}

//------------------------------------------------------------------------
void CGameRules::OnHostMigrationStateChanged()
{
	CGame::EHostMigrationState migrationState = g_pGame->GetHostMigrationState();

	if (migrationState == CGame::eHMS_Resuming)
	{
		// Assume remaining players aren't going to make it
		const uint32 maxMigratingPlayers = m_migratingPlayerMaxCount;

		for (uint32 index = 0; index < maxMigratingPlayers; ++ index)
		{
			SMigratingPlayerInfo *pPlayerInfo = &m_pMigratingPlayerInfo[index];

			if (pPlayerInfo->InUse())
			{
				// Pretend the player has disconnected
				FakeDisconnectPlayer(pPlayerInfo->m_originalEntityId);
				pPlayerInfo->Reset();
			}
		}

		if (gEnv->bServer)
		{
			GetGameObject()->InvokeRMI(ClHostMigrationFinished(), NoParams(), eRMI_ToRemoteClients);
		}
	}
	else if (migrationState == CGame::eHMS_NotMigrating)
	{
		if (gEnv->bServer)
		{
			TPlayers players;
			GetPlayers(players);
			const int numPlayers = players.size();

			for (int i = 0; i < numPlayers; ++ i)
			{
				IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(players[i]);

				if (pActor)
				{
					pActor->SetMigrating(false);
				}
			}
		}
		else
		{
			CPlayer *pPlayer = static_cast<CPlayer *>(g_pGame->GetIGameFramework()->GetClientActor());

			if (pPlayer)
			{
				pPlayer->SetMigrating(false);
			}
		}

		CallOnForbiddenAreas("OnHostMigrationFinished");
		// Migration has finished, if we've still got client params then they won't be valid anymore
		SAFE_DELETE(m_pHostMigrationClientParams);
	}
	else if (migrationState == CGame::eHMS_WaitingForPlayers)
	{
		CallOnForbiddenAreas("OnHostMigrationStarted");
	}
}

//------------------------------------------------------------------------
int CGameRules::GetMigratingPlayerIndex(TNetChannelID channelID)
{
	int migratingPlayerIndex = -1;

	for (uint32 index = 0; index < m_migratingPlayerMaxCount; ++index)
	{
		if (m_pMigratingPlayerInfo[index].InUse() && m_pMigratingPlayerInfo[index].m_channelID == channelID)
		{
			migratingPlayerIndex = index;
			break;
		}
	}

	return migratingPlayerIndex;
}

//------------------------------------------------------------------------
void CGameRules::OnUserLeftLobby(int channelId)
{
}

//------------------------------------------------------------------------
void CGameRules::FinishMigrationForPlayer(int migratingIndex)
{
	// Remove the migrating player info (so we don't check again on a game restart!)
	m_pMigratingPlayerInfo[migratingIndex].Reset();
	// Check to see if this was the last player
	bool haveRemainingPlayers = false;

	for (uint32 i = 0; i < m_migratingPlayerMaxCount; ++i)
	{
		if (m_pMigratingPlayerInfo[i].InUse())
		{
			haveRemainingPlayers = true;
			break;
		}
	}

	if (!haveRemainingPlayers)
	{
		CryLog("CGameRules::FinishMigrationForPlayer, all players have now migrated, resuming game");
		g_pGame->SetHostMigrationState(CGame::eHMS_Resuming);
	}
}

//------------------------------------------------------------------------
void CGameRules::FakeDisconnectPlayer(EntityId playerId)
{
	// Pretend the player has disconnected
	ClientDisconnect_NotifyListeners(playerId);
	const int numListeners = m_clientConnectionListeners.size();

	for (int i = 0; i < numListeners; ++ i)
	{
		m_clientConnectionListeners[i]->OnClientDisconnect(-1, playerId);
	}

	// Remove the actor
	gEnv->pEntitySystem->RemoveEntity(playerId);
}

//------------------------------------------------------------------------
void CGameRules::ClientEnteredGame_NotifyListeners( EntityId clientId )
{
	if(m_rulesListeners.empty() == false)
	{
		TGameRulesListenerVec::iterator iter = m_rulesListeners.begin();

		while(iter != m_rulesListeners.end())
		{
			(*iter)->ClientEnteredGame( clientId );
			++iter;
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::ClientDisconnect_NotifyListeners( EntityId clientId )
{
	if(m_rulesListeners.empty() == false)
	{
		TGameRulesListenerVec::iterator iter = m_rulesListeners.begin();

		while(iter != m_rulesListeners.end())
		{
			(*iter)->ClientDisconnect( clientId );
			++iter;
		}
	}
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClHostMigrationFinished)
{
	g_pGame->SetHostMigrationState(CGame::eHMS_Resuming);
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClMidMigrationJoin)
{
	CryLog("CGameRules::ClMidMigrationJoin() state=%i, timeSinceChange=%f", params.m_state, params.m_timeSinceStateChanged);
	CGame::EHostMigrationState newState = CGame::EHostMigrationState(params.m_state);
	float timeOfChange = gEnv->pTimer->GetAsyncCurTime() - params.m_timeSinceStateChanged;
	g_pGame->SetHostMigrationStateAndTime(newState, timeOfChange);
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, ClHostMigrationPlayerJoined)
{
	const EntityId playerId = params.entityId;
#if !defined(_RELEASE)
	IEntity *pPlayer = gEnv->pEntitySystem->GetEntity(playerId);
	CryLog("CGameRules::ClHostMigrationPlayerJoined() '%s'", pPlayer ? pPlayer->GetName() : "<NULL>");
#endif
	// todo: ui
	return true;
}

//------------------------------------------------------------------------
bool CGameRules::OnInitiate(SHostMigrationInfo &hostMigrationInfo, uint32 &state)
{
	if (!g_pGame->GetIGameFramework()->ShouldMigrateNub(hostMigrationInfo.m_session))
	{
		return true;
	}

	CryLog("[Host Migration]: CGameRules::OnInitiate() Saving character for host migration started");
	m_hostMigrationClientHasRejoined = false;
	IEntityScriptProxy *pScriptProxy = static_cast<IEntityScriptProxy *>(GetEntity()->GetProxy(ENTITY_PROXY_SCRIPT));

	if (pScriptProxy)
	{
		if (string(pScriptProxy->GetState())  == "InGame")
		{
			m_hostMigrationTimeSinceGameStarted = (m_cachedServerTime - m_gameStartedTime);
		}
	}

	HostMigrationStopAddingPlayers();

	if (gEnv->IsClient())
	{
		if (!m_pHostMigrationParams)
		{
			m_pHostMigrationParams = new SHostMigrationClientRequestParams();
			m_pHostMigrationClientParams = new SHostMigrationClientControlledParams();
		}

		CPlayer *pPlayer = static_cast<CPlayer *>(g_pGame->GetIGameFramework()->GetClientActor());

		if (pPlayer)
		{
			m_pHostMigrationClientParams->m_viewQuat = pPlayer->GetViewRotation();
			m_pHostMigrationClientParams->m_position = pPlayer->GetEntity()->GetPos();
			pe_status_living livStat;
			IPhysicalEntity *pPhysicalEntity = pPlayer->GetEntity()->GetPhysics();

			if (pPhysicalEntity != NULL && (pPhysicalEntity->GetType() == PE_LIVING) && (pPhysicalEntity->GetStatus(&livStat) > 0))
			{
				m_pHostMigrationClientParams->m_velocity = livStat.velUnconstrained;
				m_pHostMigrationClientParams->m_hasValidVelocity = true;
				CryLog("    velocity={%f,%f,%f}", m_pHostMigrationClientParams->m_velocity.x, m_pHostMigrationClientParams->m_velocity.y, m_pHostMigrationClientParams->m_velocity.z);
			}

			IMovementController *pMovementController = pPlayer->GetMovementController();
			SMovementState movementState;
			pMovementController->GetMovementState(movementState);
			m_pHostMigrationClientParams->m_aimDirection = movementState.aimDirection;
		}
		else
		{
			CRY_ASSERT_MESSAGE(false, "Failed to find client actor when initiating a host migration");
			gEnv->pNetwork->TerminateHostMigration(hostMigrationInfo.m_session);
			return false;
		}
	}

	g_pGame->SetHostMigrationState(CGame::eHMS_WaitingForPlayers);
	CCCPOINT(HostMigration_OnInitiate);
	return true;
}

//------------------------------------------------------------------------
bool CGameRules::OnDemoteToClient(SHostMigrationInfo &hostMigrationInfo, uint32 &state)
{
	if (!g_pGame->GetIGameFramework()->ShouldMigrateNub(hostMigrationInfo.m_session))
	{
		return true;
	}

	CryLogAlways("[Host Migration]: CGameRules::OnDemoteToClient() started");

	if (m_hostMigrationCachedEntities.empty())
	{
		HostMigrationFindDynamicEntities(m_hostMigrationCachedEntities);
	}
	else
	{
		HostMigrationRemoveDuplicateDynamicEntities();
	}

	CryLogAlways("[Host Migration]: CGameRules::OnDemoteToClient() finished");
	CCCPOINT(HostMigration_OnDemoteToClient);
	return true;
}

//------------------------------------------------------------------------
void CGameRules::HostMigrationFindDynamicEntities(TEntityIdVec &results)
{
	IEntityItPtr pEntityIt = gEnv->pEntitySystem->GetEntityIterator();

	while (IEntity *pEntity = pEntityIt->Next())
	{
		if (pEntity->GetFlags() & ENTITY_FLAG_NEVER_NETWORK_STATIC)
		{
			results.push_back(pEntity->GetId());
			CryLog("    found dynamic entity %i '%s'", pEntity->GetId(), pEntity->GetName());
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::HostMigrationRemoveDuplicateDynamicEntities()
{
	CRY_ASSERT(!m_hostMigrationCachedEntities.empty());
	TEntityIdVec dynamicEntities;
	HostMigrationFindDynamicEntities(dynamicEntities);
	CryLog("CGameRules::HostMigrationRemoveDuplicateDynamicEntities(), found %i entities, already know about %i", (uint32)dynamicEntities.size(), (uint32)m_hostMigrationCachedEntities.size());
	// Any entities in the dynamicEntities vector that aren't in the m_hostMigrationCachedEntities vector have been added during a previous migration attempt, need to remove them now
	// Note: entities in the m_hostMigrationCachedEntities vector are removed in OnFinalise
	const int numEntities = dynamicEntities.size();

	for (int i = 0; i < numEntities; ++ i)
	{
		EntityId entityId = dynamicEntities[i];

		if (!stl::find(m_hostMigrationCachedEntities, entityId))
		{
			gEnv->pEntitySystem->RemoveEntity(entityId, true);
		}
	}
}

//------------------------------------------------------------------------
bool CGameRules::OnPromoteToServer(SHostMigrationInfo &hostMigrationInfo, uint32 &state)
{
	if (!g_pGame->GetIGameFramework()->ShouldMigrateNub(hostMigrationInfo.m_session))
	{
		return true;
	}

	CryLogAlways("[Host Migration]: CGameRules::OnPromoteToServer() started");
	// Server time will change after we migrate (change from old server time to new server time)
	m_gameStartedTime.SetValue(m_gameStartedTime.GetValue() - m_cachedServerTime.GetValue());
	m_gameStartTime.SetValue(m_gameStartTime.GetValue() - m_cachedServerTime.GetValue());

	// If this migration has reset (we're not the original anticipated host, remove any entities from the first attempt
	if (!m_hostMigrationCachedEntities.empty())
	{
		HostMigrationRemoveDuplicateDynamicEntities();
	}

	for (uint32 i = 0; i < MAX_PLAYERS; ++ i)
	{
		m_migratedPlayerChannels[i] = 0;
	}

	IEntityItPtr it = gEnv->pEntitySystem->GetEntityIterator();
	it->MoveFirst();

	for (uint32 i = 0; i < m_hostMigrationItemMaxCount; ++ i)
	{
		m_pHostMigrationItemInfo[i].Reset();
	}

	uint32 itemIndex = 0;
	IEntity *pEntity = NULL;
	// Tell entities that we're host migrating
	// - Currently only used by ForbiddenArea but may well be needed for other entities later
	// - Currently only called on the new server, add to OnDemoteToClient if we need to use this on a client
	IScriptTable *pScript = pEntity->GetScriptTable();

	if (pScript != NULL && pScript->GetValueType("OnHostMigration") == svtFunction)
	{
		m_pScriptSystem->BeginCall(pScript, "OnHostMigration");
		m_pScriptSystem->PushFuncParam(pScript);
		m_pScriptSystem->PushFuncParam(true);
		m_pScriptSystem->EndCall();
	}

	// This needs initialising on the new server otherwise the respawn timer will be counting down
	// from uninitialised data.  Likewise for the pre-round timer.
	ResetReviveCycleTime();
	// the server does not listen for entity_event_done, clients do however, when we migrate
	// the new server needs to remove any of these events he may be listening for
	TEntityTeamIdMap::iterator entityTeamsIt = m_entityteams.begin();

	for (; entityTeamsIt != m_entityteams.end(); ++ entityTeamsIt)
	{
		EntityId entityId = entityTeamsIt->first;
		RemoveEntityEventDoneListener(entityId);
#if !defined(_RELEASE)
		IEntity *pTeamEntity = gEnv->pEntitySystem->GetEntity(entityId);
		CryLog("[GameRules] OnPromoteToServer RemoveEntityEventLister(%d(%s), ENTITY_EVENT_DONE, %p)", entityId,
			   pTeamEntity ? pTeamEntity->GetName() : "null",
			   this);
#endif
	}

	ClearRemoveEntityEventListeners();
	const int numRespawnParams = m_respawndata.size();

	for (int i = 0; i < numRespawnParams; ++ i)
	{
		SEntityRespawnData *pData = &m_respawndata[i];
		pEntity = gEnv->pEntitySystem->GetEntity(pData->m_currentEntityId);

		if (pEntity == NULL)
		{
			CryLog("  detected respawn entity (id=%u) is not present, scheduling for respawn", pData->m_currentEntityId);
			ScheduleEntityRespawn(pData->m_currentEntityId, false, g_pGameCVars->g_defaultItemRespawnTimer);
		}
	}

	CryLog("[Host Migration]: CGameRules::OnPromoteToServer() finished");
	CCCPOINT(HostMigration_OnPromoteToServer);
	return true;
}

//------------------------------------------------------------------------
bool CGameRules::OnReconnectClient(SHostMigrationInfo &hostMigrationInfo, uint32 &state)
{
	if (!g_pGame->GetIGameFramework()->ShouldMigrateNub(hostMigrationInfo.m_session))
	{
		return true;
	}

	CryLogAlways("[Host Migration]: CGameRules::OnReconnectClient() started");

	if (hostMigrationInfo.IsNewHost())
	{
		// Can't use gamerules cached version of server time since this function will be called before the Update()
		m_gameStartedTime.SetValue(m_gameStartedTime.GetValue() + g_pGame->GetIGameFramework()->GetServerTime().GetValue());
		m_gameStartTime.SetValue(m_gameStartTime.GetValue() + g_pGame->GetIGameFramework()->GetServerTime().GetValue());
	}

	CryLogAlways("[Host Migration]: CGameRules::OnReconnectClient() finished");
	CCCPOINT(HostMigration_OnReconnectClient);
	return true;
}

static void FlushPhysicsQueues()
{
	// Flush the physics linetest and events queue
	if (gEnv->pPhysicalWorld)
	{
		gEnv->pPhysicalWorld->TracePendingRays(0);
		gEnv->pPhysicalWorld->ClearLoggedEvents();
	}

	if (gEnv->p3DEngine)
	{
		IDeferredPhysicsEventManager *pPhysEventManager = gEnv->p3DEngine->GetDeferredPhysicsEventManager();

		if (pPhysEventManager)
		{
			pPhysEventManager->ClearDeferredEvents();
		}
	}
}


//------------------------------------------------------------------------
bool CGameRules::OnFinalise(SHostMigrationInfo &hostMigrationInfo, uint32 &state)
{
	if (!g_pGame->GetIGameFramework()->ShouldMigrateNub(hostMigrationInfo.m_session))
	{
		return true;
	}

	CryLogAlways("[Host Migration]: CGameRules::OnFinalise() started");
	//if (m_hostMigrationClientHasRejoined)
	{
		CCCPOINT(HostMigration_OnFinalise);

		if (!hostMigrationInfo.IsNewHost())
		{
			FlushPhysicsQueues();
			int numEntities = m_hostMigrationCachedEntities.size();
			CryLog("    removing %i entities", numEntities);

			for (int i = 0; i < numEntities; ++ i)
			{
				EntityId entId = m_hostMigrationCachedEntities[i];
				gEnv->pEntitySystem->RemoveEntity(entId, true);
				TEntityTeamIdMap::iterator entityTeamsIt = m_entityteams.begin();

				for (; entityTeamsIt != m_entityteams.end(); ++ entityTeamsIt)
				{
					EntityId entityId = entityTeamsIt->first;

					if (entityId == entId)
					{
						RemoveEntityEventDoneListener(entityId);
						int teamId = entityTeamsIt->second;

						if (teamId)
						{
							// Remove this entity from the team lists
							TPlayers &playersVec = m_playerteams[teamId];
							stl::find_and_erase(playersVec, entId);
							m_entityteams.erase(entityTeamsIt);
						}

						break;
					}
				}
			}
		}

		m_hostMigrationCachedEntities.clear();
		HostMigrationResumeAddingPlayers();
		g_pGame->PlayerIdSet(g_pGame->GetIGameFramework()->GetClientActorId());
		CryLogAlways("[Host Migration]: CGameRules::OnFinalise() finished - success");
		return true;
	}
	CryLogAlways("[Host Migration]: CGameRules::OnFinalise() finished - failure");
	return false;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CGameRules, SvHostMigrationRequestSetup)
{
	uint16 channelId = m_pGameFramework->GetGameChannelId(pNetChannel);
	IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActorByChannelId(channelId);
	CRY_ASSERT(pActor);
	CryLog("SvHostMigrationRequestSetup, unpacking setup for player '%s', channel=%i", pActor->GetEntity()->GetName(), channelId);
	// 	if (pActor->IsDead() && (params.m_timeToAutoRevive > 0.f))
	// 	{
	// 		IGameRulesSpawningModule *pSpawningModule = GetSpawningModule();
	// 		pSpawningModule->HostMigrationInsertIntoReviveQueue(pActor->GetEntityId(), params.m_timeToAutoRevive * 1000.f);
	// 	}
	return true;
}
