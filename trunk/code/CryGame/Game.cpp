/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$

 -------------------------------------------------------------------------
  History:
  - 3:8:2004   11:26 : Created by Marcio Martins
  - 17:8:2005        : Modified - NickH: Factory registration moved to GameFactory.cpp

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"
#include "GameActions.h"

#include "GameRules.h"

#include <ICryPak.h>
#include <CryPath.h>
#include <IActionMapManager.h>
#include <IViewSystem.h>
#include <ILevelSystem.h>
#include <IVehicleSystem.h>
#include <IMovieSystem.h>
#include <IScaleformGFx.h>
#include <IPlatformOS.h>

#include "ScriptBind_Actor.h"
#include "ScriptBind_GameRules.h"
#include "ScriptBind_Game.h"

#include "Camera/CameraManager.h"
#include "GameFactory.h"

#include "ServerSynchedStorage.h"
#include "ClientSynchedStorage.h"

#include "ISaveGame.h"
#include "ILoadGame.h"
#include "CryPath.h"
#include <IPathfinder.h>

#include "IMaterialEffects.h"


#include "Player.h"

#include "GameMechanismManager/GameMechanismManager.h"
#include "ICheckPointSystem.h"

#include "CodeCheckpointDebugMgr.h"


#define GAME_DEBUG_MEM  // debug memory usage
#undef  GAME_DEBUG_MEM

#define SDK_GUID "{CDCB9B7A-7390-45AA-BF2F-3A7C7933DCF3}"

//FIXME: really horrible. Remove ASAP
int OnImpulse( const EventPhys *pEvent )
{
	//return 1;
	return 0;
}

#define GAME_DEBUG_MEM  // debug memory usage
#undef  GAME_DEBUG_MEM

CGame *g_pGame = 0;
SCVars *g_pGameCVars = 0;
CGameActions *g_pGameActions = 0;

CGame::CGame()
	: m_pFramework(0),
	  m_pConsole(0),
	  m_pWeaponSystem(0),
	  m_pScriptBindActor(0),
	  m_pScriptBindGame(0),
	  m_pServerSynchedStorage(0),
	  m_pClientSynchedStorage(0),
	  m_pServerGameTokenSynch(0),
	  m_pClientGameTokenSynch(0),
	  m_clientActorId(-1),
	  m_pRayCaster(0),

	  m_pIntersectionTester(NULL),
	  m_cachedUserRegion(-1),
	  m_hostMigrationState(eHMS_NotMigrating),
	  m_hostMigrationTimeStateChanged(0.f),
	  m_randomGenerator(gEnv->bNoRandomSeed ? 0 : (uint32)gEnv->pTimer->GetAsyncTime().GetValue())
{
	m_pCVars = new SCVars();
	g_pGameCVars = m_pCVars;
	m_pGameActions = new CGameActions();
	g_pGameActions = m_pGameActions;
	g_pGame = this;
	m_bReload = false;
	m_inDevMode = false;
	m_pCameraManager = new CCameraManager();
	m_pGameMechanismManager = new CGameMechanismManager();
	m_pDefaultAM = 0;
	m_pMultiplayerAM = 0;
	GetISystem()->SetIGame( this );
}

CGame::~CGame()
{
	m_pFramework->EndGameContext();
	m_pFramework->UnregisterListener(this);
	gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
	ReleaseScriptBinds();
	//SAFE_DELETE(m_pCameraManager);
	SAFE_DELETE(m_pGameMechanismManager);
	SAFE_DELETE(m_pCVars);
	ClearGameSessionHandler(); // make sure this is cleared before the gamePointer is gone
	g_pGame = 0;
	g_pGameCVars = 0;
	g_pGameActions = 0;
	SAFE_DELETE(m_pRayCaster);
	SAFE_DELETE(m_pGameActions);
	SAFE_DELETE(m_pIntersectionTester);
	gEnv->pGame = 0;
}

