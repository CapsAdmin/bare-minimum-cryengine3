#include "StdAfx.h"
#include "ScriptBind_GameRules.h"
#include "GameRules.h"
#include "Game.h"
#include "GameCVars.h"
#include "Actor.h"
#include "Player.h"

#include "IUIDraw.h"
#include "IMovieSystem.h"

#include "ServerSynchedStorage.h"

#include "GameActions.h"
#include "Voting.h"
#include "IWorldQuery.h"

#include <StlUtils.h>
#include <StringUtils.h>

#include <IBreakableManager.h>

int CGameRules::s_invulnID = 0;
int CGameRules::s_barbWireID = 0;

//------------------------------------------------------------------------
CGameRules::CGameRules()
	: m_pGameFramework(0),
	  m_pGameplayRecorder(0),
	  m_pSystem(0),
	  m_pActorSystem(0),
	  m_pEntitySystem(0),
	  m_pScriptSystem(0),
	  m_pMaterialManager(0),
	  m_onCollisionFunc(0),
	  m_pClientNetChannel(0),
	  m_teamIdGen(0),
	  m_hitMaterialIdGen(0),
	  m_hitTypeIdGen(0),
	  m_currentStateId(0),
	  m_endTime(0.0f),
	  m_roundEndTime(0.0f),
	  m_preRoundEndTime(0.0f),
	  m_gameStartTime(0.0f),
	  m_gameStartedTime(0.0f),
	  m_ignoreEntityNextCollision(0),
	  m_timeOfDayInitialized(false),
	  m_processingHit(0),
	  m_bBlockPlayerAddition(false),
	  m_pMigratingPlayerInfo(NULL),
	  m_pHostMigrationItemInfo(NULL),
	  m_migratingPlayerMaxCount(0),
	  m_pHostMigrationParams(NULL),
	  m_pHostMigrationClientParams(NULL),
	  m_hostMigrationClientHasRejoined(false),
	  m_explosionScreenFX(true)
{
	m_timeLimit = g_pGameCVars->g_timelimit;

	if (gEnv->bMultiplayer)
	{
		m_migratingPlayerMaxCount = MAX_PLAYER_LIMIT;

		if (gEnv->pConsole)
		{
			ICVar *pMaxPlayers = gEnv->pConsole->GetCVar("sv_maxplayers");

			if (pMaxPlayers)
			{
				m_migratingPlayerMaxCount = pMaxPlayers->GetIVal();
			}
		}

		m_pMigratingPlayerInfo = new SMigratingPlayerInfo[m_migratingPlayerMaxCount];
		CRY_ASSERT(m_pMigratingPlayerInfo != NULL);
		m_hostMigrationItemMaxCount = m_migratingPlayerMaxCount * 8;		// Allow for 8 items per person
		m_pHostMigrationItemInfo = new SHostMigrationItemInfo[m_hostMigrationItemMaxCount];
		CRY_ASSERT(m_pHostMigrationItemInfo != NULL);
		m_hostMigrationCachedEntities.reserve(128);
	}
}

//------------------------------------------------------------------------
CGameRules::~CGameRules()
{
	SAFE_DELETE_ARRAY(m_pMigratingPlayerInfo)
	SAFE_DELETE_ARRAY(m_pHostMigrationItemInfo);
	SAFE_DELETE(m_pHostMigrationParams);
	SAFE_DELETE(m_pHostMigrationClientParams);

	if (m_onCollisionFunc)
	{
		gEnv->pScriptSystem->ReleaseFunc(m_onCollisionFunc);
		m_onCollisionFunc = 0;
	}

	if (m_pGameFramework)
	{
		if (m_pGameFramework->GetIGameRulesSystem())
		{
			m_pGameFramework->GetIGameRulesSystem()->SetCurrentGameRules(0);
		}

		if(m_pGameFramework->GetIViewSystem())
		{
			m_pGameFramework->GetIViewSystem()->RemoveListener(this);
		}
	}

	GetGameObject()->ReleaseActions(this);
	gEnv->pNetwork->RemoveHostMigrationEventListener(this);

	if (g_pGame->GetHostMigrationState() != CGame::eHMS_NotMigrating)
	{
		// Quitting game mid-migration (probably caused by a failed migration), re-enable timers so that the game isn't paused if we join a new one!
		g_pGame->AbortHostMigration();
	}
}

//------------------------------------------------------------------------
bool CGameRules::Init( IGameObject *pGameObject )
{
	SetGameObject(pGameObject);

	if (!GetGameObject()->BindToNetwork())
	{
		return false;
	}

	GetGameObject()->EnablePostUpdates(this);
	m_pGameFramework = g_pGame->GetIGameFramework();
	m_pGameplayRecorder = m_pGameFramework->GetIGameplayRecorder();
	m_pSystem = m_pGameFramework->GetISystem();
	m_pActorSystem = m_pGameFramework->GetIActorSystem();
	m_pEntitySystem = gEnv->pEntitySystem;
	m_pScriptSystem = m_pSystem->GetIScriptSystem();
	m_pMaterialManager = gEnv->p3DEngine->GetMaterialManager();
	s_invulnID = m_pMaterialManager->GetSurfaceTypeManager()->GetSurfaceTypeByName("mat_invulnerable")->GetId();
	s_barbWireID = m_pMaterialManager->GetSurfaceTypeManager()->GetSurfaceTypeByName("mat_metal_barbwire")->GetId();

	//Register as ViewSystem listener (for cut-scenes, ...)
	if(m_pGameFramework->GetIViewSystem())
	{
		m_pGameFramework->GetIViewSystem()->AddListener(this);
	}

	m_script = GetEntity()->GetScriptTable();

	if (!m_script)
	{
		// script table not found
	}
	else
	{
		m_script->GetValue("Client", m_clientScript);
		m_script->GetValue("Server", m_serverScript);
		m_script->GetValue("OnCollision", m_onCollisionFunc);
	}

	m_collisionTable = gEnv->pScriptSystem->CreateTable();
	m_clientStateScript = m_clientScript;
	m_serverStateScript = m_serverScript;
	m_scriptHitInfo.Create(gEnv->pScriptSystem);
	m_scriptExplosionInfo.Create(gEnv->pScriptSystem);
	SmartScriptTable affected(gEnv->pScriptSystem);
	m_scriptExplosionInfo->SetValue("AffectedEntities", affected);
	SmartScriptTable affectedObstruction(gEnv->pScriptSystem);
	m_scriptExplosionInfo->SetValue("AffectedEntitiesObstruction", affectedObstruction);
	m_pGameFramework->GetIGameRulesSystem()->SetCurrentGameRules(this);
	g_pGame->GetGameRulesScriptBind()->AttachTo(this);

	// setup animation time scaling (until we have assets that cover the speeds we need timescaling).
	if (gEnv->pCharacterManager)
	{
		gEnv->pCharacterManager->SetScalingLimits( Vec2(0.5f, 3.0f) );
	}

	bool isMultiplayer = gEnv->bMultiplayer;

	if(gEnv->IsClient())
	{
		IActionMapManager *pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();
		IActionMap *am = NULL;
		pActionMapMan->EnableActionMap("multiplayer", isMultiplayer);
		pActionMapMan->EnableActionMap("singleplayer", !isMultiplayer);

		if(isMultiplayer)
		{
			am = pActionMapMan->GetActionMap("multiplayer");
		}
		else
		{
			am = pActionMapMan->GetActionMap("singleplayer");
		}

		bool b = GetGameObject()->CaptureActions(this);
		assert(b);

		if(am)
		{
			am->SetActionListener(GetEntity()->GetId());
		}
	}

	gEnv->pNetwork->AddHostMigrationEventListener(this, "CGameRules");

	if (g_pGame->GetHostMigrationState() != CGame::eHMS_NotMigrating)
	{
		// Quitting game mid-migration (probably caused by a failed migration), re-enable timers so that the game isn't paused if we join a new one!
		g_pGame->AbortHostMigration();
	}

	return true;
}

//------------------------------------------------------------------------
void CGameRules::PostInit( IGameObject *pGameObject )
{
	pGameObject->EnableUpdateSlot(this, 0);
	pGameObject->SetUpdateSlotEnableCondition(this, 0, eUEC_WithoutAI);
	pGameObject->EnablePostUpdates(this);
	IConsole *pConsole = gEnv->pConsole;
	RegisterConsoleCommands(pConsole);
	RegisterConsoleVars(pConsole);
}

//------------------------------------------------------------------------
void CGameRules::InitClient(int channelId)
{
}