bool CGame::Init(IGameFramework *pFramework)
{
	LOADING_TIME_PROFILE_SECTION(GetISystem());
#ifdef GAME_DEBUG_MEM
	DumpMemInfo("CGame::Init start");
#endif
	m_pFramework = pFramework;
	assert(m_pFramework);
	m_pConsole = gEnv->pConsole;
	RegisterConsoleVars();
	RegisterConsoleCommands();
	RegisterGameObjectEvents();
	LoadActionMaps( ACTIONMAP_DEFAULT_PROFILE );
	InitScriptBinds();
	//load user levelnames for ingame text and savegames
	XmlNodeRef lnames = GetISystem()->LoadXmlFromFile(PathUtil::GetGameFolder() + "/Scripts/GameRules/LevelNames.xml");

	if(lnames)
	{
		int num = lnames->getNumAttributes();
		const char *nameA, *nameB;

		for(int n = 0; n < num; ++n)
		{
			lnames->getAttributeByIndex(n, &nameA, &nameB);
			m_mapNames[string(nameA)] = string(nameB);
		}
	}

	// Register all the games factory classes e.g. maps "Player" to CPlayer
	InitGameFactory(m_pFramework);
	gEnv->pConsole->CreateKeyBind("f12", "r_getscreenshot 2");
	// set game GUID
	m_pFramework->SetGameGUID(SDK_GUID);
	gEnv->pSystem->GetPlatformOS()->UserDoSignIn(0); // sign in the default user
	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this);

	// TEMP
	// Load the action map beforehand (see above)
	// afterwards load the user's profile whose action maps get merged with default's action map

	if (!m_pServerSynchedStorage)
	{
		m_pServerSynchedStorage = new CServerSynchedStorage(GetIGameFramework());
	}

	m_pFramework->RegisterListener(this, "Game", eFLPriority_Game);
	
	m_pRayCaster = new GlobalRayCaster;
	m_pRayCaster->SetQuota(6);
	m_pIntersectionTester = new GlobalIntersectionTester;
	m_pIntersectionTester->SetQuota(6);

	if (m_pServerSynchedStorage == NULL)
	{
		m_pServerSynchedStorage = new CServerSynchedStorage(GetIGameFramework());
	}

	if (m_pServerGameTokenSynch == NULL)
	{
		m_pServerGameTokenSynch = new CServerGameTokenSynch(m_pFramework->GetIGameTokenSystem());
	}

#ifdef GAME_DEBUG_MEM
	DumpMemInfo("CGame::Init end");
#endif

	return true;
}

bool CGame::CompleteInit()
{
#ifdef GAME_DEBUG_MEM
	DumpMemInfo("CGame::CompleteInit");
#endif
	return true;
}

void CGame::RegisterGameFlowNodes()
{
}

void CGame::ResetServerGameTokenSynch()
{
	if (m_pServerGameTokenSynch)
	{
		m_pServerGameTokenSynch->~CServerGameTokenSynch();
		new(m_pServerGameTokenSynch) CServerGameTokenSynch(m_pFramework->GetIGameTokenSystem());
	}
	else
	{
		m_pServerGameTokenSynch = new CServerGameTokenSynch(m_pFramework->GetIGameTokenSystem());
	}
}

// Small test for the IPathfinder.h interfaces
/*
extern INavPath *g_testPath;
extern IPathFollower *g_pathFollower;
extern Vec3 g_pos;
extern Vec3 g_vel;
*/

int CGame::Update(bool haveFocus, unsigned int updateFlags)
{
	bool bRun = m_pFramework->PreUpdate( true, updateFlags );
	float frameTime = gEnv->pTimer->GetFrameTime();
	m_pGameMechanismManager->Update(frameTime);
	m_pFramework->PostUpdate( true, updateFlags );
	return bRun ? 1 : 0;
}

void CGame::ConfigureGameChannel(bool isServer, IProtocolBuilder *pBuilder)
{
	if (isServer)
	{
		m_pServerSynchedStorage->DefineProtocol(pBuilder);
		m_pServerGameTokenSynch->DefineProtocol(pBuilder);
	}
	else
	{
		m_pClientSynchedStorage = new CClientSynchedStorage(GetIGameFramework());
		m_pClientSynchedStorage->DefineProtocol(pBuilder);
		m_pClientGameTokenSynch = new CClientGameTokenSynch(m_pFramework->GetIGameTokenSystem());
		m_pClientGameTokenSynch->DefineProtocol(pBuilder);
	}
}

void CGame::EditorResetGame(bool bStart)
{
	CRY_ASSERT(gEnv->IsEditor());

	if(bStart)
	{
		IActionMapManager *pAM = m_pFramework->GetIActionMapManager();

		if (pAM)
		{
			pAM->EnableActionMap(0, true); // enable all action maps
			pAM->EnableFilter(0, false); // disable all filters
		}
	}
}

void CGame::PlayerIdSet(EntityId playerId)
{
	m_clientActorId = playerId;
}

string CGame::InitMapReloading()
{
	string levelFileName = GetIGameFramework()->GetLevelName();
	levelFileName = PathUtil::GetFileName(levelFileName);

	if(const char *visibleName = GetMappedLevelName(levelFileName))
	{
		levelFileName = visibleName;
	}

	levelFileName.append("_cryengine.cryenginejmsf");

#ifndef WIN32
	m_bReload = true; //using map command
#else
	m_bReload = false;
	levelFileName.clear();
#endif
	return levelFileName;
}

void CGame::Shutdown()
{
	SAFE_DELETE(m_pServerSynchedStorage);
	SAFE_DELETE(m_pServerGameTokenSynch);
	this->~CGame();
}

const char *CGame::GetLongName()
{
	return GAME_LONGNAME;
}

const char *CGame::GetName()
{
	return GAME_NAME;
}

void CGame::OnPostUpdate(float fDeltaTime)
{
	//update camera system
	//m_pCameraManager->Update();
}

void CGame::OnSaveGame(ISaveGame *pSaveGame)
{
	ScopedSwitchToGlobalHeap useGlobalHeap;
	IActor		*pActor = GetIGameFramework()->GetClientActor();
	CPlayer	*pPlayer = static_cast<CPlayer *>(pActor);
	GetGameRules()->PlayerPosForRespawn(pPlayer, true);

	//save difficulty
	pSaveGame->AddMetadata("sp_difficulty", g_pGameCVars->g_difficultyLevel);
	pSaveGame->AddMetadata("v_altitudeLimit", g_pGameCVars->pAltitudeLimitCVar->GetString());
}

void CGame::OnLoadGame(ILoadGame *pLoadGame)
{
	int difficulty = g_pGameCVars->g_difficultyLevel;
	pLoadGame->GetMetadata("sp_difficulty", difficulty);

	// altitude limit
	const char *v_altitudeLimit =	pLoadGame->GetMetadata("v_altitudeLimit");

	if (v_altitudeLimit && *v_altitudeLimit)
	{
		g_pGameCVars->pAltitudeLimitCVar->ForceSet(v_altitudeLimit);
	}
	else
	{
		CryFixedStringT<128> buf;
		buf.FormatFast("%g", g_pGameCVars->v_altitudeLimitDefault());
		g_pGameCVars->pAltitudeLimitCVar->ForceSet(buf.c_str());
	}
}

void CGame::OnActionEvent(const SActionEvent &event)
{
	switch(event.m_event)
	{
		case  eAE_channelDestroyed:
			GameChannelDestroyed(event.m_value == 1);
			break;

		case eAE_serverIp:
			if(gEnv->bServer && GetServerSynchedStorage())
			{
				GetServerSynchedStorage()->SetGlobalValue(GLOBAL_SERVER_IP_KEY, string(event.m_description));
				GetServerSynchedStorage()->SetGlobalValue(GLOBAL_SERVER_PUBLIC_PORT_KEY, event.m_value);
			}

			break;

		case eAE_serverName:
			if(gEnv->bServer && GetServerSynchedStorage())
			{
				GetServerSynchedStorage()->SetGlobalValue(GLOBAL_SERVER_NAME_KEY, string(event.m_description));
			}

			break;

		case eAE_unloadLevel:
			m_clientActorId = 0;
			break;
	}
}