//------------------------------------------------------------------------
void CGameRules::PostInitClient(int channelId)
{
	// update the time
	GetGameObject()->InvokeRMI(ClSetGameTime(), SetGameTimeParams(m_endTime), eRMI_ToClientChannel, channelId);
	GetGameObject()->InvokeRMI(ClSetRoundTime(), SetGameTimeParams(m_roundEndTime), eRMI_ToClientChannel, channelId);
	GetGameObject()->InvokeRMI(ClSetPreRoundTime(), SetGameTimeParams(m_preRoundEndTime), eRMI_ToClientChannel, channelId);
	GetGameObject()->InvokeRMI(ClSetReviveCycleTime(), SetGameTimeParams(m_reviveCycleEndTime), eRMI_ToClientChannel, channelId);

	if (m_gameStartTime.GetMilliSeconds() > m_pGameFramework->GetServerTime().GetMilliSeconds())
	{
		GetGameObject()->InvokeRMI(ClSetGameStartTimer(), SetGameTimeParams(m_gameStartTime), eRMI_ToClientChannel, channelId);
	}

	// update team status on the client
	for (TEntityTeamIdMap::const_iterator tit = m_entityteams.begin(); tit != m_entityteams.end(); ++tit)
	{
		GetGameObject()->InvokeRMIWithDependentObject(ClSetTeam(), SetTeamParams(tit->first, tit->second), eRMI_ToClientChannel, tit->first, channelId);
	}

	// init spawn groups
	for (TSpawnGroupMap::const_iterator sgit = m_spawnGroups.begin(); sgit != m_spawnGroups.end(); ++sgit)
	{
		GetGameObject()->InvokeRMIWithDependentObject(ClAddSpawnGroup(), SpawnGroupParams(sgit->first), eRMI_ToClientChannel, sgit->first, channelId);
	}

	// update minimap entities on the client
	for (TMinimap::const_iterator mit = m_minimap.begin(); mit != m_minimap.end(); ++mit)
	{
		GetGameObject()->InvokeRMIWithDependentObject(ClAddMinimapEntity(), AddMinimapEntityParams(mit->entityId, mit->lifetime, mit->type), eRMI_ToClientChannel, mit->entityId, channelId);
	}

	// freeze stuff on the clients
	for (TFrozenEntities::const_iterator fit = m_frozen.begin(); fit != m_frozen.end(); ++fit)
	{
		GetGameObject()->InvokeRMIWithDependentObject(ClFreezeEntity(), FreezeEntityParams(fit->first, true, false), eRMI_ToClientChannel, fit->first, channelId);
	}

	if (g_pGame->GetHostMigrationState() != CGame::eHMS_NotMigrating)
	{
		// Tell this client who else has made it
		for (int i = 0; i < MAX_PLAYERS; ++ i)
		{
			TNetChannelID migratedChannelId = m_migratedPlayerChannels[i];

			if (migratedChannelId)
			{
				IActor *pActor = GetActorByChannelId(migratedChannelId);

				if (pActor)
				{
					EntityParams params;
					params.entityId = pActor->GetEntityId();
					GetGameObject()->InvokeRMIWithDependentObject(ClHostMigrationPlayerJoined(), params, eRMI_ToClientChannel | eRMI_NoLocalCalls, params.entityId, channelId);
				}
			}
			else
			{
				break;
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::Release()
{
	UnregisterConsoleCommands(gEnv->pConsole);
	delete this;
}

//------------------------------------------------------------------------
void CGameRules::FullSerialize( TSerialize ser )
{
	if (ser.IsReading())
	{
		ResetFrozen();
	}

	TFrozenEntities frozen(m_frozen);
	ser.Value("FrozenEntities", frozen);

	if (ser.IsReading())
	{
		for (TFrozenEntities::const_iterator it = frozen.begin(), end = frozen.end(); it != end; ++it)
		{
			FreezeEntity(it->first, true, false, true);
		}
	}
}

//-----------------------------------------------------------------------------------------------------
void CGameRules::PostSerialize()
{
}

//------------------------------------------------------------------------
void CGameRules::Update( SEntityUpdateContext &ctx, int updateSlot )
{
	m_cachedServerTime = g_pGame->GetIGameFramework()->GetServerTime();

	if (m_hostMigrationTimeSinceGameStarted.GetValue())
	{
		int64 initialValue = m_gameStartedTime.GetValue();
		m_gameStartedTime = (m_cachedServerTime - m_hostMigrationTimeSinceGameStarted);
		m_hostMigrationTimeSinceGameStarted.SetValue(0);
	}

	if (updateSlot != 0)
	{
		return;
	}

	//g_pGame->GetServerSynchedStorage()->SetGlobalValue(15, 1026);
	bool server = gEnv->bServer;

	if (server)
	{
		ProcessQueuedExplosions();
		UpdateEntitySchedules(ctx.fFrameTime);

		if (gEnv->bMultiplayer)
		{
			TFrozenEntities::const_iterator next;

			for (TFrozenEntities::const_iterator fit = m_frozen.begin(); fit != m_frozen.end(); fit = next)
			{
				next = fit;
				++next;
			}
		}
	}

	UpdateMinimap(ctx.fFrameTime);

	if (gEnv->bServer)
	{
		GetGameObject()->ChangedNetworkState( eEA_GameServerDynamic );
	}
}

//------------------------------------------------------------------------
void CGameRules::HandleEvent( const SGameObjectEvent &event)
{
}

//------------------------------------------------------------------------
void CGameRules::ProcessEvent( SEntityEvent &event)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_GAME);
	static ICVar *pTOD = gEnv->pConsole->GetCVar("sv_timeofdayenable");

	switch(event.event)
	{
		case ENTITY_EVENT_RESET:
			m_timeOfDayInitialized = false;
			ResetFrozen();

			while (!m_queuedExplosions.empty())
			{
				m_queuedExplosions.pop();
			}

			while (!m_queuedHits.empty())
			{
				m_queuedHits.pop();
			}

			m_processingHit = 0;
			// TODO: move this from here
			m_respawns.clear();
			m_removals.clear();
			break;

		case ENTITY_EVENT_START_GAME:
			m_timeOfDayInitialized = false;

			if (gEnv->bServer && gEnv->bMultiplayer && pTOD && pTOD->GetIVal() && g_pGame->GetIGameFramework()->IsImmersiveMPEnabled())
			{
				static ICVar *pStart = gEnv->pConsole->GetCVar("sv_timeofdaystart");

				if (pStart)
				{
					gEnv->p3DEngine->GetTimeOfDay()->SetTime(pStart->GetFVal(), true);
				}
			}

			break;

		case ENTITY_EVENT_ENTER_SCRIPT_STATE:
			m_currentStateId = (int)event.nParam[0];
			m_clientStateScript = 0;
			m_serverStateScript = 0;
			IEntityScriptProxy *pScriptProxy = static_cast<IEntityScriptProxy *>(GetEntity()->GetProxy(ENTITY_PROXY_SCRIPT));

			if (pScriptProxy)
			{
				const char *stateName = pScriptProxy->GetState();
				m_clientScript->GetValue(stateName, m_clientStateScript);
				m_serverScript->GetValue(stateName, m_serverStateScript);
			}

			break;
	}
}

//------------------------------------------------------------------------
void CGameRules::SetAuthority( bool auth )
{
}

//------------------------------------------------------------------------
void CGameRules::PostUpdate( float frameTime )
{
}

//------------------------------------------------------------------------
CActor *CGameRules::GetActorByChannelId(int channelId) const
{
	if (m_hostMigrationCachedEntities.empty())
	{
		return static_cast<CActor *>(m_pGameFramework->GetIActorSystem()->GetActorByChannelId(channelId));
	}
	else
	{
		CRY_ASSERT(g_pGame->GetHostMigrationState() != CGame::eHMS_NotMigrating);
		CActor *pCachedActor = NULL;
		CActor *pCurrentActor = NULL;
		IActorSystem *pActorSystem = g_pGame->GetIGameFramework()->GetIActorSystem();
		IActorIteratorPtr it = pActorSystem->CreateActorIterator();

		while (IActor *pActor = it->Next())
		{
			if (pActor->GetChannelId() == channelId)
			{
				const bool bInRemoveList = stl::find(m_hostMigrationCachedEntities, pActor->GetEntityId());

				if (bInRemoveList)
				{
					CRY_ASSERT(!pCachedActor);
					pCachedActor = static_cast<CActor *>(pActor);
				}
				else
				{
					CRY_ASSERT(!pCurrentActor);
					pCurrentActor = static_cast<CActor *>(pActor);
				}
			}
		}

		if (gEnv->bServer)
		{
			// Server: if we've got a cached one then we are a secondary server, this can give us a few frames of
			// having a duplicated actor (pCurrentActor), we need to use the one that will be kept (pCachedActor).
			if (pCachedActor)
			{
				return pCachedActor;
			}
			else
			{
				return pCurrentActor;
			}
		}
		else
		{
			// Client: Use actor given to us by the server, if we haven't been given one then use the cached actor.
			if (pCurrentActor)
			{
				return pCurrentActor;
			}
			else
			{
				return pCachedActor;
			}
		}
	}
}

//------------------------------------------------------------------------
CActor *CGameRules::GetActorByEntityId(EntityId entityId) const
{
	return static_cast<CActor *>(m_pGameFramework->GetIActorSystem()->GetActor(entityId));
}

//------------------------------------------------------------------------
int CGameRules::GetChannelId(EntityId entityId) const
{
	CActor *pActor = static_cast<CActor *>(m_pGameFramework->GetIActorSystem()->GetActor(entityId));

	if (pActor)
	{
		return pActor->GetChannelId();
	}

	return 0;
}

//------------------------------------------------------------------------
bool CGameRules::IsDead(EntityId id) const
{
	if (CActor *pActor = GetActorByEntityId(id))
	{
		return (pActor->GetHealth() <= 0);
	}

	return false;
}

//------------------------------------------------------------------------
bool CGameRules::IsSpectator(EntityId id) const
{
	if (CActor *pActor = GetActorByEntityId(id))
	{
		return (pActor->GetSpectatorMode() != 0);
	}

	return false;
}


//------------------------------------------------------------------------
bool CGameRules::ShouldKeepClient(int channelId, EDisconnectionCause cause, const char *desc) const
{
	return (!strcmp("timeout", desc) || cause == eDC_Timeout);
}

//------------------------------------------------------------------------
void CGameRules::PrecacheLevel()
{
	CallScript(m_script, "PrecacheLevel");
}

//------------------------------------------------------------------------
void CGameRules::OnConnect(struct INetChannel *pNetChannel)
{
	m_pClientNetChannel = pNetChannel;
	CallScript(m_clientStateScript, "OnConnect");
}


//------------------------------------------------------------------------
void CGameRules::OnDisconnect(EDisconnectionCause cause, const char *desc)
{
	m_pClientNetChannel = 0;
	int icause = (int)cause;
	CallScript(m_clientStateScript, "OnDisconnect", icause, desc);

	// BecomeRemotePlayer() will put the player camera into 3rd person view, but
	// the player rig will still be first person (headless, not z sorted) so
	// don't do it during host migration events
	if (!g_pGame->IsGameSessionHostMigrating())
	{
		CActor *pLocalActor = static_cast<CActor *>(g_pGame->GetIGameFramework()->GetClientActor());

		if (pLocalActor)
		{
			pLocalActor->BecomeRemotePlayer();
		}
	}
}

//------------------------------------------------------------------------
bool CGameRules::OnClientConnect(int channelId, bool isReset)
{
	if (m_bBlockPlayerAddition)
	{
		return false;
	}

	if (!isReset)
	{
		m_channelIds.push_back(channelId);
		g_pGame->GetServerSynchedStorage()->OnClientConnect(channelId);
	}

	bool useExistingActor = false;
	IActor *pActor = NULL;
	// Check if there's a migrating player for this channel
	int migratingIndex = GetMigratingPlayerIndex(channelId);

	if (migratingIndex >= 0)
	{
		useExistingActor = true;
		pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_pMigratingPlayerInfo[migratingIndex].m_originalEntityId);
		pActor->SetMigrating(true);
		CryLog("CGameRules::OnClientConnect() migrating player, channelId=%i, name=%s", channelId, pActor->GetEntity()->GetName());
	}

	if (!useExistingActor)
	{
		string playerName;

		if (gEnv->bServer && gEnv->bMultiplayer)
		{
			if (INetChannel *pNetChannel = m_pGameFramework->GetNetChannel(channelId))
			{
				playerName = pNetChannel->GetNickname();

				if (!playerName.empty())
				{
					playerName = VerifyName(playerName);
				}
			}

			if(!playerName.empty())
			{
				CallScript(m_serverStateScript, "OnClientConnect", channelId, isReset, playerName.c_str());
			}
			else
			{
				CallScript(m_serverStateScript, "OnClientConnect", channelId, isReset);
			}
		}
		else
		{
			CallScript(m_serverStateScript, "OnClientConnect", channelId);
		}

		pActor = GetActorByChannelId(channelId);
	}

	if (pActor)
	{
		// Hide spawned actors until the client *enters* the game
		pActor->GetEntity()->Hide(true);
		//we need to pass team somehow so it will be reported correctly
		int status[2];
		status[0] = GetTeam(pActor->GetEntityId());
		status[1] = pActor->GetSpectatorMode();
		m_pGameplayRecorder->Event(pActor->GetEntity(), GameplayEvent(eGE_Connected, 0, m_pGameFramework->IsChannelOnHold(channelId) ? 1.0f : 0.0f, (void *)status));

		if (isReset)
		{
			SetTeam(GetChannelTeam(channelId), pActor->GetEntityId());
		}

		//notify client he has entered the game
		GetGameObject()->InvokeRMIWithDependentObject(ClEnteredGame(), NoParams(), eRMI_ToClientChannel, pActor->GetEntityId(), channelId);
	}

	if (migratingIndex != -1)
	{
		FinishMigrationForPlayer(migratingIndex);
	}
	else if (g_pGame->GetHostMigrationState() != CGame::eHMS_NotMigrating)
	{
		// This is a new client joining while we're migrating, need to tell them to pause game etc
		const float timeSinceStateChange = g_pGame->GetTimeSinceHostMigrationStateChanged();
		SMidMigrationJoinParams params(int(g_pGame->GetHostMigrationState()), timeSinceStateChange);
		GetGameObject()->InvokeRMI(ClMidMigrationJoin(), params, eRMI_ToClientChannel, channelId);
	}

	return pActor != 0;
}

//------------------------------------------------------------------------
void CGameRules::OnClientDisconnect(int channelId, EDisconnectionCause cause, const char *desc, bool keepClient)
{
	CActor *pActor = GetActorByChannelId(channelId);
	//assert(pActor);

	if (!pActor || !keepClient)
		if (g_pGame->GetServerSynchedStorage())
		{
			g_pGame->GetServerSynchedStorage()->OnClientDisconnect(channelId, false);
		}

	if (!pActor)
	{
		return;
	}

	string playerName = GetPlayerName(channelId);
	RenameEntityParams params(pActor->GetEntityId(), playerName.c_str());
	GetGameObject()->InvokeRMI(ClPlayerLeft(), params, eRMI_ToAllClients);

	if (pActor)
	{
		m_pGameplayRecorder->Event(pActor->GetEntity(), GameplayEvent(eGE_Disconnected, "", keepClient ? 1.0f : 0.0f));
	}

	if (keepClient)
	{
		if (g_pGame->GetServerSynchedStorage())
		{
			g_pGame->GetServerSynchedStorage()->OnClientDisconnect(channelId, true);
		}

		pActor->GetGameObject()->SetAspectProfile(eEA_Physics, eAP_NotPhysicalized);
		return;
	}

	if(pActor->GetActorClass() == CPlayer::GetActorClassType())
	{
		static_cast<CPlayer *>(pActor)->RemoveAllExplosives(0.0f);
	}

	SetTeam(0, pActor->GetEntityId());
	std::vector<int>::iterator channelit = std::find(m_channelIds.begin(), m_channelIds.end(), channelId);

	if (channelit != m_channelIds.end())
	{
		m_channelIds.erase(channelit);
	}

	CallScript(m_serverStateScript, "OnClientDisconnect", channelId);
	return;
}

//------------------------------------------------------------------------
bool CGameRules::OnClientEnteredGame(int channelId, bool isReset)
{
	CActor *pActor = GetActorByChannelId(channelId);

	if (!pActor)
	{
		return false;
	}

	// Ensure the actor is visible when entering the game (but not in the editor)
	if (!gEnv->IsEditing())
	{
		pActor->GetEntity()->Hide(false);
	}

	string playerName = GetPlayerName(channelId);
	RenameEntityParams params(pActor->GetEntityId(), playerName.c_str());
	GetGameObject()->InvokeRMI(ClPlayerJoined(), params, eRMI_ToAllClients);

	if (g_pGame->GetServerSynchedStorage())
	{
		g_pGame->GetServerSynchedStorage()->OnClientEnteredGame(channelId);
	}

	IScriptTable *pPlayer = pActor->GetEntity()->GetScriptTable();
	int loadingSaveGame = m_pGameFramework->IsLoadingSaveGame() ? 1 : 0;
	CallScript(m_serverStateScript, "OnClientEnteredGame", channelId, pPlayer, isReset, loadingSaveGame);

	// don't do this on reset - have already been added to correct team!
	if(!isReset || GetTeamCount() < 2)
	{
		ReconfigureVoiceGroups(pActor->GetEntityId(), -999, 0);    /* -999 should never exist :) */
	}

	// Need to update the time of day serialization chunk so that the new client can start at the right point
	// Note: Since we don't generally have a dynamic time of day, this will likely only effect clients
	// rejoining after a host migration since they won't be loading the value from the level
	CHANGED_NETWORK_STATE(this, eEA_GameServerDynamic);
	CHANGED_NETWORK_STATE(this, eEA_GameServerStatic);
	return true;
}

//------------------------------------------------------------------------
void CGameRules::OnEntitySpawn(IEntity *pEntity)
{
}

//------------------------------------------------------------------------
void CGameRules::OnEntityRemoved(IEntity *pEntity)
{
	if (gEnv->IsClient())
	{
		SetTeam(0, pEntity->GetId());
	}
}

//------------------------------------------------------------------------
void CGameRules::OnTextMessage(ETextMessageType type, const char *msg,
							   const char *p0, const char *p1, const char *p2, const char *p3)
{
	switch(type)
	{
		case eTextMessageConsole:
			CryLogAlways("%s", msg);
			break;

		case eTextMessageServer:
			{
				string completeMsg("** Server: ");
				completeMsg.append(msg);
				completeMsg.append(" **");
				CryLogAlways("[server] %s", msg);
			}
			break;

		case eTextMessageError:
			break;

		case eTextMessageInfo:
			break;

		case eTextMessageCenter:
			break;
	}
}

//------------------------------------------------------------------------
void CGameRules::OnChatMessage(EChatMessageType type, EntityId sourceId, EntityId targetId, const char *msg, bool teamChatOnly)
{
	//send chat message to hud
	int teamFaction = 0;

	if(IActor *pActor = gEnv->pGame->GetIGameFramework()->GetClientActor())
	{
		if(pActor->GetEntityId() != sourceId)
		{
			if(GetTeamCount() > 1)
			{
				if(GetTeam(pActor->GetEntityId()) == GetTeam(sourceId))
				{
					teamFaction = 1;
				}
				else
				{
					teamFaction = 2;
				}
			}
			else
			{
				teamFaction = 2;
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::OnRevive(CActor *pActor, const Vec3 &pos, const Quat &rot, int teamId)
{
	ScriptHandle handle(pActor->GetEntityId());
	Vec3 rotVec = Vec3(Ang3(rot));
	CallScript(m_clientScript, "OnRevive", handle, pos, rotVec, teamId);
}

//------------------------------------------------------------------------
void CGameRules::OnKill(CActor *pActor, EntityId shooterId, const char *weaponClassName, int damage, int material, int hit_type)
{
	ScriptHandle handleEntity(pActor->GetEntityId()), handleShooter(shooterId);
	CallScript(m_clientStateScript, "OnKill", handleEntity, handleShooter, weaponClassName, damage, material, hit_type);
}

//------------------------------------------------------------------------
void CGameRules::AddTaggedEntity(EntityId shooter, EntityId targetId, bool temporary, float time)
{
	if(!gEnv->bServer) // server sends to all clients
	{
		return;
	}

	if(GetTeamCount() > 1)
	{
		EntityId teamId = GetTeam(shooter);
		TPlayerTeamIdMap::const_iterator tit = m_playerteams.find(teamId);

		if (tit != m_playerteams.end())
		{
			for (TPlayers::const_iterator it = tit->second.begin(); it != tit->second.end(); ++it)
			{
				if(temporary)
				{
					TempRadarTaggingParams params(targetId, time);
					GetGameObject()->InvokeRMI(ClTempRadarEntity(), params, eRMI_ToClientChannel, GetChannelId(*it));
				}
				else
				{
					EntityParams params(targetId);
					GetGameObject()->InvokeRMI(ClTaggedEntity(), params, eRMI_ToClientChannel, GetChannelId(*it));
				}
			}
		}
	}
	else
	{
		if(temporary)
		{
			TempRadarTaggingParams params(targetId, time);
			GetGameObject()->InvokeRMI(ClTempRadarEntity(), params, eRMI_ToClientChannel, GetChannelId(shooter));
		}
		else
		{
			EntityParams params(targetId);
			GetGameObject()->InvokeRMI(ClTaggedEntity(), params, eRMI_ToClientChannel, GetChannelId(shooter));
		}
	}

	// add PP and CP for tagging this entity
	ScriptHandle shooterHandle(shooter);
	ScriptHandle targetHandle(targetId);
	CallScript(m_serverScript, "OnAddTaggedEntity", shooterHandle, targetHandle);
}

//------------------------------------------------------------------------
void CGameRules::OnKillMessage(EntityId targetId, EntityId shooterId, const char *weaponClassName, float damage, int material, int hit_type)
{
	if(EntityId client_id = g_pGame->GetIGameFramework()->GetClientActor() ? g_pGame->GetIGameFramework()->GetClientActor()->GetEntityId() : 0)
	{
		if(!gEnv->bServer && gEnv->IsClient() && client_id == shooterId && client_id != targetId)
		{
			m_pGameplayRecorder->Event(gEnv->pGame->GetIGameFramework()->GetClientActor()->GetEntity(), GameplayEvent(eGE_Kill, weaponClassName));
		}
	}
}

//------------------------------------------------------------------------
CActor *CGameRules::SpawnPlayer(int channelId, const char *name, const char *className, const Vec3 &pos, const Ang3 &angles)
{
	if (!gEnv->bServer)
	{
		return 0;
	}

	CActor *pActor = GetActorByChannelId(channelId);

	if (!pActor)
	{
		pActor = static_cast<CActor *>(m_pActorSystem->CreateActor(channelId, VerifyName(name).c_str(), className, pos, Quat(angles), Vec3(1, 1, 1)));
	}

	return pActor;
}

//------------------------------------------------------------------------
CActor *CGameRules::ChangePlayerClass(int channelId, const char *className)
{
	if (!gEnv->bServer)
	{
		return 0;
	}

	CActor *pOldActor = GetActorByChannelId(channelId);

	if (!pOldActor)
	{
		return 0;
	}

	if (!strcmp(pOldActor->GetEntity()->GetClass()->GetName(), className))
	{
		return pOldActor;
	}

	EntityId oldEntityId = pOldActor->GetEntityId();
	string oldName = pOldActor->GetEntity()->GetName();
	Ang3 oldAngles = pOldActor->GetAngles();
	Vec3 oldPos = pOldActor->GetEntity()->GetWorldPos();
	m_pEntitySystem->RemoveEntity(pOldActor->GetEntityId(), true);
	CActor *pActor = static_cast<CActor *>(m_pActorSystem->CreateActor(channelId, oldName.c_str(), className, oldPos, Quat::CreateRotationXYZ(oldAngles), Vec3(1, 1, 1), oldEntityId));

	if (pActor)
	{
		MovePlayer(pActor, oldPos, oldAngles);
	}

	return pActor;
}

//------------------------------------------------------------------------
void CGameRules::RevivePlayer(CActor *pActor, const Vec3 &pos, const Ang3 &angles, int teamId, bool clearInventory)
{

	if (IsFrozen(pActor->GetEntityId()))
	{
		FreezeEntity(pActor->GetEntityId(), false, false);
	}

	float health = 100;

	if(!gEnv->bMultiplayer && pActor->IsClient())
	{
		health = g_pGameCVars->g_playerHealthValue;
	}

	pActor->SetMaxHealth(health);

	if (!m_pGameFramework->IsChannelOnHold(pActor->GetChannelId()))
	{
		pActor->GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Alive);
	}

	Matrix34 tm(pActor->GetEntity()->GetWorldTM());
	tm.SetTranslation(pos);
	pActor->GetEntity()->SetWorldTM(tm);
	pActor->SetAngles(angles);

	pActor->NetReviveAt(pos, Quat(angles), teamId);
	// PLAYERPREDICTION
	pActor->GetGameObject()->InvokeRMI(CActor::ClRevive(), CActor::ReviveParams(pos, angles, teamId, pActor->GetNetPhysCounter()),
									   eRMI_ToAllClients | eRMI_NoLocalCalls);
	// ~PLAYERPREDICTION

	// re-enable player
	if ( pActor->GetEntity()->GetAI() && !pActor->GetEntity()->GetAI()->IsEnabled() )
	{
		pActor->GetEntity()->GetAI()->Event(AIEVENT_ENABLE, NULL);
	}

	m_pGameplayRecorder->Event(pActor->GetEntity(), GameplayEvent(eGE_Revive));
}

//------------------------------------------------------------------------
void CGameRules::RenamePlayer(CActor *pActor, const char *name)
{
	string fixed = VerifyName(name, pActor->GetEntity());
	RenameEntityParams params(pActor->GetEntityId(), fixed.c_str());

	if (!stricmp(fixed.c_str(), pActor->GetEntity()->GetName()))
	{
		return;
	}

	if (gEnv->bServer)
	{
		if (!gEnv->IsClient())
		{
			pActor->GetEntity()->SetName(fixed.c_str());
		}

		GetGameObject()->InvokeRMIWithDependentObject(ClRenameEntity(), params, eRMI_ToAllClients, params.entityId);

		if (INetChannel *pNetChannel = pActor->GetGameObject()->GetNetChannel())
		{
			pNetChannel->SetNickname(fixed.c_str());
		}

		m_pGameplayRecorder->Event(pActor->GetEntity(), GameplayEvent(eGE_Renamed, fixed));
	}
	else if (pActor->GetEntityId() == m_pGameFramework->GetClientActor()->GetEntityId())
	{
		GetGameObject()->InvokeRMIWithDependentObject(SvRequestRename(), params, eRMI_ToServer, params.entityId);
	}
}

//------------------------------------------------------------------------
string CGameRules::VerifyName(const char *name, IEntity *pEntity)
{
	string nameFormatter(name);

	// size limit is 26
	if (nameFormatter.size() > 26)
	{
		nameFormatter.resize(26);
	}

	// no spaces at start/end
	nameFormatter.TrimLeft(' ');
	nameFormatter.TrimRight(' ');

	// no empty names
	if (nameFormatter.empty())
	{
		nameFormatter = "empty";
	}

	// no @ signs
	nameFormatter.replace("@", "_");

	// search for duplicates
	if (IsNameTaken(nameFormatter.c_str(), pEntity))
	{
		int n = 1;
		string appendix;

		do
		{
			appendix.Format("(%d)", n++);
		}
		while(IsNameTaken(nameFormatter + appendix));

		nameFormatter.append(appendix);
	}

	return nameFormatter;
}

//------------------------------------------------------------------------
bool CGameRules::IsNameTaken(const char *name, IEntity *pEntity)
{
	for (std::vector<int>::const_iterator it = m_channelIds.begin(); it != m_channelIds.end(); ++it)
	{
		CActor *pActor = GetActorByChannelId(*it);

		if (pActor && pActor->GetEntity() != pEntity && !stricmp(name, pActor->GetEntity()->GetName()))
		{
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------
void CGameRules::KillPlayer(IActor *pActor, const bool inDropItem, const bool inDoRagdoll, const HitInfo &inHitInfo)
{
	if(!gEnv->bServer)
	{
		return;
	}

	CActor *pCActor = static_cast<CActor *>(pActor);

	CActor::KillParams params;
	params.shooterId = inHitInfo.shooterId;
	params.targetId = inHitInfo.targetId;
	params.weaponId = inHitInfo.weaponId;
	params.damage = inHitInfo.damage;
	params.material = -1;
	params.hit_type = inHitInfo.type;
	params.hit_joint = inHitInfo.partId;
	params.projectileClassId = inHitInfo.projectileClassId;
	params.penetration = inHitInfo.penetrationCount;
	params.firstKill = false;
	params.killViaProxy = inHitInfo.hitViaProxy;
	params.forceLocalKill = inHitInfo.forceLocalKill;
	params.dir = inHitInfo.dir;
	params.impulseScale = inHitInfo.impulseScale;
	params.ragdoll = inDoRagdoll;
	params.penetration = inHitInfo.penetrationCount;

	if (params.ragdoll)
	{
		pActor->GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Ragdoll);
	}

	pCActor->NetKill(inHitInfo.shooterId, 0, (int)inHitInfo.damage, inHitInfo.material, inHitInfo.type);
	pActor->GetGameObject()->InvokeRMI(CActor::ClKill(), params, eRMI_ToAllClients | eRMI_NoLocalCalls);
	m_pGameplayRecorder->Event(pActor->GetEntity(), GameplayEvent(eGE_Death));

	if (inHitInfo.shooterId && inHitInfo.shooterId != pActor->GetEntityId())
	{
		if (IActor *pShooter = m_pGameFramework->GetIActorSystem()->GetActor(inHitInfo.shooterId))
		{
			m_pGameplayRecorder->Event(pShooter->GetEntity(), GameplayEvent(eGE_Kill, 0, 0, (void *)&inHitInfo.weaponId));
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::MovePlayer(CActor *pActor, const Vec3 &pos, const Ang3 &angles)
{
	CActor::MoveParams params(pos, Quat(angles));
	pActor->GetGameObject()->InvokeRMI(CActor::ClMoveTo(), params, eRMI_ToClientChannel | eRMI_NoLocalCalls, pActor->GetChannelId());
	pActor->GetEntity()->SetWorldTM(Matrix34::Create(Vec3(1, 1, 1), params.rot, params.pos));
}

//------------------------------------------------------------------------
void CGameRules::ChangeSpectatorMode(CActor *pActor, uint8 mode, EntityId targetId, bool resetAll)
{
	if (pActor->GetSpectatorMode() == mode && mode != CActor::eASM_Follow)
	{
		return;
	}

	SpectatorModeParams params(pActor->GetEntityId(), mode, targetId, resetAll);

	if (gEnv->bServer)
	{
		ScriptHandle handle(params.entityId);
		ScriptHandle target(targetId);
		CallScript(m_serverStateScript, "OnChangeSpectatorMode", handle, mode, target, resetAll);
		m_pGameplayRecorder->Event(pActor->GetEntity(), GameplayEvent(eGE_Spectator, 0, (float)mode));
	}
	else if (pActor->GetEntityId() == m_pGameFramework->GetClientActor()->GetEntityId())
	{
		GetGameObject()->InvokeRMIWithDependentObject(SvRequestSpectatorMode(), params, eRMI_ToServer, params.entityId);
	}
}

//------------------------------------------------------------------------
void CGameRules::RequestNextSpectatorTarget(CActor *pActor, int change)
{
	if(pActor->GetSpectatorMode() != CActor::eASM_Follow)
	{
		return;
	}

	if(gEnv->bServer && pActor)
	{
		ScriptHandle playerId(pActor->GetEntityId());
		CallScript(m_serverStateScript, "RequestSpectatorTarget", playerId, change);
	}
}

//------------------------------------------------------------------------
void CGameRules::ChangeTeam(CActor *pActor, int teamId)
{
	if (teamId == GetTeam(pActor->GetEntityId()))
	{
		return;
	}

	ChangeTeamParams params(pActor->GetEntityId(), teamId);

	if (gEnv->bServer)
	{
		ScriptHandle handle(params.entityId);
		CallScript(m_serverStateScript, "OnChangeTeam", handle, params.teamId);
	}
	else if (pActor->GetEntityId() == m_pGameFramework->GetClientActor()->GetEntityId())
	{
		GetGameObject()->InvokeRMIWithDependentObject(SvRequestChangeTeam(), params, eRMI_ToServer, params.entityId);
	}
}

//------------------------------------------------------------------------
void CGameRules::ChangeTeam(CActor *pActor, const char *teamName)
{
	if (!teamName)
	{
		return;
	}

	int teamId = GetTeamId(teamName);

	if (!teamId)
	{
		CryLogAlways("Invalid team: %s", teamName);
		return;
	}

	ChangeTeam(pActor, teamId);
}

//------------------------------------------------------------------------
int CGameRules::GetPlayerCount(bool inGame) const
{
	if (!inGame)
	{
		return (int)m_channelIds.size();
	}

	int count = 0;

	for (std::vector<int>::const_iterator it = m_channelIds.begin(); it != m_channelIds.end(); ++it)
	{
		if (IsChannelInGame(*it))
		{
			++count;
		}
	}

	return count;
}

//------------------------------------------------------------------------
int CGameRules::GetSpectatorCount(bool inGame) const
{
	int count = 0;

	for (std::vector<int>::const_iterator it = m_channelIds.begin(); it != m_channelIds.end(); ++it)
	{
		CActor *pActor = GetActorByChannelId(*it);

		if (pActor && pActor->GetSpectatorMode() != 0)
		{
			if (!inGame || IsChannelInGame(*it))
			{
				++count;
			}
		}
	}

	return count;
}

//------------------------------------------------------------------------
EntityId CGameRules::GetPlayer(int idx)
{
	if (idx < 0 || idx >= m_channelIds.size())
	{
		return 0;
	}

	CActor *pActor = GetActorByChannelId(m_channelIds[idx]);
	return pActor ? pActor->GetEntityId() : 0;
}

//------------------------------------------------------------------------
void CGameRules::GetPlayers(TPlayers &players)
{
	players.resize(0);
	players.reserve(m_channelIds.size());

	for (std::vector<int>::const_iterator it = m_channelIds.begin(); it != m_channelIds.end(); ++it)
	{
		CActor *pActor = GetActorByChannelId(*it);

		if (pActor)
		{
			players.push_back(pActor->GetEntityId());
		}
	}
}

//------------------------------------------------------------------------
bool CGameRules::IsPlayerInGame(EntityId playerId) const
{
	const bool isLocalPlayer = (playerId == m_pGameFramework->GetClientActorId());
	INetChannel *pNetChannel = NULL;

	if(isLocalPlayer)
	{
		pNetChannel = g_pGame->GetIGameFramework()->GetClientChannel();
	}
	else
	{
		pNetChannel = g_pGame->GetIGameFramework()->GetNetChannel(GetChannelId(playerId));
	}

	if (pNetChannel && pNetChannel->GetContextViewState() >= eCVS_InGame)
	{
		return true;
	}

	return false;
}

//------------------------------------------------------------------------
bool CGameRules::IsPlayerActivelyPlaying(EntityId playerId) const
{
	if(!gEnv->bMultiplayer)
	{
		return true;
	}

	// 'actively playing' means they have selected a team / joined the game.

	if(GetTeamCount() == 1)
	{
		CActor *pActor = reinterpret_cast<CActor *>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(playerId));

		if(!pActor)
		{
			return false;
		}

		// in IA, out of the game if spectating when alive
		return (pActor->GetHealth() >= 0 || pActor->GetSpectatorMode() == CActor::eASM_None);
	}
	else
	{
		// in PS, out of the game if not yet on a team
		return (GetTeam(playerId) != 0);
	}
}

//------------------------------------------------------------------------
bool CGameRules::IsChannelInGame(int channelId) const
{
	INetChannel *pNetChannel = g_pGame->GetIGameFramework()->GetNetChannel(channelId);

	if (pNetChannel && pNetChannel->GetContextViewState() >= eCVS_InGame)
	{
		return true;
	}

	return false;
}

//------------------------------------------------------------------------
void CGameRules::StartVoting(CActor *pActor, EVotingState t, EntityId id, const char *param)
{
}

//------------------------------------------------------------------------
void CGameRules::Vote(CActor *pActor, bool yes)
{
}

//------------------------------------------------------------------------
void CGameRules::EndVoting(bool success)
{
}

//------------------------------------------------------------------------
int CGameRules::CreateTeam(const char *name)
{
	TTeamIdMap::iterator it = m_teams.find(CONST_TEMP_STRING(name));

	if (it != m_teams.end())
	{
		return it->second;
	}

	m_teams.insert(TTeamIdMap::value_type(name, ++m_teamIdGen));
	m_playerteams.insert(TPlayerTeamIdMap::value_type(m_teamIdGen, TPlayers()));
	return m_teamIdGen;
}

//------------------------------------------------------------------------
void CGameRules::RemoveTeam(int teamId)
{
	TTeamIdMap::iterator it = m_teams.find(CONST_TEMP_STRING(GetTeamName(teamId)));

	if (it == m_teams.end())
	{
		return;
	}

	m_teams.erase(it);

	for (TEntityTeamIdMap::iterator eit = m_entityteams.begin(); eit != m_entityteams.end(); ++eit)
	{
		if (eit->second == teamId)
		{
			eit->second = 0;    // 0 is no team
		}
	}

	m_playerteams.erase(m_playerteams.find(teamId));
}

//------------------------------------------------------------------------
const char *CGameRules::GetTeamName(int teamId) const
{
	for (TTeamIdMap::const_iterator it = m_teams.begin(); it != m_teams.end(); ++it)
	{
		if (teamId == it->second)
		{
			return it->first;
		}
	}

	return 0;
}

//------------------------------------------------------------------------
int CGameRules::GetTeamId(const char *name) const
{
	TTeamIdMap::const_iterator it = m_teams.find(CONST_TEMP_STRING(name));

	if (it != m_teams.end())
	{
		return it->second;
	}

	return 0;
}

//------------------------------------------------------------------------
int CGameRules::GetTeamCount() const
{
	return (int)m_teams.size();
}

//------------------------------------------------------------------------
int CGameRules::GetTeamPlayerCount(int teamId, bool inGame) const
{
	if (!inGame)
	{
		TPlayerTeamIdMap::const_iterator it = m_playerteams.find(teamId);

		if (it != m_playerteams.end())
		{
			return (int)it->second.size();
		}

		return 0;
	}
	else
	{
		TPlayerTeamIdMap::const_iterator it = m_playerteams.find(teamId);

		if (it != m_playerteams.end())
		{
			int count = 0;
			const TPlayers &players = it->second;

			for (TPlayers::const_iterator pit = players.begin(); pit != players.end(); ++pit)
				if (IsPlayerInGame(*pit))
				{
					++count;
				}

			return count;
		}

		return 0;
	}
}

//------------------------------------------------------------------------
int CGameRules::GetTeamChannelCount(int teamId, bool inGame) const
{
	int count = 0;

	for (TChannelTeamIdMap::const_iterator it = m_channelteams.begin(); it != m_channelteams.end(); ++it)
	{
		if (teamId == it->second)
		{
			if (!inGame || IsChannelInGame(it->first))
			{
				++count;
			}
		}
	}

	return count;
}

//------------------------------------------------------------------------
EntityId CGameRules::GetTeamPlayer(int teamId, int idx)
{
	TPlayerTeamIdMap::const_iterator it = m_playerteams.find(teamId);

	if (it != m_playerteams.end())
	{
		if (idx >= 0 && idx < it->second.size())
		{
			return it->second[idx];
		}
	}

	return 0;
}

//------------------------------------------------------------------------
void CGameRules::GetTeamPlayers(int teamId, TPlayers &players)
{
	players.resize(0);
	TPlayerTeamIdMap::const_iterator it = m_playerteams.find(teamId);

	if (it != m_playerteams.end())
	{
		players = it->second;
	}
}

//------------------------------------------------------------------------
void CGameRules::SetTeam(int teamId, EntityId id)
{
	if (!gEnv->bServer )
	{
		assert(0);
		return;
	}

	int oldTeam = GetTeam(id);

	if (oldTeam == teamId)
	{
		return;
	}

	TEntityTeamIdMap::iterator it = m_entityteams.find(id);

	if (it != m_entityteams.end())
	{
		m_entityteams.erase(it);
	}

	IActor *pActor = m_pActorSystem->GetActor(id);
	bool isplayer = pActor != 0;

	if (isplayer && oldTeam)
	{
		TPlayerTeamIdMap::iterator pit = m_playerteams.find(oldTeam);
		assert(pit != m_playerteams.end());
		stl::find_and_erase(pit->second, id);
	}

	if (teamId)
	{
		m_entityteams.insert(TEntityTeamIdMap::value_type(id, teamId));

		if (isplayer)
		{
			TPlayerTeamIdMap::iterator pit = m_playerteams.find(teamId);
			assert(pit != m_playerteams.end());
			pit->second.push_back(id);
			UpdateObjectivesForPlayer(GetChannelId(id), teamId);
		}
	}

	if(isplayer)
	{
		ReconfigureVoiceGroups(id, oldTeam, teamId);
		int channelId = GetChannelId(id);
		TChannelTeamIdMap::iterator itChannels = m_channelteams.find(channelId);

		if (itChannels != m_channelteams.end())
		{
			if (!teamId)
			{
				m_channelteams.erase(itChannels);
			}
			else
			{
				itChannels->second = teamId;
			}
		}
		else if(teamId)
		{
			m_channelteams.insert(TChannelTeamIdMap::value_type(channelId, teamId));
		}
	}

	{
		ScriptHandle handle(id);
		CallScript(m_serverStateScript, "OnSetTeam", handle, teamId);
	}

	if (gEnv->IsClient())
	{
		ScriptHandle handle(id);
		CallScript(m_clientStateScript, "OnSetTeam", handle, teamId);
	}

	// if this is a spawn group, update it's validity
	if (m_spawnGroups.find(id) != m_spawnGroups.end())
	{
		CheckSpawnGroupValidity(id);
	}

	GetGameObject()->InvokeRMIWithDependentObject(ClSetTeam(), SetTeamParams(id, teamId), eRMI_ToRemoteClients, id);

	if (IEntity *pEntity = m_pEntitySystem->GetEntity(id))
	{
		m_pGameplayRecorder->Event(pEntity, GameplayEvent(eGE_ChangedTeam, 0, (float)teamId));
	}
}

//------------------------------------------------------------------------
int CGameRules::GetTeam(EntityId entityId) const
{
	TEntityTeamIdMap::const_iterator it = m_entityteams.find(entityId);

	if (it != m_entityteams.end())
	{
		return it->second;
	}

	return 0;
}

//------------------------------------------------------------------------
int CGameRules::GetChannelTeam(int channelId) const
{
	TChannelTeamIdMap::const_iterator it = m_channelteams.find(channelId);

	if (it != m_channelteams.end())
	{
		return it->second;
	}

	return 0;
}

//------------------------------------------------------------------------
void CGameRules::AddObjective(int teamId, const char *objective, EMissionObjectiveState status, EntityId entityId)
{
	TObjectiveMap *pObjectives = GetTeamObjectives(teamId);

	if (!pObjectives)
	{
		m_objectives.insert(TTeamObjectiveMap::value_type(teamId, TObjectiveMap()));
	}

	if (pObjectives = GetTeamObjectives(teamId))
	{
		if (pObjectives->find(CONST_TEMP_STRING(objective)) == pObjectives->end())
		{
			pObjectives->insert(TObjectiveMap::value_type(objective, TObjective(status, entityId)));
		}
		else
		{
			SetObjectiveStatus( teamId, objective, status );
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::SetObjectiveStatus(int teamId, const char *objective, EMissionObjectiveState status)
{
	if (TObjective *pObjective = GetObjective(teamId, objective))
	{
		pObjective->status = status;
	}

	if (gEnv->bServer)
	{
		GAMERULES_INVOKE_ON_TEAM(teamId, ClSetObjectiveStatus(), SetObjectiveStatusParams(objective, status))
	}
}

//------------------------------------------------------------------------
CGameRules::EMissionObjectiveState CGameRules::GetObjectiveStatus(int teamId, const char *objective)
{
	if (TObjective *pObjective = GetObjective(teamId, objective))
	{
		return pObjective->status;
	}

	return eMOS_Deactivated;
}

//------------------------------------------------------------------------
void CGameRules::SetObjectiveEntity(int teamId, const char *objective, EntityId entityId)
{
	if (TObjective *pObjective = GetObjective(teamId, objective))
	{
		pObjective->entityId = entityId;
	}

	if (gEnv->bServer)
	{
		GAMERULES_INVOKE_ON_TEAM(teamId, ClSetObjectiveEntity(), SetObjectiveEntityParams(objective, entityId))
	}
}

//------------------------------------------------------------------------
void CGameRules::RemoveObjective(int teamId, const char *objective)
{
	if (TObjectiveMap *pObjectives = GetTeamObjectives(teamId))
	{
		TObjectiveMap::iterator it = pObjectives->find(CONST_TEMP_STRING(objective));

		if (it != pObjectives->end())
		{
			pObjectives->erase(it);
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::ResetObjectives()
{
	m_objectives.clear();

	if (gEnv->bServer)
	{
		GetGameObject()->InvokeRMI(ClResetObjectives(), NoParams(), eRMI_ToAllClients);
	}
}

//------------------------------------------------------------------------
CGameRules::TObjectiveMap *CGameRules::GetTeamObjectives(int teamId)
{
	TTeamObjectiveMap::iterator it = m_objectives.find(teamId);

	if (it != m_objectives.end())
	{
		return &it->second;
	}

	return 0;
}

//------------------------------------------------------------------------
CGameRules::TObjective *CGameRules::GetObjective(int teamId, const char *objective)
{
	if (TObjectiveMap *pObjectives = GetTeamObjectives(teamId))
	{
		TObjectiveMap::iterator it = pObjectives->find(CONST_TEMP_STRING(objective));

		if (it != pObjectives->end())
		{
			return &it->second;
		}
	}

	return 0;
}

//------------------------------------------------------------------------
void CGameRules::UpdateObjectivesForPlayer(int channelId, int teamId)
{
	GetGameObject()->InvokeRMI(ClResetObjectives(), NoParams(), eRMI_ToClientChannel, channelId);
}

//------------------------------------------------------------------------
bool CGameRules::IsFrozen(EntityId entityId) const
{
	if (m_frozen.find(entityId) != m_frozen.end())
	{
		return true;
	}

	IEntity *pEntity = m_pEntitySystem->GetEntity(entityId);

	if (!pEntity)
	{
		return false;
	}

	if (IEntityRenderProxy *pRenderProxy = (IEntityRenderProxy *)pEntity->GetProxy(ENTITY_PROXY_RENDER))
	{
		return (pRenderProxy->GetMaterialLayersMask()&MTL_LAYER_FROZEN) != 0;
	}

	return false;
}

//------------------------------------------------------------------------
void CGameRules::ResetFrozen()
{
	TFrozenEntities::iterator it = m_frozen.begin();

	while(it != m_frozen.end())
	{
		EntityId id = it->first;
		m_frozen.erase(it);
		FreezeEntity(id, false, false);
		it = m_frozen.begin();
	}

	m_frozen.clear();
}

//------------------------------------------------------------------------
void CGameRules::FreezeEntity(EntityId entityId, bool freeze, bool vapor, bool force)
{
	if (IsFrozen(entityId) == freeze)
	{
		if (freeze) // need to refresh the timer
		{
			TFrozenEntities::iterator it = m_frozen.find(entityId);

			if (it != m_frozen.end())
			{
				it->second = gEnv->pTimer->GetFrameStartTime();
			}
		}

		return;
	}

	IEntity *pEntity = m_pEntitySystem->GetEntity(entityId);

	if (!pEntity)
	{
		return;
	}

	IGameObject *pGameObject = m_pGameFramework->GetGameObject(entityId);
	IScriptTable *pScriptTable = pEntity->GetScriptTable();
	IActor *pActor = m_pGameFramework->GetIActorSystem()->GetActor(entityId);

	if (freeze && gEnv->bServer)
	{
		// don't freeze if ai doesn't want to
		if (pActor && !pActor->IsPlayer() && gEnv->pAISystem)
		{
			IEntity *pObject = 0;

			if (gEnv->pAISystem->GetSmartObjectManager()->SmartObjectEvent("DontFreeze", pEntity, pObject, 0))
			{
				return;
			}

			//Check also for invulnerable flag (just in case)
			SmartScriptTable props;

			if(pScriptTable && pScriptTable->GetValue("Properties", props))
			{
				int invulnerable = 0;

				if(props->GetValue("bInvulnerable", invulnerable) && invulnerable != 0)
				{
					return;
				}
			}
		}

		// if entity implements "GetFrozenAmount", check it and freeze only when >= 1
		HSCRIPTFUNCTION pfnGetFrozenAmount = 0;

		if (!force && pScriptTable && pScriptTable->GetValue("GetFrozenAmount", pfnGetFrozenAmount))
		{
			float frost = 1.0f;
			Script::CallReturn(gEnv->pScriptSystem, pfnGetFrozenAmount, pScriptTable, frost);
			gEnv->pScriptSystem->ReleaseFunc(pfnGetFrozenAmount);

			if (g_pGameCVars->cl_debugFreezeShake)
			{
				CryLog("%s frost amount: %.2f", pEntity->GetName(), frost);
			}

			if (frost < 0.99f)
			{
				return;
			}
		}
	}

	// call script event
	if (pScriptTable && pScriptTable->GetValueType("OnPreFreeze") == svtFunction)
	{
		bool ret = false;
		HSCRIPTFUNCTION func = 0;
		pScriptTable->GetValue("OnPreFreeze", func);

		if (Script::CallReturn(pScriptTable->GetScriptSystem(), func, pScriptTable, freeze, vapor, ret) && !ret)
		{
			return;
		}
	}

	// send event to game object
	if (pGameObject)
	{
		SGameObjectEvent event(eCGE_PreFreeze, eGOEF_ToAll, IGameObjectSystem::InvalidExtensionID, (void *)freeze);
		pGameObject->SendEvent(event);
	}

	{
		// apply frozen material layer
		IEntityRenderProxy *pRenderProxy = (IEntityRenderProxy *)pEntity->GetProxy(ENTITY_PROXY_RENDER);

		if (pRenderProxy)
		{
			uint8 activeLayers = pRenderProxy->GetMaterialLayersMask();

			if (freeze)
			{
				activeLayers |= MTL_LAYER_FROZEN;
			}
			else
			{
				activeLayers &= ~MTL_LAYER_FROZEN;
			}

			gEnv->p3DEngine->DeleteEntityDecals(pRenderProxy->GetRenderNode()); // remove any decals on this entity
			pRenderProxy->SetMaterialLayersMask(activeLayers);
		}

		// set the ice physics material
		IPhysicalEntity *pPhysicalEntity = pEntity->GetPhysics();
		int matId = -1;

		if (ISurfaceType *pSurfaceType = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeByName("mat_ice"))
		{
			matId = pSurfaceType->GetId();
		}

		if (pPhysicalEntity && matId > -1)
		{
			pe_status_nparts status_nparts;
			int nparts = pPhysicalEntity->GetStatus(&status_nparts);

			if (nparts)
			{
				for (int i = 0; i < nparts; i++)
				{
					pe_params_part part, partset;
					part.ipart = partset.ipart = i;

					if (!pPhysicalEntity->GetParams(&part))
					{
						continue;
					}

					if (!part.pMatMapping || !part.nMats)
					{
						continue;
					}

					if (freeze)
					{
						if (part.pPhysGeom)
						{
							static int map[255];

							for (int m = 0; m < part.nMats; m++)
							{
								map[m] = matId;
							}

							partset.pMatMapping = map;
							partset.nMats = part.nMats;
						}
					}
					else if (part.pPhysGeom)
					{
						partset.pMatMapping = part.pPhysGeom->pMatMapping;
						partset.nMats = part.pPhysGeom->nMats;
					}

					pPhysicalEntity->SetParams(&partset);
				}
			}

			pe_action_awake awake;
			awake.bAwake = 1;

			if (pPhysicalEntity)
			{
				pPhysicalEntity->Action(&awake);
			}
		}

		// freeze children
		int n = pEntity->GetChildCount();

		for (int i = 0; i < n; i++)
		{
			IEntity *pChild = pEntity->GetChild(i);

			// don't freeze players attached to entities (vehicles)
			if (IActor *pPlayerActor = m_pGameFramework->GetIActorSystem()->GetActor(pChild->GetId()))
			{
				if (pPlayerActor && pPlayerActor->IsPlayer())
				{
					continue;
				}
			}

			FreezeEntity(pChild->GetId(), freeze, vapor);
		}
	}

	if (freeze)
	{
		std::pair<TFrozenEntities::iterator, bool> result = m_frozen.insert(TFrozenEntities::value_type(entityId, CTimeValue(0.0f)));
		result.first->second = gEnv->pTimer->GetFrameStartTime();
	}
	else
	{
		m_frozen.erase(entityId);
	}

	// send event to game object
	if (pGameObject)
	{
		SGameObjectEvent event(eCGE_PostFreeze, eGOEF_ToAll, IGameObjectSystem::InvalidExtensionID, (void *)freeze);
		pGameObject->SendEvent(event);
	}

	// call script event
	if (pScriptTable && pScriptTable->GetValueType("OnPostFreeze") == svtFunction)
	{
		Script::CallMethod(pScriptTable, "OnPostFreeze", freeze);
	}

	if (gEnv->IsClient())
	{
		// spawn the vapor
		if (freeze && vapor)
		{
			SpawnParams params;
			params.eAttachForm = GeomForm_Surface;
			params.eAttachType = GeomType_Physics;
			//params.bIgnoreLocation=true;
			gEnv->pEntitySystem->GetBreakableManager()->AttachSurfaceEffect(pEntity, 0, SURFACE_BREAKAGE_TYPE("freeze_vapor"), params);
		}
	}

	if (gEnv->bServer)
	{
		GetGameObject()->InvokeRMIWithDependentObject(ClFreezeEntity(), FreezeEntityParams(entityId, freeze, vapor), eRMI_ToRemoteClients, entityId);
	}
}

//------------------------------------------------------------------------
void CGameRules::ShatterEntity(EntityId entityId, const Vec3 &pos, const Vec3 &impulse)
{
	IEntity *pEntity = m_pEntitySystem->GetEntity(entityId);

	if (!pEntity)
	{
		return;
	}

	// FIXME: Marcio: fix order of Shatter/Freeze on client, otherwise this check fails
	//if (!IsFrozen(entityId))
	if (gEnv->bServer && !IsFrozen(entityId))
	{
		return;
	}

	pe_params_structural_joint psj;
	psj.idx = 0;

	if (pEntity->GetFlags() & ENTITY_FLAG_MODIFIED_BY_PHYSICS ||
			pEntity->GetPhysics() && pEntity->GetPhysics()->GetParams(&psj))
	{
		return;
	}

	IGameObject *pGameObject = m_pGameFramework->GetGameObject(entityId);
	IScriptTable *pScriptTable = pEntity->GetScriptTable();

	if (pScriptTable)
	{
		// Ask script if it allows shattering.
		HSCRIPTFUNCTION pfnCanShatter = 0;

		if (pScriptTable->GetValue("CanShatter", pfnCanShatter))
		{
			bool bCanShatter = true;
			Script::CallReturn(gEnv->pScriptSystem, pfnCanShatter, pScriptTable, bCanShatter);
			gEnv->pScriptSystem->ReleaseFunc(pfnCanShatter);

			if (!bCanShatter)
			{
				return;
			}
		}
	}

	// send event to game object
	if (pGameObject)
	{
		SGameObjectEvent event(eCGE_PreShatter, eGOEF_ToAll, IGameObjectSystem::InvalidExtensionID, 0);
		pGameObject->SendEvent(event);
	}
	else
	{
		// This is simple entity.
		// So check if entity can be shattered.
		if (!gEnv->pEntitySystem->GetBreakableManager()->CanShatterEntity(pEntity))
		{
			return;
		}
	}

	// call script event
	if (pScriptTable && pScriptTable->GetValueType("OnPreShatter") == svtFunction)
	{
		Script::CallMethod(pScriptTable, "OnPreShatter", pos, impulse);
	}

	int nFrozenSlot = -1;

	if (pScriptTable)
	{
		HSCRIPTFUNCTION pfnGetFrozenSlot = 0;

		if (pScriptTable->GetValue("GetFrozenSlot", pfnGetFrozenSlot))
		{
			Script::CallReturn(gEnv->pScriptSystem, pfnGetFrozenSlot, pScriptTable, nFrozenSlot);
			gEnv->pScriptSystem->ReleaseFunc(pfnGetFrozenSlot);
		}
	}

	/*
		SpawnParams spawnparams;
		spawnparams.eAttachForm=GeomForm_Surface;
		spawnparams.eAttachType=GeomType_Render;
		spawnparams.bIndependent=true;
		spawnparams.bCountPerUnit=1;
		spawnparams.fCountScale=1.0f;

		gEnv->pEntitySystem->GetBreakableManager()->AttachSurfaceEffect(pEntity, 0, SURFACE_BREAKAGE_TYPE("freeze_shatter"), spawnparams);
	*/
	IBreakableManager::BreakageParams breakage;
	breakage.type = IBreakableManager::BREAKAGE_TYPE_FREEZE_SHATTER;
	breakage.bMaterialEffects = true;			// Automatically create "destroy" and "breakage" material effects on pieces.
	breakage.fParticleLifeTime = 7.0f;		// Average lifetime of particle pieces.
	breakage.nGenericCount = 0;						// If not 0, force particle pieces to spawn generically, this many times.
	breakage.bForceEntity = false;				// Force pieces to spawn as entities.
	breakage.bOnlyHelperPieces = false;	 // Only spawn helper pieces.
	// Impulse params.
	breakage.fExplodeImpulse = 10.0f;
	breakage.vHitImpulse = impulse;
	breakage.vHitPoint = pos;
	gEnv->pEntitySystem->GetBreakableManager()->BreakIntoPieces(pEntity, 0, nFrozenSlot, breakage);

	// send event to game object
	if (pGameObject)
	{
		SGameObjectEvent event(eCGE_PostShatter, eGOEF_ToAll, IGameObjectSystem::InvalidExtensionID, 0);
		pGameObject->SendEvent(event);
	}

	// call script event
	if (pScriptTable && pScriptTable->GetValueType("OnPostShatter") == svtFunction)
	{
		Script::CallMethod(pScriptTable, "OnPostShatter", pos, impulse);
	}

	// shatter children
	int n = pEntity->GetChildCount();

	for (int i = n - 1; i >= 0; --i)
	{
		if (IEntity *pChild = pEntity->GetChild(i))
		{
			ShatterEntity(pChild->GetId(), pos, impulse);
		}
	}

	FreezeEntity(entityId, false, false);

	if (gEnv->bServer && !m_pGameFramework->IsEditing())
	{
		GetGameObject()->InvokeRMIWithDependentObject(ClShatterEntity(), ShatterEntityParams(entityId, pos, impulse), eRMI_ToRemoteClients, entityId);
		// make sure we don't remove players
		IActor *pActor = m_pGameFramework->GetIActorSystem()->GetActor(entityId);

		if (!pActor || !pActor->IsPlayer())
		{
			if (gEnv->IsEditor())
			{
				pEntity->Hide(true);
			}
			else
			{
				m_pEntitySystem->RemoveEntity(entityId);
			}
		}
	}
	else if (m_pGameFramework->IsEditing())
	{
		pEntity->Hide(true);
	}
}


struct compare_spawns
{
	bool operator() (EntityId lhs, EntityId rhs ) const
	{
		int lhsT = g_pGame->GetGameRules()->GetTeam(lhs);
		int rhsT = g_pGame->GetGameRules()->GetTeam(rhs);

		if (lhsT == rhsT)
		{
			EntityId lhsG = g_pGame->GetGameRules()->GetSpawnLocationGroup(lhs);
			EntityId rhsG = g_pGame->GetGameRules()->GetSpawnLocationGroup(rhs);

			if (lhsG == rhsG)
			{
				IEntity *pLhs = gEnv->pEntitySystem->GetEntity(lhs);
				IEntity *pRhs = gEnv->pEntitySystem->GetEntity(rhs);
				return strcmp(pLhs->GetName(), pRhs->GetName()) < 0;
			}

			return lhsG < rhsG;
		}

		return lhsT < rhsT;
	}
};

//------------------------------------------------------------------------
void CGameRules::AddSpawnLocation(EntityId location)
{
	stl::push_back_unique(m_spawnLocations, location);
	std::sort(m_spawnLocations.begin(), m_spawnLocations.end(), compare_spawns());
}

//------------------------------------------------------------------------
void CGameRules::RemoveSpawnLocation(EntityId id)
{
	stl::find_and_erase(m_spawnLocations, id);
	//std::sort(m_spawnLocations.begin(), m_spawnLocations.end(), compare_spawns());
}

//------------------------------------------------------------------------
int CGameRules::GetSpawnLocationCount() const
{
	return (int)m_spawnLocations.size();
}

//------------------------------------------------------------------------
EntityId CGameRules::GetSpawnLocation(int idx) const
{
	if (idx >= 0 && idx < (int)m_spawnLocations.size())
	{
		return m_spawnLocations[idx];
	}

	return 0;
}

//------------------------------------------------------------------------
void CGameRules::GetSpawnLocations(TSpawnLocations &locations) const
{
	locations.resize(0);
	locations = m_spawnLocations;
}

//------------------------------------------------------------------------
bool CGameRules::IsSpawnLocationSafe(EntityId playerId, EntityId spawnLocationId, float safeDistance, bool ignoreTeam, float zoffset) const
{
	IEntity *pSpawn = gEnv->pEntitySystem->GetEntity(spawnLocationId);

	if (!pSpawn)
	{
		return false;
	}

	if (safeDistance <= 0.01f)
	{
		return true;
	}

	int playerTeamId = GetTeam(playerId);
	SEntityProximityQuery query;
	Vec3	c(pSpawn->GetWorldPos());
	float l(safeDistance * 1.5f);
	float safeDistanceSq = safeDistance * safeDistance;
	query.box = AABB(Vec3(c.x - l, c.y - l, c.z - 0.15f), Vec3(c.x + l, c.y + l, c.z + 2.0f));
	query.pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Player");
	gEnv->pEntitySystem->QueryProximity(query);
	bool result = true;

	if (zoffset <= 0.0001f)
	{
		for (int i = 0; i < query.nCount; i++)
		{
			EntityId entityId = query.pEntities[i]->GetId();

			if (playerId == entityId) // ignore self
			{
				continue;
			}

			CActor *pActor = static_cast<CActor *>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(entityId));

			if (pActor && pActor->GetSpectatorMode() != 0) // ignore spectators
			{
				continue;
			}

			if (playerTeamId && playerTeamId == GetTeam(entityId)) // ignore team players on team games
			{
				if (pActor && (pActor->GetEntity()->GetWorldPos() - c).len2() <= safeDistanceSq) // only if they are not too close
				{
					result = false;
					break;
				}

				continue;
			}

			result = false;
			break;
		}
	}
	else
	{
		result = TestSpawnLocationWithEnvironment(spawnLocationId, playerId, zoffset, 2.0f);
	}

	return result;
}

//------------------------------------------------------------------------
bool CGameRules::IsSpawnLocationFarEnough(EntityId spawnLocationId, float minDistance, const Vec3 &testPosition) const
{
	if (minDistance <= 0.1f)
	{
		return true;
	}

	IEntity *pSpawn = gEnv->pEntitySystem->GetEntity(spawnLocationId);

	if (!pSpawn)
	{
		return false;
	}

	if ((pSpawn->GetWorldPos() - testPosition).len2() < minDistance * minDistance)
	{
		return false;
	}

	return true;
}

//------------------------------------------------------------------------
bool CGameRules::TestSpawnLocationWithEnvironment(EntityId spawnLocationId, EntityId playerId, float offset, float height) const
{
	if (!spawnLocationId)
	{
		return false;
	}

	IEntity *pSpawn = gEnv->pEntitySystem->GetEntity(spawnLocationId);

	if (!pSpawn)
	{
		return false;
	}

	IPhysicalEntity *pPlayerPhysics = 0;
	IEntity *pPlayer = gEnv->pEntitySystem->GetEntity(playerId);

	if (pPlayer)
	{
		pPlayerPhysics = pPlayer->GetPhysics();
	}

	static float r = 0.3f;
	primitives::sphere sphere;
	sphere.center = pSpawn->GetWorldPos();
	sphere.r = r;
	sphere.center.z += offset + r;
	Vec3 end = sphere.center;
	end.z += height - 2.0f * r;
	geom_contact *pContact = 0;
	float dst = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, end - sphere.center, ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid | ent_living,
				&pContact, 0, geom_colltype_player, 0, 0, 0, &pPlayerPhysics, pPlayerPhysics ? 1 : 0);

	if(dst > 0.001f)
	{
		return false;
	}
	else
	{
		return true;
	}
}

//------------------------------------------------------------------------
EntityId CGameRules::GetSpawnLocation(EntityId playerId, bool ignoreTeam, bool includeNeutral, EntityId groupId, float minDistToDeath, const Vec3 &deathPos, float *pZOffset) const
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);
	const TSpawnLocations *locations = 0;

	if (groupId)
	{
		TSpawnGroupMap::const_iterator it = m_spawnGroups.find(groupId);

		if (it == m_spawnGroups.end())
		{
			return 0;
		}

		locations = &it->second;
	}
	else
	{
		locations = &m_spawnLocations;
	}

	if (locations->empty())
	{
		return 0;
	}

	static TSpawnLocations candidates;
	candidates.resize(0);
	int playerTeamId = GetTeam(playerId);

	for (TSpawnLocations::const_iterator it = locations->begin(); it != locations->end(); ++it)
	{
		int teamId = GetTeam(*it);

		if ((ignoreTeam || playerTeamId == teamId) || (!teamId && includeNeutral))
		{
			candidates.push_back(*it);
		}
	}

	int n = candidates.size();

	if (!n)
	{
		return 0;
	}

	int s = Random(n);
	int i = s;
	float mdtd = minDistToDeath;
	float zoffset = 0.0f;
	float safeDistance = 0.82f; // this is 2x the radius of a player collider (capsule/cylinder)

	while (!IsSpawnLocationSafe(playerId, candidates[i], safeDistance, ignoreTeam, zoffset) ||
			!IsSpawnLocationFarEnough(candidates[i], mdtd, deathPos))
	{
		++i;

		if (i == n)
		{
			i = 0;
		}

		if (i == s)
		{
			if (mdtd > 0.0f && mdtd == minDistToDeath) // if we have a min distance to death point
			{
				mdtd *= 0.5f;    // half it and see if it helps
			}
			else if (mdtd > 0.0f)										// if that didn't help
			{
				mdtd = 0.0f;    // ignore death point
			}
			else if (zoffset == 0.0f)								// nothing worked, so we'll have to resort to height offset
			{
				zoffset = 2.0f;
			}
			else
			{
				return 0;    // can't do anything else, just don't spawn and wait for the situation to clear up
			}

			s = Random(n);													// select a random starting point again
			i = s;
		}
	}

	if (pZOffset)
	{
		*pZOffset = zoffset;
	}

	return candidates[i];
}

//------------------------------------------------------------------------
EntityId CGameRules::GetFirstSpawnLocation(int teamId, EntityId groupId) const
{
	if (!m_spawnLocations.empty())
	{
		for (TSpawnLocations::const_iterator it = m_spawnLocations.begin(); it != m_spawnLocations.end(); ++it)
		{
			if (teamId == GetTeam(*it) && (!groupId || groupId == GetSpawnLocationGroup(*it)))
			{
				return *it;
			}
		}
	}

	return 0;
}

//------------------------------------------------------------------------
void CGameRules::AddSpawnGroup(EntityId groupId)
{
	if (m_spawnGroups.find(groupId) == m_spawnGroups.end())
	{
		m_spawnGroups.insert(TSpawnGroupMap::value_type(groupId, TSpawnLocations()));
	}

	if (gEnv->bServer)
	{
		GetGameObject()->InvokeRMIWithDependentObject(ClAddSpawnGroup(), SpawnGroupParams(groupId), eRMI_ToAllClients | eRMI_NoLocalCalls, groupId);
	}
}

//------------------------------------------------------------------------
void CGameRules::AddSpawnLocationToSpawnGroup(EntityId groupId, EntityId location)
{
	TSpawnGroupMap::iterator it = m_spawnGroups.find(groupId);

	if (it == m_spawnGroups.end())
	{
		return;
	}

	stl::push_back_unique(it->second, location);
	std::sort(m_spawnLocations.begin(), m_spawnLocations.end(), compare_spawns()); // need to resort spawn location
}

//------------------------------------------------------------------------
void CGameRules::RemoveSpawnLocationFromSpawnGroup(EntityId groupId, EntityId location)
{
	TSpawnGroupMap::iterator it = m_spawnGroups.find(groupId);

	if (it == m_spawnGroups.end())
	{
		return;
	}

	stl::find_and_erase(it->second, location);
	std::sort(m_spawnLocations.begin(), m_spawnLocations.end(), compare_spawns()); // need to resort spawn location
}

//------------------------------------------------------------------------
void CGameRules::RemoveSpawnGroup(EntityId groupId)
{
	TSpawnGroupMap::iterator it = m_spawnGroups.find(groupId);

	if (it != m_spawnGroups.end())
	{
		m_spawnGroups.erase(it);
	}

	std::sort(m_spawnLocations.begin(), m_spawnLocations.end(), compare_spawns()); // need to resort spawn location

	if (gEnv->bServer)
	{
		GetGameObject()->InvokeRMI(ClRemoveSpawnGroup(), SpawnGroupParams(groupId), eRMI_ToAllClients | eRMI_NoLocalCalls, groupId);
		TTeamIdEntityIdMap::iterator next;

		for (TTeamIdEntityIdMap::iterator dit = m_teamdefaultspawns.begin(); dit != m_teamdefaultspawns.end(); dit = next)
		{
			next = dit;
			++next;

			if (dit->second == groupId)
			{
				m_teamdefaultspawns.erase(dit);
			}
		}
	}

	CheckSpawnGroupValidity(groupId);
}

//------------------------------------------------------------------------
EntityId CGameRules::GetSpawnLocationGroup(EntityId spawnId) const
{
	for (TSpawnGroupMap::const_iterator it = m_spawnGroups.begin(); it != m_spawnGroups.end(); ++it)
	{
		TSpawnLocations::const_iterator sit = std::find(it->second.begin(), it->second.end(), spawnId);

		if (sit != it->second.end())
		{
			return it->first;
		}
	}

	return 0;
}

//------------------------------------------------------------------------
int CGameRules::GetSpawnGroupCount() const
{
	return (int)m_spawnGroups.size();
}

//------------------------------------------------------------------------
EntityId CGameRules::GetSpawnGroup(int idx) const
{
	if (idx >= 0 && idx < (int)m_spawnGroups.size())
	{
		TSpawnGroupMap::const_iterator it = m_spawnGroups.begin();
		std::advance(it, idx);
		return it->first;
	}

	return 0;
}

//------------------------------------------------------------------------
void CGameRules::GetSpawnGroups(TSpawnLocations &groups) const
{
	groups.resize(0);
	groups.reserve(m_spawnGroups.size());

	for (TSpawnGroupMap::const_iterator it = m_spawnGroups.begin(); it != m_spawnGroups.end(); ++it)
	{
		groups.push_back(it->first);
	}
}

//------------------------------------------------------------------------
bool CGameRules::IsSpawnGroup(EntityId id) const
{
	TSpawnGroupMap::const_iterator it = m_spawnGroups.find(id);
	return it != m_spawnGroups.end();
}

//------------------------------------------------------------------------
void CGameRules::RequestSpawnGroup(EntityId spawnGroupId)
{
	CallScript(m_script, "RequestSpawnGroup", ScriptHandle(spawnGroupId));
}

//------------------------------------------------------------------------
void CGameRules::SetPlayerSpawnGroup(EntityId playerId, EntityId spawnGroupId)
{
	CallScript(m_script, "SetPlayerSpawnGroup", ScriptHandle(playerId), ScriptHandle(spawnGroupId));
}

//------------------------------------------------------------------------
EntityId CGameRules::GetPlayerSpawnGroup(CActor *pActor)
{
	if (!m_script || m_script->GetValueType("GetPlayerSpawnGroup") != svtFunction)
	{
		return 0;
	}

	ScriptHandle ret(0);
	m_pScriptSystem->BeginCall(m_script, "GetPlayerSpawnGroup");
	m_pScriptSystem->PushFuncParam(m_script);
	m_pScriptSystem->PushFuncParam(pActor->GetEntity()->GetScriptTable());
	m_pScriptSystem->EndCall(ret);
	return (EntityId)ret.n;
}

//------------------------------------------------------------------------
void CGameRules::SetTeamDefaultSpawnGroup(int teamId, EntityId spawnGroupId)
{
	TTeamIdEntityIdMap::iterator it = m_teamdefaultspawns.find(teamId);

	if (it != m_teamdefaultspawns.end())
	{
		it->second = spawnGroupId;
	}
	else
	{
		m_teamdefaultspawns.insert(TTeamIdEntityIdMap::value_type(teamId, spawnGroupId));
	}
}

//------------------------------------------------------------------------
EntityId CGameRules::GetTeamDefaultSpawnGroup(int teamId)
{
	TTeamIdEntityIdMap::iterator it = m_teamdefaultspawns.find(teamId);

	if (it != m_teamdefaultspawns.end())
	{
		return it->second;
	}

	return 0;
}

//------------------------------------------------------------------------
void CGameRules::CheckSpawnGroupValidity(EntityId spawnGroupId)
{
	bool exists = spawnGroupId &&
				  (m_spawnGroups.find(spawnGroupId) != m_spawnGroups.end()) &&
				  (gEnv->pEntitySystem->GetEntity(spawnGroupId) != 0);
	bool valid = exists && GetTeam(spawnGroupId) != 0;

	for (std::vector<int>::const_iterator it = m_channelIds.begin(); it != m_channelIds.end(); ++it)
	{
		CActor *pActor = GetActorByChannelId(*it);

		if (!pActor)
		{
			continue;
		}

		EntityId playerId = pActor->GetEntityId();

		if (GetPlayerSpawnGroup(pActor) == spawnGroupId)
		{
			if (!valid || GetTeam(spawnGroupId) != GetTeam(playerId))
			{
				CallScript(m_serverScript, "OnSpawnGroupInvalid", ScriptHandle(playerId), ScriptHandle(spawnGroupId));
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::AddSpectatorLocation(EntityId location)
{
	stl::push_back_unique(m_spectatorLocations, location);
}

//------------------------------------------------------------------------
void CGameRules::RemoveSpectatorLocation(EntityId id)
{
	stl::find_and_erase(m_spectatorLocations, id);
}

//------------------------------------------------------------------------
int CGameRules::GetSpectatorLocationCount() const
{
	return (int)m_spectatorLocations.size();
}

//------------------------------------------------------------------------
EntityId CGameRules::GetSpectatorLocation(int idx) const
{
	if (idx >= 0 && idx < m_spectatorLocations.size())
	{
		return m_spectatorLocations[idx];
	}

	return 0;
}

//------------------------------------------------------------------------
void CGameRules::GetSpectatorLocations(TSpawnLocations &locations) const
{
	locations.resize(0);
	locations = m_spectatorLocations;
}

//------------------------------------------------------------------------
EntityId CGameRules::GetRandomSpectatorLocation() const
{
	int idx = Random(GetSpectatorLocationCount());
	return GetSpectatorLocation(idx);
}

//------------------------------------------------------------------------
EntityId CGameRules::GetInterestingSpectatorLocation() const
{
	return GetRandomSpectatorLocation();
}

//------------------------------------------------------------------------
void CGameRules::ResetMinimap()
{
	m_minimap.resize(0);

	if (gEnv->bServer)
	{
		GetGameObject()->InvokeRMI(ClResetMinimap(), NoParams(), eRMI_ToAllClients | eRMI_NoLocalCalls);
	}
}

//------------------------------------------------------------------------
void CGameRules::UpdateMinimap(float frameTime)
{
	TMinimap::iterator next;

	for (TMinimap::iterator eit = m_minimap.begin(); eit != m_minimap.end(); eit = next)
	{
		next = eit;
		++next;
		SMinimapEntity &entity = *eit;

		if (entity.lifetime > 0.0f)
		{
			entity.lifetime -= frameTime;

			if (entity.lifetime <= 0.0f)
			{
				next = m_minimap.erase(eit);
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::AddMinimapEntity(EntityId entityId, int type, float lifetime)
{
	TMinimap::iterator it = std::find(m_minimap.begin(), m_minimap.end(), SMinimapEntity(entityId, 0, 0.0f));

	if (it != m_minimap.end())
	{
		if (type > it->type)
		{
			it->type = type;
		}

		if (lifetime == 0.0f || lifetime > it->lifetime)
		{
			it->lifetime = lifetime;
		}
	}
	else
	{
		m_minimap.push_back(SMinimapEntity(entityId, type, lifetime));
	}

	if (gEnv->bServer)
	{
		GetGameObject()->InvokeRMIWithDependentObject(ClAddMinimapEntity(), AddMinimapEntityParams(entityId, lifetime, (int)type), eRMI_ToAllClients | eRMI_NoLocalCalls, entityId);
	}
}

//------------------------------------------------------------------------
void CGameRules::RemoveMinimapEntity(EntityId entityId)
{
	stl::find_and_erase(m_minimap, SMinimapEntity(entityId, 0, 0.0f));

	if (gEnv->bServer)
	{
		GetGameObject()->InvokeRMI(ClRemoveMinimapEntity(), EntityParams(entityId), eRMI_ToAllClients | eRMI_NoLocalCalls);
	}
}

//------------------------------------------------------------------------
const CGameRules::TMinimap &CGameRules::GetMinimapEntities() const
{
	return m_minimap;
}

//------------------------------------------------------------------------
void CGameRules::AddHitListener(IHitListener *pHitListener)
{
	stl::push_back_unique(m_hitListeners, pHitListener);
}

//------------------------------------------------------------------------
void CGameRules::RemoveHitListener(IHitListener *pHitListener)
{
	stl::find_and_erase(m_hitListeners, pHitListener);
}

//------------------------------------------------------------------------
void CGameRules::AddGameRulesListener(SGameRulesListener *pRulesListener)
{
	stl::push_back_unique(m_rulesListeners, pRulesListener);
}

//------------------------------------------------------------------------
void CGameRules::RemoveGameRulesListener(SGameRulesListener *pRulesListener)
{
	stl::find_and_erase(m_rulesListeners, pRulesListener);
}

//------------------------------------------------------------------------
int CGameRules::RegisterHitMaterial(const char *materialName)
{
	if (int id = GetHitMaterialId(materialName))
	{
		return id;
	}

	ISurfaceType *pSurfaceType = m_pMaterialManager->GetSurfaceTypeByName(materialName);

	if (pSurfaceType)
	{
		m_hitMaterials.insert(THitMaterialMap::value_type(++m_hitMaterialIdGen, pSurfaceType->GetId()));
		return m_hitMaterialIdGen;
	}

	return 0;
}

//------------------------------------------------------------------------
int CGameRules::GetHitMaterialId(const char *materialName) const
{
	ISurfaceType *pSurfaceType = m_pMaterialManager->GetSurfaceTypeByName(materialName);

	if (!pSurfaceType)
	{
		return 0;
	}

	int id = pSurfaceType->GetId();

	for (THitMaterialMap::const_iterator it = m_hitMaterials.begin(); it != m_hitMaterials.end(); ++it)
	{
		if (it->second == id)
		{
			return it->first;
		}
	}

	return 0;
}

//------------------------------------------------------------------------
int CGameRules::GetHitMaterialIdFromSurfaceId(int surfaceId) const
{
	for (THitMaterialMap::const_iterator it = m_hitMaterials.begin(); it != m_hitMaterials.end(); ++it)
	{
		if (it->second == surfaceId)
		{
			return it->first;
		}
	}

	return 0;
}

//------------------------------------------------------------------------
ISurfaceType *CGameRules::GetHitMaterial(int id) const
{
	THitMaterialMap::const_iterator it = m_hitMaterials.find(id);

	if (it == m_hitMaterials.end())
	{
		return 0;
	}

	ISurfaceType *pSurfaceType = m_pMaterialManager->GetSurfaceType(it->second);
	return pSurfaceType;
}

//------------------------------------------------------------------------
void CGameRules::ResetHitMaterials()
{
	m_hitMaterials.clear();
	m_hitMaterialIdGen = 0;
}

//------------------------------------------------------------------------
int CGameRules::RegisterHitType(const char *type)
{
	if (int id = GetHitTypeId(type))
	{
		return id;
	}

	m_hitTypes.insert(THitTypeMap::value_type(++m_hitTypeIdGen, type));
	return m_hitTypeIdGen;
}

//------------------------------------------------------------------------
int CGameRules::GetHitTypeId(const char *type) const
{
	for (THitTypeMap::const_iterator it = m_hitTypes.begin(); it != m_hitTypes.end(); ++it)
	{
		if (it->second == type)
		{
			return it->first;
		}
	}

	return 0;
}

//------------------------------------------------------------------------
const char *CGameRules::GetHitType(int id) const
{
	THitTypeMap::const_iterator it = m_hitTypes.find(id);

	if (it == m_hitTypes.end())
	{
		return 0;
	}

	return it->second.c_str();
}

//------------------------------------------------------------------------
void CGameRules::ResetHitTypes()
{
	m_hitTypes.clear();
	m_hitTypeIdGen = 0;
}

//------------------------------------------------------------------------
void CGameRules::SendTextMessage(ETextMessageType type, const char *msg, unsigned int to, int channelId,
								 const char *p0, const char *p1, const char *p2, const char *p3)
{
	GetGameObject()->InvokeRMI(ClTextMessage(), TextMessageParams(type, msg, p0, p1, p2, p3), to, channelId);
}

//------------------------------------------------------------------------
bool CGameRules::CanReceiveChatMessage(EChatMessageType type, EntityId sourceId, EntityId targetId) const
{
	if(sourceId == targetId)
	{
		return true;
	}

	bool sspec = !IsPlayerActivelyPlaying(sourceId);
	bool sdead = IsDead(sourceId);
	bool tspec = !IsPlayerActivelyPlaying(targetId);
	bool tdead = IsDead(targetId);

	if(sdead != tdead)
	{
		//CryLog("Disallowing msg (dead): source %d, target %d, sspec %d, sdead %d, tspec %d, tdead %d", sourceId, targetId, sspec, sdead, tspec, tdead);
		return false;
	}

	if(!(tspec || (sspec == tspec)))
	{
		//CryLog("Disallowing msg (spec): source %d, target %d, sspec %d, sdead %d, tspec %d, tdead %d", sourceId, targetId, sspec, sdead, tspec, tdead);
		return false;
	}

	//CryLog("Allowing msg: source %d, target %d, sspec %d, sdead %d, tspec %d, tdead %d", sourceId, targetId, sspec, sdead, tspec, tdead);
	return true;
}

//------------------------------------------------------------------------
void CGameRules::ChatLog(EChatMessageType type, EntityId sourceId, EntityId targetId, const char *msg)
{
	IEntity *pSource = gEnv->pEntitySystem->GetEntity(sourceId);
	IEntity *pTarget = gEnv->pEntitySystem->GetEntity(targetId);
	const char *sourceName = pSource ? pSource->GetName() : "<unknown>";
	const char *targetName = pTarget ? pTarget->GetName() : "<unknown>";
	int teamId = GetTeam(sourceId);
	char tempBuffer[64];

	switch (type)
	{
		case eChatToTeam:
			if (teamId)
			{
				targetName = tempBuffer;
				sprintf(tempBuffer, "Team %s", GetTeamName(teamId));
			}
			else
			{
			case eChatToAll:
				targetName = "ALL";
			}

			break;
	}

	CryLogAlways("CHAT %s to %s:", sourceName, targetName);
	CryLogAlways("   %s", msg);
}

//------------------------------------------------------------------------
void CGameRules::SendChatMessage(EChatMessageType type, EntityId sourceId, EntityId targetId, const char *msg)
{
	ChatMessageParams params(type, sourceId, targetId, msg, (type == eChatToTeam) ? true : false);
	bool sdead = IsDead(sourceId);
	bool sspec = IsSpectator(sourceId);
	ChatLog(type, sourceId, targetId, msg);

	if (gEnv->bServer)
	{
		switch(type)
		{
			case eChatToTarget:
				{
					if (CanReceiveChatMessage(type, sourceId, targetId))
					{
						GetGameObject()->InvokeRMIWithDependentObject(ClChatMessage(), params, eRMI_ToClientChannel, targetId, GetChannelId(targetId));
					}
				}
				break;

			case eChatToAll:
				{
					std::vector<int>::const_iterator begin = m_channelIds.begin();
					std::vector<int>::const_iterator end = m_channelIds.end();

					for (std::vector<int>::const_iterator it = begin; it != end; ++it)
					{
						if (CActor *pActor = GetActorByChannelId(*it))
						{
							if (CanReceiveChatMessage(type, sourceId, pActor->GetEntityId()) && IsPlayerInGame(pActor->GetEntityId()))
							{
								GetGameObject()->InvokeRMIWithDependentObject(ClChatMessage(), params, eRMI_ToClientChannel, pActor->GetEntityId(), *it);
							}
						}
					}
				}
				break;

			case eChatToTeam:
				{
					int teamId = GetTeam(sourceId);

					if (teamId)
					{
						TPlayerTeamIdMap::const_iterator tit = m_playerteams.find(teamId);

						if (tit != m_playerteams.end())
						{
							TPlayers::const_iterator begin = tit->second.begin();
							TPlayers::const_iterator end = tit->second.end();

							for (TPlayers::const_iterator it = begin; it != end; ++it)
							{
								if (CanReceiveChatMessage(type, sourceId, *it))
								{
									GetGameObject()->InvokeRMIWithDependentObject(ClChatMessage(), params, eRMI_ToClientChannel, *it, GetChannelId(*it));
								}
							}
						}
					}
				}
				break;
		}
	}
	else
	{
		GetGameObject()->InvokeRMI(SvRequestChatMessage(), params, eRMI_ToServer);
	}
}

//------------------------------------------------------------------------
void CGameRules::ForbiddenAreaWarning(bool active, int timer, EntityId targetId)
{
	GetGameObject()->InvokeRMI(ClForbiddenAreaWarning(), ForbiddenAreaWarningParams(active, timer), eRMI_ToClientChannel, GetChannelId(targetId));
}

//------------------------------------------------------------------------
void CGameRules::ResetGameTime()
{
	m_endTime.SetSeconds(0.0f);
	m_gameStartedTime = m_cachedServerTime;
	float timeLimit = g_pGameCVars->g_timelimit;

	if (timeLimit > 0.0f)
	{
		m_endTime.SetSeconds(m_pGameFramework->GetServerTime().GetSeconds() + timeLimit * 60.0f);
	}

	GetGameObject()->InvokeRMI(ClSetGameTime(), SetGameTimeParams(m_endTime), eRMI_ToRemoteClients);
}

//------------------------------------------------------------------------
float CGameRules::GetRemainingGameTime() const
{
	return MAX(0, (m_endTime - m_pGameFramework->GetServerTime()).GetSeconds());
}

//------------------------------------------------------------------------
void CGameRules::SetRemainingGameTime(float seconds)
{
	if (!g_pGame->IsGameSessionHostMigrating() || !gEnv->bServer)
	{
		// This function should only ever be called as part of the host migration
		// process, when the new server is being created
		GameWarning("CGameRules::SetRemainingGameTime() should only be called by the new server during host migration");
		return;
	}

	float currentTime = m_cachedServerTime.GetSeconds();
	float timeLimit = MAX(m_timeLimit * 60.0f, currentTime + seconds);
	// Set the start of the game at the appropriate point back in time...
	m_gameStartedTime.SetSeconds(currentTime + seconds - timeLimit);
}

//------------------------------------------------------------------------
bool CGameRules::IsClientFriendlyProjectile(const EntityId projectileId, const EntityId targetEntityId)
{
	return false;
}

//------------------------------------------------------------------------
bool CGameRules::IsTimeLimited() const
{
	return m_endTime.GetSeconds() > 0.0f;
}

//------------------------------------------------------------------------
void CGameRules::ResetRoundTime()
{
	m_roundEndTime.SetSeconds(0.0f);
	float roundTime = g_pGameCVars->g_roundtime;

	if (roundTime > 0.0f)
	{
		m_roundEndTime.SetSeconds(m_pGameFramework->GetServerTime().GetSeconds() + roundTime * 60.0f);
	}

	GetGameObject()->InvokeRMI(ClSetRoundTime(), SetGameTimeParams(m_roundEndTime), eRMI_ToRemoteClients);
}

//------------------------------------------------------------------------
float CGameRules::GetRemainingRoundTime() const
{
	return MAX(0, (m_roundEndTime - m_pGameFramework->GetServerTime()).GetSeconds());
}

//------------------------------------------------------------------------
bool CGameRules::IsRoundTimeLimited() const
{
	return m_roundEndTime.GetSeconds() > 0.0f;
}

//------------------------------------------------------------------------
void CGameRules::ResetPreRoundTime()
{
	m_preRoundEndTime.SetSeconds(0.0f);
	int preRoundTime = g_pGameCVars->g_preroundtime;

	if (preRoundTime > 0)
	{
		m_preRoundEndTime.SetSeconds(m_pGameFramework->GetServerTime().GetSeconds() + preRoundTime);
	}

	GetGameObject()->InvokeRMI(ClSetPreRoundTime(), SetGameTimeParams(m_preRoundEndTime), eRMI_ToRemoteClients);
}

//------------------------------------------------------------------------
float CGameRules::GetRemainingPreRoundTime() const
{
	return MAX(0, (m_preRoundEndTime - m_pGameFramework->GetServerTime()).GetSeconds());
}

//------------------------------------------------------------------------
void CGameRules::ResetReviveCycleTime()
{
	if (!gEnv->bServer)
	{
		GameWarning("CGameRules::ResetReviveCycleTime() called on client");
		return;
	}

	m_reviveCycleEndTime.SetSeconds(0.0f);

	if (g_pGameCVars->g_revivetime < 5)
	{
		gEnv->pConsole->GetCVar("g_revivetime")->Set(5);
	}

	m_reviveCycleEndTime = m_pGameFramework->GetServerTime() + float(g_pGameCVars->g_revivetime);
	GetGameObject()->InvokeRMI(ClSetReviveCycleTime(), SetGameTimeParams(m_reviveCycleEndTime), eRMI_ToRemoteClients);
}

//------------------------------------------------------------------------
float CGameRules::GetRemainingReviveCycleTime() const
{
	return MAX(0, (m_reviveCycleEndTime - m_pGameFramework->GetServerTime()).GetSeconds());
}


//------------------------------------------------------------------------
void CGameRules::ResetGameStartTimer(float time)
{
	if (!gEnv->bServer)
	{
		GameWarning("CGameRules::ResetGameStartTimer() called on client");
		return;
	}

	m_gameStartTime = m_pGameFramework->GetServerTime() + time;
	GetGameObject()->InvokeRMI(ClSetGameStartTimer(), SetGameTimeParams(m_gameStartTime), eRMI_ToRemoteClients);
}

//------------------------------------------------------------------------
float CGameRules::GetRemainingStartTimer() const
{
	return (m_gameStartTime - m_pGameFramework->GetServerTime()).GetSeconds();
}

//------------------------------------------------------------------------
bool CGameRules::OnCollision(const SGameCollision &event)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	if (!m_onCollisionFunc || IsDemoPlayback())
	{
		return true;
	}

	// filter out self-collisions
	if (event.pSrcEntity == event.pTrgEntity)
	{
		return true;
	}

	// collisions involving partId<-1 are to be ignored by game's damage calculations
	// usually created articially to make stuff break. See CMelee::Impulse
	if (event.pCollision->partid[0] < -1 || event.pCollision->partid[1] < -1)
	{
		return true;
	}

	//Prevent squad-mates being hit by bullets/collision damage from object held by the player
	if(!gEnv->bMultiplayer)
	{
		IEntity *pTarget = event.pCollision->iForeignData[1] == PHYS_FOREIGN_ID_ENTITY ? (IEntity *)event.pCollision->pForeignData[1] : 0;

		if(pTarget)
		{
			if(pTarget->GetId() == m_ignoreEntityNextCollision)
			{
				m_ignoreEntityNextCollision = 0;
				return false;
			}
		}
	}

	// collisions with very low resulting impulse are ignored
	if (event.pCollision->normImpulse <= 0.001f)
	{
		return true;
	}

	static IEntityClass *s_pBasicEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("BasicEntity");
	static IEntityClass *s_pDefaultClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Default");
	bool srcClassFilter = false;
	bool trgClassFilter = false;
	IEntityClass *pSrcClass = 0;

	if (event.pSrcEntity)
	{
		pSrcClass = event.pSrcEntity->GetClass();

		srcClassFilter = (pSrcClass == s_pBasicEntityClass || pSrcClass == s_pDefaultClass);

		if (srcClassFilter && !event.pTrgEntity)
		{
			return true;
		}
	}

	IEntityClass *pTrgClass = 0;

	if (event.pTrgEntity)
	{
		pTrgClass = event.pTrgEntity->GetClass();
		trgClassFilter = (pTrgClass == s_pBasicEntityClass || pTrgClass == s_pDefaultClass);

		if (trgClassFilter && !event.pSrcEntity)
		{
			return true;
		}
	}

	if (srcClassFilter && trgClassFilter)
	{
		return true;
	}

	if (event.pCollision->idmat[0] != s_invulnID && event.pSrcEntity && event.pSrcEntity->GetScriptTable())
	{
		PrepCollision(0, 1, event, event.pTrgEntity);
		Script::CallMethod(m_script, m_onCollisionFunc, event.pSrcEntity->GetScriptTable(), m_collisionTable);
	}

	if (event.pCollision->idmat[1] != s_invulnID && event.pTrgEntity && event.pTrgEntity->GetScriptTable())
	{
		PrepCollision(1, 0, event, event.pSrcEntity);
		Script::CallMethod(m_script, m_onCollisionFunc, event.pTrgEntity->GetScriptTable(), m_collisionTable);
	}

	return true;
}

//------------------------------------------------------------------------
void CGameRules::RegisterConsoleCommands(IConsole *pConsole)
{
	// todo: move to power struggle implementation when there is one
	REGISTER_COMMAND("buy",			"if (g_gameRules and g_gameRules.Buy) then g_gameRules:Buy(%1); end", VF_NULL, "");
	REGISTER_COMMAND("buyammo", "if (g_gameRules and g_gameRules.BuyAmmo) then g_gameRules:BuyAmmo(%%); end", VF_NULL, "");
	REGISTER_COMMAND("g_debug_spawns", CmdDebugSpawns, VF_NULL, "");
	REGISTER_COMMAND("g_debug_minimap", CmdDebugMinimap, VF_NULL, "");
	REGISTER_COMMAND("g_debug_teams", CmdDebugTeams, VF_NULL, "");
	REGISTER_COMMAND("g_debug_objectives", CmdDebugObjectives, VF_NULL, "");
}

//------------------------------------------------------------------------
void CGameRules::UnregisterConsoleCommands(IConsole *pConsole)
{
	pConsole->RemoveCommand("buy");
	pConsole->RemoveCommand("buyammo");
	pConsole->RemoveCommand("g_debug_spawns");
	pConsole->RemoveCommand("g_debug_minimap");
	pConsole->RemoveCommand("g_debug_teams");
	pConsole->RemoveCommand("g_debug_objectives");
}

//------------------------------------------------------------------------
void CGameRules::RegisterConsoleVars(IConsole *pConsole)
{
}


//------------------------------------------------------------------------
void CGameRules::CmdDebugSpawns(IConsoleCmdArgs *pArgs)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();

	if (!pGameRules->m_spawnGroups.empty())
	{
		CryLogAlways("// Spawn Groups //");

		for (TSpawnGroupMap::const_iterator sit = pGameRules->m_spawnGroups.begin(); sit != pGameRules->m_spawnGroups.end(); ++sit)
		{
			IEntity *pEntity = gEnv->pEntitySystem->GetEntity(sit->first);
			int groupTeamId = pGameRules->GetTeam(pEntity->GetId());
			const char *Default = "$5*DEFAULT*";
			CryLogAlways("Spawn Group: %s  (eid: %d %08x  team: %d) %s", pEntity->GetName(), pEntity->GetId(), pEntity->GetId(), groupTeamId,
						 (sit->first == pGameRules->GetTeamDefaultSpawnGroup(groupTeamId)) ? Default : "");

			for (TSpawnLocations::const_iterator lit = sit->second.begin(); lit != sit->second.end(); ++lit)
			{
				int spawnTeamId = pGameRules->GetTeam(pEntity->GetId());
				IEntity *pSpawnEntity = gEnv->pEntitySystem->GetEntity(*lit);
				CryLogAlways("    -> Spawn Location: %s  (eid: %d %08x  team: %d)", pSpawnEntity->GetName(), pSpawnEntity->GetId(), pSpawnEntity->GetId(), spawnTeamId);
			}
		}
	}

	CryLogAlways("// Spawn Locations //");

	for (TSpawnLocations::const_iterator lit = pGameRules->m_spawnLocations.begin(); lit != pGameRules->m_spawnLocations.end(); ++lit)
	{
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(*lit);
		Vec3 pos = pEntity ? pEntity->GetWorldPos() : ZERO;
		CryLogAlways("Spawn Location: %s  (eid: %d %08x  team: %d) %.2f,%.2f,%.2f", pEntity->GetName(), pEntity->GetId(), pEntity->GetId(), pGameRules->GetTeam(pEntity->GetId()), pos.x, pos.y, pos.z);
	}
}

//------------------------------------------------------------------------
void CGameRules::CmdDebugMinimap(IConsoleCmdArgs *pArgs)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();

	if (!pGameRules->m_minimap.empty())
	{
		CryLogAlways("// Minimap Entities //");

		for (TMinimap::const_iterator it = pGameRules->m_minimap.begin(); it != pGameRules->m_minimap.end(); ++it)
		{
			IEntity *pEntity = gEnv->pEntitySystem->GetEntity(it->entityId);
			CryLogAlways("  -> Entity %s  (eid: %d %08x  class: %s  lifetime: %.3f  type: %d)", pEntity->GetName(), pEntity->GetId(), pEntity->GetId(), pEntity->GetClass()->GetName(), it->lifetime, it->type);
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::CmdDebugTeams(IConsoleCmdArgs *pArgs)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();

	if (!pGameRules->m_entityteams.empty())
	{
		CryLogAlways("// Teams //");

		for (TTeamIdMap::const_iterator tit = pGameRules->m_teams.begin(); tit != pGameRules->m_teams.end(); ++tit)
		{
			CryLogAlways("Team: %s  (id: %d)", tit->first.c_str(), tit->second);

			for (TEntityTeamIdMap::const_iterator eit = pGameRules->m_entityteams.begin(); eit != pGameRules->m_entityteams.end(); ++eit)
			{
				if (eit->second == tit->second)
				{
					IEntity *pEntity = gEnv->pEntitySystem->GetEntity(eit->first);
					CryLogAlways("    -> Entity: %s  class: %s  (eid: %d %08x)", pEntity ? pEntity->GetName() : "<null>", pEntity ? pEntity->GetClass()->GetName() : "<null>", eit->first, eit->first);
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::CmdDebugObjectives(IConsoleCmdArgs *pArgs)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();

	if (!pGameRules->m_objectives.empty())
	{
		CryLogAlways("// Teams //");
	}
}
//------------------------------------------------------------------------
void CGameRules::CreateScriptHitInfo(SmartScriptTable &scriptHitInfo, const HitInfo &hitInfo)
{
	CScriptSetGetChain hit(scriptHitInfo);
	{
		hit.SetValue("normal", hitInfo.normal);
		hit.SetValue("pos", hitInfo.pos);
		hit.SetValue("dir", hitInfo.dir);
		hit.SetValue("partId", hitInfo.partId);
		hit.SetValue("backface", hitInfo.normal.Dot(hitInfo.dir) >= 0.0f);
		hit.SetValue("targetId", ScriptHandle(hitInfo.targetId));
		hit.SetValue("shooterId", ScriptHandle(hitInfo.shooterId));
		hit.SetValue("weaponId", ScriptHandle(hitInfo.weaponId));
		hit.SetValue("projectileId", ScriptHandle(hitInfo.projectileId));
		IEntity *pTarget = m_pEntitySystem->GetEntity(hitInfo.targetId);
		IEntity *pShooter = m_pEntitySystem->GetEntity(hitInfo.shooterId);
		IEntity *pWeapon = m_pEntitySystem->GetEntity(hitInfo.weaponId);
		IEntity *pProjectile = m_pEntitySystem->GetEntity(hitInfo.projectileId);
		hit.SetValue("projectile", pProjectile ? pProjectile->GetScriptTable() : (IScriptTable *)0);
		hit.SetValue("target", pTarget ? pTarget->GetScriptTable() : (IScriptTable *)0);
		hit.SetValue("shooter", pShooter ? pShooter->GetScriptTable() : (IScriptTable *)0);
		hit.SetValue("weapon", pWeapon ? pWeapon->GetScriptTable() : (IScriptTable *)0);
		//hit.SetValue("projectile_class", pProjectile?pProjectile->GetClass()->GetName():"");
		hit.SetValue("materialId", hitInfo.material);
		ISurfaceType *pSurfaceType = GetHitMaterial(hitInfo.material);

		if (pSurfaceType)
		{
			hit.SetValue("material", pSurfaceType->GetName());
			hit.SetValue("material_type", pSurfaceType->GetType());
		}
		else
		{
			hit.SetToNull("material");
			hit.SetToNull("material_type");
		}

		hit.SetValue("damage", hitInfo.damage);
		hit.SetValue("radius", hitInfo.radius);
		hit.SetValue("typeId", hitInfo.type);
		const char *type = GetHitType(hitInfo.type);
		hit.SetValue("type", type ? type : "");
		hit.SetValue("remote", hitInfo.remote);
		hit.SetValue("bulletType", hitInfo.bulletType);
		// Check for hit assistance
		float assist = 0.0f;

		if (pShooter &&
				((g_pGameCVars->hit_assistSingleplayerEnabled && !gEnv->bMultiplayer) ||
				 (g_pGameCVars->hit_assistMultiplayerEnabled && gEnv->bMultiplayer)))
		{
			IActor *pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pShooter->GetId());

			if (pActor && pActor->IsPlayer())
			{
				CPlayer *player = (CPlayer *)pActor;
				assist = player->HasHitAssistance() ? 1.0f : 0.0f;
			}
		}

		hit.SetValue("assistance", assist);
	}
}

void CGameRules::CreateHitInfoFromScript(const SmartScriptTable &scriptHitInfo, HitInfo &hitInfo)
{
	CRY_ASSERT(scriptHitInfo.GetPtr());
	CScriptSetGetChain hit(scriptHitInfo);
	{
		hit.GetValue("normal", hitInfo.normal);
		hit.GetValue("pos", hitInfo.pos);
		hit.GetValue("dir", hitInfo.dir);
		hit.GetValue("partId", hitInfo.partId);
		ScriptHandle entId;
		const IEntitySystem *pEntitySystem = gEnv->pEntitySystem;
		hit.GetValue("targetId", entId);
		hitInfo.targetId = static_cast<EntityId>(entId.n);
		hit.GetValue("shooterId", entId);
		hitInfo.shooterId = static_cast<EntityId>(entId.n);
		hit.GetValue("weaponId", entId);
		hitInfo.weaponId = static_cast<EntityId>(entId.n);
		hit.GetValue("projectileId", entId);
		hitInfo.projectileId = static_cast<EntityId>(entId.n);
		unsigned int uProjectileClassId = 0;
		hit.GetValue("projectileClassId", uProjectileClassId);
		hitInfo.projectileClassId = uProjectileClassId;
		unsigned int uWeaponClassId = 0;
		hit.GetValue("weaponClassId", uWeaponClassId);
		hitInfo.weaponClassId = uWeaponClassId;
		hit.GetValue("materialId", hitInfo.material);
		hit.GetValue("damage", hitInfo.damage);
		hit.GetValue("radius", hitInfo.radius);
		hit.GetValue("typeId", hitInfo.type);
		hit.GetValue("remote", hitInfo.remote);
		hit.GetValue("bulletType", hitInfo.bulletType);
	}
}

//------------------------------------------------------------------------
void CGameRules::CreateScriptExplosionInfo(SmartScriptTable &scriptExplosionInfo, const ExplosionInfo &explosionInfo)
{
	CScriptSetGetChain explosion(scriptExplosionInfo);
	{
		explosion.SetValue("pos", explosionInfo.pos);
		explosion.SetValue("dir", explosionInfo.dir);
		explosion.SetValue("shooterId", ScriptHandle(explosionInfo.shooterId));
		explosion.SetValue("weaponId", ScriptHandle(explosionInfo.weaponId));
		IEntity *pShooter = m_pEntitySystem->GetEntity(explosionInfo.shooterId);
		IEntity *pWeapon = m_pEntitySystem->GetEntity(explosionInfo.weaponId);
		explosion.SetValue("shooter", pShooter ? pShooter->GetScriptTable() : (IScriptTable *)0);
		explosion.SetValue("weapon", pWeapon ? pWeapon->GetScriptTable() : (IScriptTable *)0);
		explosion.SetValue("materialId", 0);
		explosion.SetValue("damage", explosionInfo.damage);
		explosion.SetValue("min_radius", explosionInfo.minRadius);
		explosion.SetValue("radius", explosionInfo.radius);
		explosion.SetValue("pressure", explosionInfo.pressure);
		explosion.SetValue("hole_size", explosionInfo.hole_size);
		explosion.SetValue("effect", explosionInfo.effect_name.c_str());
		explosion.SetValue("effectScale", explosionInfo.effect_scale);
		explosion.SetValue("effectClass", explosionInfo.effect_class.c_str());
		explosion.SetValue("typeId", explosionInfo.type);
		const char *type = GetHitType(explosionInfo.type);
		explosion.SetValue("type", type);
		explosion.SetValue("angle", explosionInfo.angle);
		explosion.SetValue("impact", explosionInfo.impact);
		explosion.SetValue("impact_velocity", explosionInfo.impact_velocity);
		explosion.SetValue("impact_normal", explosionInfo.impact_normal);
		explosion.SetValue("impact_targetId", ScriptHandle(explosionInfo.impact_targetId));
		explosion.SetValue("shakeMinR", explosionInfo.shakeMinR);
		explosion.SetValue("shakeMaxR", explosionInfo.shakeMaxR);
		explosion.SetValue("shakeScale", explosionInfo.shakeScale);
		explosion.SetValue("shakeRnd", explosionInfo.shakeRnd);
	}
	SmartScriptTable temp;

	if (scriptExplosionInfo->GetValue("AffectedEntities", temp))
	{
		temp->Clear();
	}

	if (scriptExplosionInfo->GetValue("AffectedEntitiesObstruction", temp))
	{
		temp->Clear();
	}
}

//------------------------------------------------------------------------

void CGameRules::EnableUpdateScores(bool show)
{
	CallScript(m_script, "EnableUpdateScores", show);
}

//------------------------------------------------------------------------
void CGameRules::UpdateAffectedEntitiesSet(TExplosionAffectedEntities &affectedEnts, const pe_explosion *pExplosion)
{
	if (pExplosion)
	{
		for (int i = 0; i < pExplosion->nAffectedEnts; ++i)
		{
			if (IEntity *pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pExplosion->pAffectedEnts[i]))
			{
				if (IScriptTable *pEntityTable = pEntity->GetScriptTable())
				{
					IPhysicalEntity *pEnt = pEntity->GetPhysics();

					if (pEnt)
					{
						float affected = gEnv->pPhysicalWorld->IsAffectedByExplosion(pEnt);
						AddOrUpdateAffectedEntity(affectedEnts, pEntity, affected);
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::AddOrUpdateAffectedEntity(TExplosionAffectedEntities &affectedEnts, IEntity *pEntity, float affected)
{
	TExplosionAffectedEntities::iterator it = affectedEnts.find(pEntity);

	if (it != affectedEnts.end())
	{
		if (it->second < affected)
		{
			it->second = affected;
		}
	}
	else
	{
		affectedEnts.insert(TExplosionAffectedEntities::value_type(pEntity, affected));
	}
}

//------------------------------------------------------------------------
void CGameRules::CommitAffectedEntitiesSet(SmartScriptTable &scriptExplosionInfo, TExplosionAffectedEntities &affectedEnts)
{
	CScriptSetGetChain explosion(scriptExplosionInfo);
	SmartScriptTable affected;
	SmartScriptTable affectedObstruction;

	if (scriptExplosionInfo->GetValue("AffectedEntities", affected) &&
			scriptExplosionInfo->GetValue("AffectedEntitiesObstruction", affectedObstruction))
	{
		if (affectedEnts.empty())
		{
			affected->Clear();
			affectedObstruction->Clear();
		}
		else
		{
			int k = 0;

			for (TExplosionAffectedEntities::const_iterator it = affectedEnts.begin(), end = affectedEnts.end(); it != end; ++it)
			{
				float obstruction = 1.0f - it->second;
				affected->SetAt(++k, it->first->GetScriptTable());
				affectedObstruction->SetAt(k, obstruction);
			}
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::PrepCollision(int src, int trg, const SGameCollision &event, IEntity *pTarget)
{
	const EventPhysCollision *pCollision = event.pCollision;
	CScriptSetGetChain chain(m_collisionTable);
	chain.SetValue("normal", pCollision->n);
	chain.SetValue("pos", pCollision->pt);
	Vec3 dir(0, 0, 0);

	if (pCollision->vloc[src].GetLengthSquared() > 1e-6f)
	{
		dir = pCollision->vloc[src].GetNormalized();
		chain.SetValue("dir", dir);
	}
	else
	{
		chain.SetToNull("dir");
	}

	chain.SetValue("velocity", pCollision->vloc[src]);
	pe_status_living sl;

	if (pCollision->pEntity[src]->GetStatus(&sl) && sl.bSquashed)
	{
		chain.SetValue("target_velocity", pCollision->n * (200.0f * (1 - src * 2)));
		chain.SetValue("target_mass", pCollision->mass[trg] > 0 ? pCollision->mass[trg] : 10000.0f);
	}
	else
	{
		chain.SetValue("target_velocity", pCollision->vloc[trg]);
		chain.SetValue("target_mass", pCollision->mass[trg]);
	}

	chain.SetValue("backface", pCollision->n.Dot(dir) >= 0);
	//chain.SetValue("partid", pCollision->partid[src]);
	//chain.SetValue("backface", pCollision->n.Dot(dir) >= 0);
	/*float deltaE = 0;
	if (pCollision->mass[0])
		deltaE += -pCollision->normImpulse*(pCollision->vloc[0]*pCollision->n + pCollision->normImpulse*0.5f/pCollision->mass[0]);
	if (pCollision->mass[1])
		deltaE +=  pCollision->normImpulse*(pCollision->vloc[1]*pCollision->n - pCollision->normImpulse*0.5f/pCollision->mass[1]);
	chain.SetValue("energy_loss", deltaE);*/

	//IEntity *pTarget = gEnv->pEntitySystem->GetEntityFromPhysics(pCollision->pEntity[trg]);

	//chain.SetValue("target_type", (int)pCollision->pEntity[trg]->GetType());

	if (pTarget)
	{
		ScriptHandle sh;
		sh.n = pTarget->GetId();

		if (pTarget->GetPhysics())
		{
			chain.SetValue("target_type", (int)pTarget->GetPhysics()->GetType());
		}

		chain.SetValue("target_id", sh);

		if (pTarget->GetScriptTable())
		{
			chain.SetValue("target", pTarget->GetScriptTable());
		}
		else
		{
			chain.SetToNull("target");
		}
	}
	else
	{
		chain.SetToNull("target");
		chain.SetToNull("target_id");
	}

	if(pCollision->idmat[trg] == s_barbWireID)
	{
		chain.SetValue("materialId", pCollision->idmat[trg]);    //Pass collision with barbwire to script
	}
	else
	{
		chain.SetValue("materialId", pCollision->idmat[src]);
	}

	//chain.SetValue("target_materialId", pCollision->idmat[trg]);
	//ISurfaceTypeManager *pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
}

//------------------------------------------------------------------------
void CGameRules::Restart()
{
	if (gEnv->bServer)
	{
		CallScript(m_script, "RestartGame", true);
	}
}

//------------------------------------------------------------------------
void CGameRules::NextLevel()
{
	if (!gEnv->bServer)
	{
		return;
	}

	ILevelRotation *pLevelRotation = m_pGameFramework->GetILevelSystem()->GetLevelRotation();

	if (!pLevelRotation->GetLength())
	{
		Restart();
	}
	else
	{
		pLevelRotation->ChangeLevel();
	}
}

//------------------------------------------------------------------------
void CGameRules::ResetEntities()
{
	ResetFrozen();

	while (!m_queuedExplosions.empty())
	{
		m_queuedExplosions.pop();
	}

	while (!m_queuedHits.empty())
	{
		m_queuedHits.pop();
	}

	m_processingHit = 0;
	// remove voice groups too. They'll be recreated when players are put back on their teams after reset.
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	TTeamIdVoiceGroupMap::iterator it = m_teamVoiceGroups.begin();
	TTeamIdVoiceGroupMap::iterator next;

	for(; it != m_teamVoiceGroups.end(); it = next)
	{
		next = it;
		++next;
		m_teamVoiceGroups.erase(it);
	}

#endif
	m_respawns.clear();
	m_entityteams.clear();
	m_teamdefaultspawns.clear();

	for (TPlayerTeamIdMap::iterator tit = m_playerteams.begin(); tit != m_playerteams.end(); tit++)
	{
		tit->second.resize(0);
	}

	g_pGame->GetIGameFramework()->Reset(gEnv->bServer);
	//	SEntityEvent event(ENTITY_EVENT_START_GAME);
	//	gEnv->pEntitySystem->SendEventToAll(event);
}

//------------------------------------------------------------------------
void CGameRules::OnEndGame()
{
	bool isMultiplayer = gEnv->bMultiplayer ;
#ifndef OLD_VOICE_SYSTEM_DEPRECATED

	if (isMultiplayer && gEnv->bServer)
	{
		m_teamVoiceGroups.clear();
	}

#endif

	if(gEnv->IsClient())
	{
		IActionMapManager *pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();
		pActionMapMan->EnableActionMap("multiplayer", !isMultiplayer);
		pActionMapMan->EnableActionMap("singleplayer", isMultiplayer);
		IActionMap *am = NULL;

		if(isMultiplayer)
		{
			am = pActionMapMan->GetActionMap("multiplayer");
		}
		else
		{
			am = pActionMapMan->GetActionMap("singleplayer");
		}

		if(am)
		{
			am->SetActionListener(0);
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::GameOver(int localWinner)
{
	if(m_rulesListeners.empty() == false)
	{
		TGameRulesListenerVec::iterator iter = m_rulesListeners.begin();

		while (iter != m_rulesListeners.end())
		{
			(*iter)->GameOver(localWinner);
			++iter;
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::EnteredGame()
{
	if(m_rulesListeners.empty() == false)
	{
		TGameRulesListenerVec::iterator iter = m_rulesListeners.begin();

		while (iter != m_rulesListeners.end())
		{
			(*iter)->EnteredGame();
			++iter;
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::EndGameNear(EntityId id)
{
	if(m_rulesListeners.empty() == false)
	{
		TGameRulesListenerVec::iterator iter = m_rulesListeners.begin();

		while(iter != m_rulesListeners.end())
		{
			(*iter)->EndGameNear(id);
			++iter;
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::CreateEntityRespawnData(EntityId entityId)
{
	if (!gEnv->bServer || m_pGameFramework->IsEditing())
	{
		return;
	}

	IEntity *pEntity = m_pEntitySystem->GetEntity(entityId);

	if (!pEntity)
	{
		return;
	}

	SEntityRespawnData respawn;
	respawn.position = pEntity->GetWorldPos();
	respawn.rotation = pEntity->GetWorldRotation();
	respawn.scale = pEntity->GetScale();
	respawn.flags = pEntity->GetFlags() & ~ENTITY_FLAG_UNREMOVABLE;
	respawn.pClass = pEntity->GetClass();
#ifdef _DEBUG
	respawn.name = pEntity->GetName();
#endif
	IScriptTable *pScriptTable = pEntity->GetScriptTable();

	if (pScriptTable)
	{
		pScriptTable->GetValue("Properties", respawn.properties);
	}

	m_respawndata[entityId] = respawn;
}

//------------------------------------------------------------------------
bool CGameRules::HasEntityRespawnData(EntityId entityId) const
{
	return m_respawndata.find(entityId) != m_respawndata.end();
}

//------------------------------------------------------------------------
void CGameRules::ScheduleEntityRespawn(EntityId entityId, bool unique, float timer)
{
	if (!gEnv->bServer || m_pGameFramework->IsEditing())
	{
		return;
	}

	IEntity *pEntity = m_pEntitySystem->GetEntity(entityId);

	if (!pEntity)
	{
		return;
	}

	SEntityRespawn respawn;
	respawn.timer = timer;
	respawn.unique = unique;
	m_respawns[entityId] = respawn;
}

//------------------------------------------------------------------------
void CGameRules::UpdateEntitySchedules(float frameTime)
{
	if (!gEnv->bServer || m_pGameFramework->IsEditing())
	{
		return;
	}

	TEntityRespawnMap::iterator next;

	for (TEntityRespawnMap::iterator it = m_respawns.begin(); it != m_respawns.end(); it = next)
	{
		next = it;
		++next;
		EntityId id = it->first;
		SEntityRespawn &respawn = it->second;

		if (respawn.unique)
		{
			IEntity *pEntity = m_pEntitySystem->GetEntity(id);

			if (pEntity && !pEntity->IsGarbage())
			{
				continue;
			}
		}

		respawn.timer -= frameTime;

		if (respawn.timer <= 0.0f)
		{
			TEntityRespawnDataMap::iterator dit = m_respawndata.find(id);

			if (dit == m_respawndata.end())
			{
				m_respawns.erase(it);
				continue;
			}

			SEntityRespawnData &data = dit->second;
			SEntitySpawnParams params;
			params.pClass = data.pClass;
			params.qRotation = data.rotation;
			params.vPosition = data.position;
			params.vScale = data.scale;
			params.nFlags = data.flags;
			string name;
#ifdef _DEBUG
			name = data.name;
			name.append("_repop");
#else
			name = data.pClass->GetName();
#endif
			params.sName = name.c_str();
			IEntity *pEntity = m_pEntitySystem->SpawnEntity(params, false);

			if (pEntity && data.properties.GetPtr())
			{
				SmartScriptTable properties;
				IScriptTable *pScriptTable = pEntity->GetScriptTable();

				if (pScriptTable && pScriptTable->GetValue("Properties", properties))
				{
					if (properties.GetPtr())
					{
						properties->Clone(data.properties, true);
					}
				}
			}

			m_pEntitySystem->InitEntity(pEntity, params);
			m_respawns.erase(it);
			m_respawndata.erase(dit);
		}
	}

	TEntityRemovalMap::iterator rnext;

	for (TEntityRemovalMap::iterator it = m_removals.begin(); it != m_removals.end(); it = rnext)
	{
		rnext = it;
		++rnext;
		EntityId id = it->first;
		SEntityRemovalData &removal = it->second;
		IEntity *pEntity = m_pEntitySystem->GetEntity(id);

		if (!pEntity)
		{
			m_removals.erase(it);
			continue;
		}

		if (removal.visibility)
		{
			AABB aabb;
			pEntity->GetWorldBounds(aabb);
			CCamera &camera = m_pSystem->GetViewCamera();

			if (camera.IsAABBVisible_F(aabb))
			{
				removal.timer = removal.time;
				continue;
			}
		}

		removal.timer -= frameTime;

		if (removal.timer <= 0.0f)
		{
			m_pEntitySystem->RemoveEntity(id);
			m_removals.erase(it);
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::ForceScoreboard(bool force)
{
}

//------------------------------------------------------------------------
void CGameRules::FreezeInput(bool freeze)
{
#if !defined(CRY_USE_GCM_HUD)

	if (gEnv->pInput)
	{
		gEnv->pInput->ClearKeyState();
	}

#endif
	g_pGameActions->FilterFreezeTime()->Enable(freeze);
	/*
		if (IActor *pClientIActor=g_pGame->GetIGameFramework()->GetClientActor())
		{
			CActor *pClientActor=static_cast<CActor *>(pClientIActor);
			if (CWeapon *pWeapon=pClientActor->GetWeapon(pClientActor->GetCurrentItemId()))
				pWeapon->StopFire(pClientActor->GetEntityId());
		}
		*/
}

//------------------------------------------------------------------------
void CGameRules::AbortEntityRespawn(EntityId entityId, bool destroyData)
{
	TEntityRespawnMap::iterator it = m_respawns.find(entityId);

	if (it != m_respawns.end())
	{
		m_respawns.erase(it);
	}

	if (destroyData)
	{
		TEntityRespawnDataMap::iterator dit = m_respawndata.find(entityId);

		if (dit != m_respawndata.end())
		{
			m_respawndata.erase(dit);
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::ScheduleEntityRemoval(EntityId entityId, float timer, bool visibility)
{
	if (!gEnv->bServer || m_pGameFramework->IsEditing())
	{
		return;
	}

	IEntity *pEntity = m_pEntitySystem->GetEntity(entityId);

	if (!pEntity)
	{
		return;
	}

	SEntityRemovalData removal;
	removal.time = timer;
	removal.timer = timer;
	removal.visibility = visibility;
	m_removals.insert(TEntityRemovalMap::value_type(entityId, removal));
}

//------------------------------------------------------------------------
void CGameRules::AbortEntityRemoval(EntityId entityId)
{
	TEntityRemovalMap::iterator it = m_removals.find(entityId);

	if (it != m_removals.end())
	{
		m_removals.erase(it);
	}
}

//------------------------------------------------------------------------

void CGameRules::SendRadioMessage(const EntityId sourceId, const int msg)
{
	/*g_pGame->GetIGameFramework()->GetClientActor()->GetEntityId()*/
	RadioMessageParams params(sourceId, msg);

	if(gEnv->bServer)
	{
		if(GetTeamCount() > 1) //team DM or PS
		{
			int teamId = GetTeam(sourceId);

			if (teamId)
			{
				TPlayerTeamIdMap::const_iterator tit = m_playerteams.find(teamId);

				if (tit != m_playerteams.end())
				{
					for (TPlayers::const_iterator it = tit->second.begin(); it != tit->second.end(); ++it)
					{
						GetGameObject()->InvokeRMIWithDependentObject(ClRadioMessage(), params, eRMI_ToClientChannel, *it, GetChannelId(*it));
					}
				}
			}
		}
		else
		{
			GetGameObject()->InvokeRMI(ClRadioMessage(), params, eRMI_ToAllClients);
		}
	}
	else
	{
		GetGameObject()->InvokeRMI(SvRequestRadioMessage(), params, eRMI_ToServer);
	}
}

void CGameRules::OnRadioMessage(const EntityId sourceId, const int msg)
{
}

void CGameRules::RadioMessageParams::SerializeWith(TSerialize ser)
{
	ser.Value("source", sourceId, 'eid');
	ser.Value("msg", msg, 'ui8');
}

void CGameRules::ShowStatus()
{
	float timeRemaining = GetRemainingGameTime();
	int mins = (int)(timeRemaining / 60.0f);
	int secs = (int)(timeRemaining - mins * 60);
	CryLogAlways("time remaining: %d:%02d", mins, secs);
}

void CGameRules::OnAction(const ActionId &actionId, int activationMode, float value)
{
}

void CGameRules::ReconfigureVoiceGroups(EntityId id, int old_team, int new_team)
{
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	INetContext *pNetContext = g_pGame->GetIGameFramework()->GetNetContext();

	if(!pNetContext)
	{
		return;
	}

	IVoiceContext *pVoiceContext = pNetContext->GetVoiceContext();

	if(!pVoiceContext)
	{
		return;    // voice context is now disabled in single player game. talk to me if there are any problems - Lin
	}

	if(old_team == new_team)
	{
		return;
	}

	TTeamIdVoiceGroupMap::iterator iter = m_teamVoiceGroups.find(old_team);

	if(iter != m_teamVoiceGroups.end())
	{
		iter->second->RemoveEntity(id);
		//CryLog("<--Removing entity %d from team %d", id, old_team);
	}
	else
	{
		//CryLog("<--Failed to remove entity %d from team %d", id, old_team);
	}

	iter = m_teamVoiceGroups.find(new_team);

	if(iter == m_teamVoiceGroups.end())
	{
		IVoiceGroup *pVoiceGroup = pVoiceContext->CreateVoiceGroup();
		iter = m_teamVoiceGroups.insert(std::make_pair(new_team, pVoiceGroup)).first;
	}

	iter->second->AddEntity(id);
	pVoiceContext->InvalidateRoutingTable();
	//CryLog("-->Adding entity %d to team %d", id, new_team);
#endif
}

void CGameRules::ForceSynchedStorageSynch(int channel)
{
	if (!gEnv->bServer)
	{
		return;
	}

	g_pGame->GetServerSynchedStorage()->FullSynch(channel, true);
}


void CGameRules::PlayerPosForRespawn(CPlayer *pPlayer, bool save)
{
	static 	Matrix34	respawnPlayerTM(IDENTITY);

	if (save)
	{
		respawnPlayerTM = pPlayer->GetEntity()->GetWorldTM();
	}
	else
	{
		pPlayer->GetEntity()->SetWorldTM(respawnPlayerTM);
	}
}

void CGameRules::SPNotifyPlayerKill(EntityId targetId, EntityId weaponId, bool bHeadShot)
{
	IActor *pActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
	EntityId wepId[1] = {weaponId};

	if (pActor)
	{
		m_pGameplayRecorder->Event(pActor->GetEntity(), GameplayEvent(eGE_Kill, 0, 0, wepId));
	}
}

string CGameRules::GetPlayerName(int channelId, bool bVerifyName)
{
	string playerName;

	if (INetChannel *pNetChannel = m_pGameFramework->GetNetChannel(channelId))
	{
		playerName = pNetChannel->GetNickname();

		if (!playerName.empty() && bVerifyName)
		{
			playerName = VerifyName(playerName);
		}
	}

	return playerName;
}


void CGameRules::GetMemoryUsage(ICrySizer *s) const
{
	s->Add(*this);
	s->AddContainer(m_channelIds);
	s->AddContainer(m_teams);
	s->AddContainer(m_entityteams);
	s->AddContainer(m_channelteams);
	s->AddContainer(m_teamdefaultspawns);
	s->AddContainer(m_playerteams);
	s->AddContainer(m_hitMaterials);
	s->AddContainer(m_hitTypes);
	s->AddContainer(m_respawndata);
	s->AddContainer(m_respawns);
	s->AddContainer(m_removals);
	s->AddContainer(m_minimap);
	s->AddContainer(m_objectives);
	s->AddContainer(m_spawnLocations);
	s->AddContainer(m_spawnGroups);
	s->AddContainer(m_hitListeners);
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	s->AddContainer(m_teamVoiceGroups);
#endif
	s->AddContainer(m_rulesListeners);

	for (TTeamIdMap::const_iterator iter = m_teams.begin(); iter != m_teams.end(); ++iter)
	{
		s->Add(iter->first);
	}

	for (TPlayerTeamIdMap::const_iterator iter = m_playerteams.begin(); iter != m_playerteams.end(); ++iter)
	{
		s->AddContainer(iter->second);
	}

	for (THitTypeMap::const_iterator iter = m_hitTypes.begin(); iter != m_hitTypes.end(); ++iter)
	{
		s->Add(iter->second);
	}

	for (TTeamObjectiveMap::const_iterator iter = m_objectives.begin(); iter != m_objectives.end(); ++iter)
	{
		s->AddContainer(iter->second);
	}

	for (TSpawnGroupMap::const_iterator iter = m_spawnGroups.begin(); iter != m_spawnGroups.end(); ++iter)
	{
		s->AddContainer(iter->second);
	}
}

bool CGameRules::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	switch (aspect)
	{
		case eEA_GameServerDynamic:
			{
				uint32 todFlags = 0;

				if (ser.IsReading())
				{
					todFlags |= ITimeOfDay::NETSER_COMPENSATELAG;

					if (!m_timeOfDayInitialized)
					{
						todFlags |= ITimeOfDay::NETSER_FORCESET;
						m_timeOfDayInitialized = true;
					}
				}

				gEnv->p3DEngine->GetTimeOfDay()->NetSerialize( ser, 0.0f, todFlags );
			}
			break;

		case eEA_GameServerStatic:
			{
				gEnv->p3DEngine->GetTimeOfDay()->NetSerialize( ser, 0.0f, ITimeOfDay::NETSER_STATICPROPS );
			}
			break;
	}

	return true;
}

bool CGameRules::OnBeginCutScene(IAnimSequence *pSeq, bool bResetFX)
{
	if(!pSeq)
	{
		return false;
	}

	m_explosionScreenFX = false;
	return true;
}

bool CGameRules::OnEndCutScene(IAnimSequence *pSeq)
{
	if(!pSeq)
	{
		return false;
	}

	m_explosionScreenFX = true;
	return true;
}

//------------------------------------------------------------------------
void CGameRules::AddEntityEventDoneListener( EntityId id )
{
	CRY_ASSERT(!stl::find(m_entityEventDoneListeners, id));
	m_entityEventDoneListeners.push_back(id);
	gEnv->pEntitySystem->AddEntityEventListener(id, ENTITY_EVENT_DONE, this);
}

//------------------------------------------------------------------------
void CGameRules::RemoveEntityEventDoneListener( EntityId id )
{
	CRY_ASSERT(stl::find(m_entityEventDoneListeners, id));
	stl::find_and_erase(m_entityEventDoneListeners, id);
	gEnv->pEntitySystem->RemoveEntityEventListener(id, ENTITY_EVENT_DONE, this);
}

//------------------------------------------------------------------------
void CGameRules::OnEntityEvent( IEntity *pEntity, SEntityEvent &event )
{
	if (!g_pGame || !g_pGame->GetGameRules())
	{
		return;
	}

	if (event.event == ENTITY_EVENT_DONE)
	{
		if (!gEnv->bServer)
		{
			CryLogAlways("[GameRules] OnEntityEvent ENTITY_EVENT_DONE %d(%s) GameRules %p", pEntity->GetId(), pEntity->GetName(), this);
			//ClDoSetTeam(0, pEntity->GetId());
			GetGameObject()->InvokeRMIWithDependentObject(ClSetTeam(), SetTeamParams(pEntity->GetId(), 0), eRMI_ToRemoteClients, pEntity->GetId());
		}
		else
		{
			SetTeam(0, pEntity->GetId());
		}
	}
}

//------------------------------------------------------------------------
void CGameRules::ClearRemoveEntityEventListeners()
{
	int numEntities = m_entityEventDoneListeners.size();

	for (int i = 0; i < numEntities; ++ i)
	{
		EntityId entityId = m_entityEventDoneListeners[i];
		gEnv->pEntitySystem->RemoveEntityEventListener(entityId, ENTITY_EVENT_DONE, this);
#if !defined(_RELEASE)
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(entityId);
		CryLogAlways("[GameRules] ClearRemoveEntityEventListeners RemoveEntityEventLister(%d(%s), ENTITY_EVENT_DONE, %p)", entityId, pEntity ? pEntity->GetName() : "null", this);
#endif
	}

	m_entityEventDoneListeners.clear();
}

//------------------------------------------------------------------------
bool CGameRules::IsRealActor(EntityId actorId) const
{
	if (g_pGame->GetHostMigrationState() == CGame::eHMS_NotMigrating)
	{
		return true;
	}
	else
	{
		// If we're host migrating, we may have 2 actors for the same person at this point.  Need to make sure we're the real one
		IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(actorId);

		if (pActor)
		{
			IActor *pChannelActor = g_pGame->GetGameRules()->GetActorByChannelId(pActor->GetChannelId());

			if (pChannelActor == pActor)
			{
				return true;
			}
		}

		return false;
	}
}