void CGame::GameChannelDestroyed(bool isServer)
{
	if (!isServer)
	{
		SAFE_DELETE(m_pClientSynchedStorage);
		SAFE_DELETE(m_pClientGameTokenSynch);

		if (!gEnv->pSystem->IsSerializingFile())
		{
			CryFixedStringT<128> buf;
			buf.FormatFast("%g", g_pGameCVars->v_altitudeLimitDefault());
			g_pGameCVars->pAltitudeLimitCVar->ForceSet(buf.c_str());
		}
	}
}


void CGame::BlockingProcess(BlockingConditionFunction f)
{
	INetwork *pNetwork = gEnv->pNetwork;

	if (!pNetwork)
	{
		return;
	}

	bool ok = false;
	ITimer *pTimer = gEnv->pTimer;
	CTimeValue startTime = pTimer->GetAsyncTime();

	while (!ok)
	{
		pNetwork->SyncWithGame(eNGS_FrameStart);
		pNetwork->SyncWithGame(eNGS_FrameEnd);
		gEnv->pTimer->UpdateOnFrameStart();
		ok = (*f)();
	}
}

uint32 CGame::AddGameWarning(const char *stringId, const char *paramMessage, IGameWarningsListener *pListener)
{
	CryLogAlways("GameWarning trying to display: %s", stringId);
	return 0;
}

void CGame::RemoveGameWarning(const char *stringId)
{
}







const static uint8 drmKeyData[16] = {0};
const static char *drmFiles = NULL;


const uint8 *CGame::GetDRMKey()
{
	return drmKeyData;
}

const char *CGame::GetDRMFileList()
{
	return drmFiles;
}

CGameRules *CGame::GetGameRules() const
{
	return static_cast<CGameRules *>(m_pFramework->GetIGameRulesSystem()->GetCurrentGameRules());
}

void CGame::LoadActionMaps(const char *filename)
{
	CRY_ASSERT_MESSAGE((filename || *filename != 0), "filename is empty!");

	if(g_pGame->GetIGameFramework()->IsGameStarted())
	{
		CryLogAlways("Can't change configuration while game is running (yet)");
		return;
	}

	IActionMapManager *pActionMapMan = m_pFramework->GetIActionMapManager();
	pActionMapMan->AddInputDeviceMapping(eAID_KeyboardMouse, "keyboard");
	pActionMapMan->AddInputDeviceMapping(eAID_XboxPad, "xboxpad");
	pActionMapMan->AddInputDeviceMapping(eAID_PS3Pad, "ps3pad");
	pActionMapMan->AddInputDeviceMapping(eAID_WiiPad, "wiipad");
	// make sure that they are also added to the GameActions.actions file!
	XmlNodeRef rootNode = m_pFramework->GetISystem()->LoadXmlFromFile(filename);

	if(rootNode)
	{
		pActionMapMan->Clear();
		pActionMapMan->LoadFromXML(rootNode);
		m_pDefaultAM = pActionMapMan->GetActionMap("default");
		m_pDebugAM = pActionMapMan->GetActionMap("debug");
		m_pMultiplayerAM = pActionMapMan->GetActionMap("multiplayer");
		// enable defaults
		pActionMapMan->EnableActionMap("default", true);
		// enable debug
		pActionMapMan->EnableActionMap("debug", gEnv->pSystem->IsDevMode());
		// enable player action map
		pActionMapMan->EnableActionMap("player", true);
	}
	else
	{
		CryLogAlways("[game] error: Could not open configuration file %s", filename);
		CryLogAlways("[game] error: this will probably cause an infinite loop later while loading a map");
	}

	m_pGameActions->Init();
}

void CGame::InitScriptBinds()
{
	m_pScriptBindActor = new CScriptBind_Actor(m_pFramework->GetISystem());
	m_pScriptBindGameRules = new CScriptBind_GameRules(m_pFramework->GetISystem(), m_pFramework);
	m_pScriptBindGame = new CScriptBind_Game(m_pFramework->GetISystem(), m_pFramework);
}

void CGame::ReleaseScriptBinds()
{
	SAFE_DELETE(m_pScriptBindActor);
	SAFE_DELETE(m_pScriptBindGameRules);
	SAFE_DELETE(m_pScriptBindGame);
}

void CGame::CheckReloadLevel()
{
	if(!m_bReload)
	{
		return;
	}

	if(gEnv->IsEditor() || gEnv->bMultiplayer)
	{
		if(m_bReload)
		{
			m_bReload = false;
		}

		return;
	}

#ifdef WIN32
	// Restart interrupts cutscenes
	gEnv->pMovieSystem->StopAllCutScenes();
	GetISystem()->SerializingFile(1);
	//load levelstart
	ILevelSystem *pLevelSystem = m_pFramework->GetILevelSystem();
	ILevel			*pLevel = pLevelSystem->GetCurrentLevel();
	ILevelInfo *pLevelInfo = pLevelSystem->GetLevelInfo(m_pFramework->GetLevelName());
	//**********
	EntityId playerID = GetIGameFramework()->GetClientActorId();
	pLevelSystem->OnLoadingStart(pLevelInfo);
	PlayerIdSet(playerID);
	string levelstart(GetIGameFramework()->GetLevelName());

	if(const char *visibleName = GetMappedLevelName(levelstart))
	{
		levelstart = visibleName;
	}

	levelstart.append("_cryengine.cryenginejmsf");
	GetIGameFramework()->LoadGame(levelstart.c_str(), true, true);
	//**********
	pLevelSystem->OnLoadingComplete(pLevel);
	m_bReload = false;	//if m_bReload is true - load at levelstart
	//if paused - start game
	m_pFramework->PauseGame(false, true);
	GetISystem()->SerializingFile(0);
#else
	string command("map ");
	command.append(m_pFramework->GetLevelName());
	gEnv->pConsole->ExecuteString(command);
#endif
}

void CGame::RegisterGameObjectEvents()
{
	IGameObjectSystem *pGOS = m_pFramework->GetIGameObjectSystem();
	pGOS->RegisterEvent(eCGE_PostFreeze, "PostFreeze");
	pGOS->RegisterEvent(eCGE_PostShatter, "PostShatter");
	pGOS->RegisterEvent(eCGE_OnShoot, "OnShoot");
	pGOS->RegisterEvent(eCGE_Recoil, "Recoil");
	pGOS->RegisterEvent(eCGE_BeginReloadLoop, "BeginReloadLoop");
	pGOS->RegisterEvent(eCGE_EndReloadLoop, "EndReloadLoop");
	pGOS->RegisterEvent(eCGE_ActorRevive, "ActorRevive");
	pGOS->RegisterEvent(eCGE_VehicleDestroyed, "VehicleDestroyed");
	pGOS->RegisterEvent(eCGE_TurnRagdoll, "TurnRagdoll");
	pGOS->RegisterEvent(eCGE_EnableFallAndPlay, "EnableFallAndPlay");
	pGOS->RegisterEvent(eCGE_DisableFallAndPlay, "DisableFallAndPlay");
	pGOS->RegisterEvent(eCGE_VehicleTransitionEnter, "VehicleTransitionEnter");
	pGOS->RegisterEvent(eCGE_VehicleTransitionExit, "VehicleTransitionExit");
	pGOS->RegisterEvent(eCGE_TextArea, "TextArea");
	pGOS->RegisterEvent(eCGE_InitiateAutoDestruction, "InitiateAutoDestruction");
	pGOS->RegisterEvent(eCGE_Event_Collapsing, "Event_Collapsing");
	pGOS->RegisterEvent(eCGE_Event_Collapsed, "Event_Collapsed");
	pGOS->RegisterEvent(eCGE_MultiplayerChatMessage, "MultiplayerChatMessage");
	pGOS->RegisterEvent(eCGE_ResetMovementController, "ResetMovementController");
	pGOS->RegisterEvent(eCGE_AnimateHands, "AnimateHands");
	pGOS->RegisterEvent(eCGE_Ragdoll, "Ragdoll");
	pGOS->RegisterEvent(eCGE_EnablePhysicalCollider, "EnablePhysicalCollider");
	pGOS->RegisterEvent(eCGE_DisablePhysicalCollider, "DisablePhysicalCollider");
	pGOS->RegisterEvent(eCGE_RebindAnimGraphInputs, "RebindAnimGraphInputs");
	pGOS->RegisterEvent(eCGE_OpenParachute, "OpenParachute");
	pGOS->RegisterEvent(eCGE_ReactionEnd, "ReactionEnd");
}

void CGame::GetMemoryStatistics(ICrySizer *s) const
{
	s->Add(*this);
	s->Add(*m_pScriptBindActor);
	s->Add(*m_pScriptBindGameRules);
	s->Add(*m_pScriptBindGame);
	s->Add(*m_pGameActions);

	if (m_pServerSynchedStorage)
	{
		m_pServerSynchedStorage->GetMemoryUsage(s);
	}

	if (m_pClientSynchedStorage)
	{
		m_pClientSynchedStorage->GetMemoryUsage(s);
	}

	if (m_pServerGameTokenSynch)
	{
		m_pServerGameTokenSynch->GetMemoryUsage(s);
	}

	if (m_pClientGameTokenSynch)
	{
		m_pClientGameTokenSynch->GetMemoryUsage(s);
	}
}

void CGame::OnClearPlayerIds()
{
}

void CGame::DumpMemInfo(const char *format, ...)
{
	CryModuleMemoryInfo memInfo;
	CryGetMemoryInfoForModule(&memInfo);
	va_list args;
	va_start(args, format);
	gEnv->pLog->LogV( ILog::eAlways, format, args );
	va_end(args);
	gEnv->pLog->LogWithType( ILog::eAlways, "Alloc=%I64d kb  String=%I64d kb  STL-alloc=%I64d kb  STL-wasted=%I64d kb", (memInfo.allocated - memInfo.freed) >> 10 , memInfo.CryString_allocated >> 10, memInfo.STL_allocated >> 10 , memInfo.STL_wasted >> 10);
	// gEnv->pLog->LogV( ILog::eAlways, "%s alloc=%llu kb  instring=%llu kb  stl-alloc=%llu kb  stl-wasted=%llu kb", text, memInfo.allocated >> 10 , memInfo.CryString_allocated >> 10, memInfo.STL_allocated >> 10 , memInfo.STL_wasted >> 10);
}


const string &CGame::GetLastSaveGame(string &levelName)
{
	return m_lastSaveGame;
}

bool RespawnIfDead(CActor *pActor)
{
	if(pActor && pActor->IsDead())
	{
		pActor->StandUp();
		pActor->Revive();
		pActor->SetHealth(pActor->GetMaxHealth());
		pActor->HolsterItem(true);
		pActor->HolsterItem(false);

		if( IEntity *pEntity = pActor->GetEntity() )
		{
			pEntity->GetAI()->Event(AIEVENT_ENABLE, NULL);
		}

		return true;
	}

	return false;
}


bool CGame::LoadLastSave()
{
	if (gEnv->bMultiplayer)
	{
		return false;
	}

	bool bLoadSave = true;

	if (gEnv->IsEditor())
	{
		ICVar *pAllowSaveLoadInEditor = gEnv->pConsole->GetCVar("g_allowSaveLoadInEditor");

		if (pAllowSaveLoadInEditor)
		{
			bLoadSave = (pAllowSaveLoadInEditor->GetIVal() != 0);
		}
		else
		{
			bLoadSave = false;
		}

		if (!bLoadSave) // Wont go through normal path which reloads hud, reload here
		{
			g_pGame->PostSerialize();
		}
	}

	bool bSuccess = true;

	if (bLoadSave)
	{
		if(g_pGameCVars->g_enableSlimCheckpoints)
		{
			bSuccess = GetIGameFramework()->GetICheckpointSystem()->LoadLastCheckpoint();
		}
		else
		{
			const string &file = g_pGame->GetLastSaveGame();

			if(file.length())
			{
				if(!g_pGame->GetIGameFramework()->LoadGame(file.c_str(), true))
				{
					bSuccess = g_pGame->GetIGameFramework()->LoadGame(file.c_str(), false);
				}
			}
		}
	}
	else
	{
		CActor *pPlayer = static_cast<CActor *>(GetIGameFramework()->GetClientActor());
		bSuccess = RespawnIfDead( pPlayer );
	}

	return bSuccess;
}


void CGame::PostSerialize()
{
}


ILINE void expandSeconds(int secs, int &days, int &hours, int &minutes, int &seconds)
{
	days  = secs / 86400;
	secs -= days * 86400;
	hours = secs / 3600;
	secs -= hours * 3600;
	minutes = secs / 60;
	seconds = secs - minutes * 60;
	hours += days * 24;
	days = 0;
}

void secondsToString(int secs, string &outString)
{
	int d, h, m, s;
	expandSeconds(secs, d, h, m, s);

	if (h > 0)
	{
		outString.Format("%02dh_%02dm_%02ds", h, m, s);
	}
	else
	{
		outString.Format("%02dm_%02ds", m, s);
	}
}

IGame::TSaveGameName CGame::CreateSaveGameName()
{
	//design wants to have different, more readable names for the savegames generated
	int id = 0;
	TSaveGameName saveGameName;

	saveGameName = CRY_SAVEGAME_FILENAME;
	char buffer[16];
	itoa(id, buffer, 10);
	saveGameName.clear();

	if(id < 10)
	{
		saveGameName += "0";
	}

	saveGameName += buffer;
	saveGameName += "_";
	const char *levelName = GetIGameFramework()->GetLevelName();
	const char *mappedName = GetMappedLevelName(levelName);
	saveGameName += mappedName;
	saveGameName += "_cryengine";
	saveGameName += CRY_SAVEGAME_FILE_EXT;
	return saveGameName;
}

const char *CGame::GetMappedLevelName(const char *levelName) const
{
	TLevelMapMap::const_iterator iter = m_mapNames.find(CONST_TEMP_STRING(levelName));
	return (iter == m_mapNames.end()) ? levelName : iter->second.c_str();
}


IGameStateRecorder *CGame::CreateGameStateRecorder(IGameplayListener *pL)
{
	return nullptr;
}

void CGame::OnSystemEvent( ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam )
{
	switch (event)
	{
		case ESYSTEM_EVENT_LEVEL_LOAD_START:
			{
				if (!m_pRayCaster)
				{
					m_pRayCaster = new GlobalRayCaster;
					m_pRayCaster->SetQuota(6);
				}

				if (!m_pIntersectionTester)
				{
					m_pIntersectionTester = new GlobalIntersectionTester;
					m_pIntersectionTester->SetQuota(6);
				}
			}
			break;

		case ESYSTEM_EVENT_LEVEL_UNLOAD:
			{
				SAFE_DELETE(m_pRayCaster);
				SAFE_DELETE(m_pIntersectionTester);
			}
			break;
	}
}

bool CGame::IsGameActive() const
{
	assert(g_pGame);
	IGameFramework *pGameFramework = g_pGame->GetIGameFramework();
	assert(pGameFramework);
	return (pGameFramework->StartingGameContext() || pGameFramework->StartedGameContext()) && (IsGameSessionHostMigrating() || pGameFramework->GetClientChannel());
}

void CGame::ClearGameSessionHandler()
{
	GetIGameFramework()->SetGameSessionHandler(NULL);
}

uint32 CGame::GetRandomNumber()
{
	return m_randomGenerator.Generate();
}

void CGame::VerifyMaxPlayers(ICVar *pVar)
{
	int nPlayers = pVar->GetIVal();

	if (nPlayers < 2)
	{
		pVar->Set(2);
	}
}

//------------------------------------------------------------------------
float CGame::GetTimeSinceHostMigrationStateChanged() const
{
	const float curTime = gEnv->pTimer->GetAsyncCurTime();
	const float timePassed = curTime - m_hostMigrationTimeStateChanged;
	return timePassed;
}

//------------------------------------------------------------------------
float CGame::GetRemainingHostMigrationTimeoutTime() const
{
	const float timePassed = GetTimeSinceHostMigrationStateChanged();
	const float timeRemaining = m_hostMigrationNetTimeoutLength - timePassed;
	return MAX(timeRemaining, 0.f);
}

//------------------------------------------------------------------------
float CGame::GetHostMigrationTimeTillResume() const
{
	float timeRemaining = 0.f;

	if (m_hostMigrationState == eHMS_WaitingForPlayers)
	{
		timeRemaining = GetRemainingHostMigrationTimeoutTime() + g_pGameCVars->g_hostMigrationResumeTime;
	}
	else if (m_hostMigrationState == eHMS_Resuming)
	{
		const float curTime = gEnv->pTimer->GetAsyncCurTime();
		const float timePassed = curTime - m_hostMigrationTimeStateChanged;
		timeRemaining = MAX(g_pGameCVars->g_hostMigrationResumeTime - timePassed, 0.f);
	}

	return timeRemaining;
}

//------------------------------------------------------------------------
void CGame::SetHostMigrationState(EHostMigrationState newState)
{
	float timeOfChange = gEnv->pTimer->GetAsyncCurTime();
	SetHostMigrationStateAndTime(newState, timeOfChange);
}

//------------------------------------------------------------------------
void CGame::SetHostMigrationStateAndTime( EHostMigrationState newState, float timeOfChange )
{
	CryLog("CGame::SetHostMigrationState() state changing to '%i' (from '%i')", int(newState), int(m_hostMigrationState));

	if ((m_hostMigrationState == eHMS_NotMigrating) && (newState != eHMS_NotMigrating))
	{
		m_pFramework->PauseGame(true, false);
		ICVar *pTimeoutCVar = gEnv->pConsole->GetCVar("net_migrate_timeout");
		m_hostMigrationNetTimeoutLength = pTimeoutCVar->GetFVal();
		pTimeoutCVar->SetOnChangeCallback(OnHostMigrationNetTimeoutChanged);
	}

	m_hostMigrationState = newState;
	m_hostMigrationTimeStateChanged = timeOfChange;

	if (newState == eHMS_WaitingForPlayers)
	{
		// todo: show UI host migration
	}
	else if (newState == eHMS_Resuming)
	{
		// todo: hide UI host migration
	}
	else if (newState == eHMS_NotMigrating)
	{
		AbortHostMigration();
	}

	// Notify the gamerules
	CGameRules *pGameRules = GetGameRules();
	pGameRules->OnHostMigrationStateChanged();
}

//------------------------------------------------------------------------
void CGame::AbortHostMigration()
{
	m_pFramework->PauseGame(false, false);
	m_hostMigrationState = eHMS_NotMigrating;
	m_hostMigrationTimeStateChanged = 0.f;
	ICVar *pTimeoutCVar = gEnv->pConsole->GetCVar("net_migrate_timeout");
	pTimeoutCVar->SetOnChangeCallback(NULL);
}

//------------------------------------------------------------------------
void CGame::OnHostMigrationNetTimeoutChanged(ICVar *pVar)
{
	g_pGame->m_hostMigrationNetTimeoutLength = pVar->GetFVal();
}

#include UNIQUE_VIRTUAL_WRAPPER(IGame)

