/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$

 -------------------------------------------------------------------------
  History:
  - 30:8:2004   11:19 : Created by Marcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "EditorGame.h"
#include "GameStartup.h"
#include "IActionMapManager.h"
#include "IActorSystem.h"
#include "IGameRulesSystem.h"
#include "INetwork.h"
#include "IAgent.h"
#include "ILevelSystem.h"
#include "IMovementController.h"
#include "IItemSystem.h"
#include "ItemParamReader.h"
#include "EquipmentSystemInterface.h"
#include "GameCVars.h"
#include "Actor.h"
#include "GameRules.h"

#define EDITOR_SERVER_PORT 0xed17

ICVar *CEditorGame::s_pEditorGameMode;
CEditorGame *CEditorGame::s_pEditorGame = NULL;
struct IGameStartup;

//------------------------------------------------------------------------
CEditorGame::CEditorGame()
	: m_pGame(0),
	  m_pGameStartup(0),
	  m_pEquipmentSystemInterface(0)
{
	m_bEnabled = false;
	m_bGameMode = false;
	m_bPlayer = false;
	s_pEditorGame = this;
	s_pEditorGameMode = NULL;
}

void CEditorGame::ResetClient(IConsoleCmdArgs *)
{
	bool value = s_pEditorGame->m_bPlayer;
	s_pEditorGame->EnablePlayer(false);
	IEntityClass *pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Player");

	if (pClass)
	{
		pClass->LoadScript(true);
	}

	if (value)
	{
		s_pEditorGame->ConfigureNetContext(true);
		SetGameRules();
	}

	s_pEditorGame->EnablePlayer(value);
	s_pEditorGame->HidePlayer(true);
}

//------------------------------------------------------------------------
void CEditorGame::ToggleMultiplayerGameRules()
{
	m_bUsingMultiplayerGameRules = !m_bUsingMultiplayerGameRules;
	bool value = s_pEditorGame->m_bPlayer;
	s_pEditorGame->EnablePlayer(false);
	s_pEditorGame->ConfigureNetContext(true);
	IEntityClass *pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Player");

	if (pClass)
	{
		pClass->LoadScript(true);
	}

	SetGameRules();
	s_pEditorGame->EnablePlayer(value);
	s_pEditorGame->HidePlayer(true);
	gEnv->pConsole->ShowConsole(false);
}

//------------------------------------------------------------------------
CEditorGame::~CEditorGame()
{
	SAFE_DELETE(m_pEquipmentSystemInterface);
	s_pEditorGame = NULL;
	SAFE_RELEASE(s_pEditorGameMode);
}

//------------------------------------------------------------------------
void CEditorGame::OnChangeEditorMode( ICVar *pVar )
{
	assert( pVar == s_pEditorGameMode );

	if (s_pEditorGame)
	{
		s_pEditorGame->SetGameMode( s_pEditorGame->m_bGameMode );
	}
}

//------------------------------------------------------------------------

extern "C"
{
	GAME_API IGameStartup *CreateGameStartup();
};

bool CEditorGame::Init(ISystem *pSystem, IGameToEditorInterface *pGameToEditorInterface)
{
	assert(pSystem);
	SSystemInitParams startupParams;
	startupParams.bEditor = true;
	startupParams.pSystem = pSystem;
	startupParams.bExecuteCommandLine = false;		// in editor we do it later - after other things are initialized
	m_pGameStartup = CreateGameStartup();
	m_pGame = m_pGameStartup->Init(startupParams);

	if (!m_pGame)
	{
		return false;
	}

	IGameFramework *pGameFramework = m_pGame->GetIGameFramework();

	if ( pGameFramework != NULL )
	{
		pGameFramework->InitEditor( pGameToEditorInterface );
	}

	InitUIEnums(pGameToEditorInterface);
	m_pEquipmentSystemInterface = new CEquipmentSystemInterface(this, pGameToEditorInterface);
	gEnv->bServer = true;
	gEnv->bMultiplayer = false;
#if !defined(XENON) && !defined(PS3) && !defined(GRINGO)
	gEnv->SetIsClient(true);
#endif
	m_bUsingMultiplayerGameRules = false;
	s_pEditorGameMode = REGISTER_INT( "net_gamemode", 0, 0, "Should editor connect a new client?");
	s_pEditorGameMode->SetOnChangeCallback(&OnChangeEditorMode);
	SetGameMode(false);
	REGISTER_COMMAND( "net_reseteditorclient", ResetClient, 0, "Resets player and gamerules!" );
	ConfigureNetContext(true);
	return true;
}

//------------------------------------------------------------------------
int CEditorGame::Update(bool haveFocus, unsigned int updateFlags)
{
	return m_pGameStartup->Update(haveFocus, updateFlags);
}

//------------------------------------------------------------------------
void CEditorGame::Shutdown()
{
	gEnv->pConsole->RemoveCommand("net_reseteditorclient");
	EnablePlayer(false);
	SetGameMode(false);
	m_pGameStartup->Shutdown();
}

//------------------------------------------------------------------------
void CEditorGame::EnablePlayer(bool bPlayer)
{
	bool spawnPlayer = false;

	if (m_bPlayer != bPlayer)
	{
		spawnPlayer = m_bPlayer = bPlayer;
	}

	if (!SetGameMode( m_bGameMode ))
	{
		GameWarning("Failed setting game mode");
	}
	else if (m_bEnabled && spawnPlayer)
	{
		if (!m_pGame->GetIGameFramework()->BlockingSpawnPlayer())
		{
			GameWarning("Failed spawning player");
		}
	}
}

//------------------------------------------------------------------------
bool CEditorGame::SetGameMode(bool bGameMode)
{
	m_bGameMode = bGameMode;
	bool on = bGameMode;

	if (s_pEditorGameMode->GetIVal() == 0)
	{
		on = m_bPlayer;
	}

	bool ok = ConfigureNetContext( on );

	if (ok)
	{
		if(gEnv->IsEditor())
		{
			m_pGame->EditorResetGame(bGameMode);
		}

		IGameFramework *pGameFramework = m_pGame->GetIGameFramework();
		pGameFramework->OnEditorSetGameMode(bGameMode);
		CActor *pActor = static_cast<CActor *>(m_pGame->GetIGameFramework()->GetClientActor());

		if (pActor)
		{
			if (bGameMode)
			{
				// Revive actor in its current location (it will be moved to the editor viewpoint location later)
				const Vec3 pos = pActor->GetEntity()->GetWorldPos();
				const Quat rot = pActor->GetEntity()->GetWorldRotation();
				const int teamId = g_pGame->GetGameRules()->GetTeam(pActor->GetEntityId());
				pActor->NetReviveAt(pos, rot, teamId);
			}
			else
			{
				pActor->Reset(true);
			}
		}
	}
	else
	{
		GameWarning("Failed configuring net context");
	}

	return ok;
}

//------------------------------------------------------------------------
IEntity *CEditorGame::GetPlayer()
{
	IGameFramework *pGameFramework = m_pGame->GetIGameFramework();

	if(!m_pGame)
	{
		return 0;
	}

	IActor *pActor = pGameFramework->GetClientActor();
	return pActor ? pActor->GetEntity() : NULL;
}

//------------------------------------------------------------------------
void CEditorGame::SetPlayerPosAng(Vec3 pos, Vec3 viewDir)
{
	IActor *pClActor = m_pGame->GetIGameFramework()->GetClientActor();

	if (pClActor)
	{
		// pos coming from editor is a camera position, we must convert it into the player position by subtructing eye height.
		IEntity *myPlayer = pClActor->GetEntity();

		if (myPlayer)
		{
			pe_player_dimensions dim;
			dim.heightEye = 0;

			if (myPlayer->GetPhysics())
			{
				myPlayer->GetPhysics()->GetParams( &dim );
				pos.z = pos.z - dim.heightEye;
			}
		}

		pClActor->GetEntity()->SetPosRotScale( pos, Quat::CreateRotationVDir(viewDir), Vec3(1, 1, 1), ENTITY_XFORM_EDITOR );
	}
}

//------------------------------------------------------------------------
void CEditorGame::HidePlayer(bool bHide)
{
	IEntity *pEntity = GetPlayer();

	if (pEntity)
	{
		pEntity->Hide( bHide );
	}
}

//------------------------------------------------------------------------
bool CEditorGame::ConfigureNetContext( bool on )
{
	bool ok = false;
	IGameFramework *pGameFramework = m_pGame->GetIGameFramework();

	if (on == m_bEnabled)
	{
		ok = true;
	}
	else if (on)
	{
		CryLogAlways( "EDITOR: Set game mode: on" );
		SGameContextParams ctx;
		SGameStartParams gameParams;
		gameParams.flags = eGSF_Server
						   | eGSF_NoSpawnPlayer
						   | eGSF_Client
						   | eGSF_NoLevelLoading
						   | eGSF_BlockingClientConnect
						   | eGSF_NoGameRules
						   | eGSF_NoQueries;

		if (!m_bUsingMultiplayerGameRules)
		{
			gameParams.flags |= eGSF_LocalOnly;
		}

		gameParams.connectionString = "";
		gameParams.hostname = "localhost";
		gameParams.port = EDITOR_SERVER_PORT;
		gameParams.pContextParams = &ctx;
		gameParams.maxPlayers = 1;

		if (pGameFramework->StartGameContext( &gameParams ))
		{
			ok = true;
		}
	}
	else
	{
		CryLogAlways( "EDITOR: Set game mode: off" );
		pGameFramework->EndGameContext();
		gEnv->pNetwork->SyncWithGame(eNGS_Shutdown);
		ok = true;
	}

	m_bEnabled = on && ok;
	return ok;
}

//------------------------------------------------------------------------
void CEditorGame::OnBeforeLevelLoad()
{
	EnablePlayer(false);
	ConfigureNetContext(true);
	SetGameRules();
	m_pGame->GetIGameFramework()->GetILevelSystem()->OnLoadingStart(0);
}

//------------------------------------------------------------------------
void CEditorGame::OnAfterLevelLoad(const char *levelName, const char *levelFolder)
{
	m_pGame->GetIGameFramework()->SetEditorLevel(levelName, levelFolder);
	ILevel *pLevel = m_pGame->GetIGameFramework()->GetILevelSystem()->SetEditorLoadedLevel(levelName);
	m_pGame->GetIGameFramework()->GetILevelSystem()->OnLoadingComplete(pLevel);
	EnablePlayer(true);
}

//------------------------------------------------------------------------
IFlowSystem *CEditorGame::GetIFlowSystem()
{
	return m_pGame->GetIGameFramework()->GetIFlowSystem();
}

//------------------------------------------------------------------------
IGameTokenSystem *CEditorGame::GetIGameTokenSystem()
{
	return m_pGame->GetIGameFramework()->GetIGameTokenSystem();
}

void CEditorGame::RegisterTelemetryTimelineRenderers(Telemetry::ITelemetryRepository *pRepository)
{
	// pRepository->RegisterTimelineRenderer("shot", RenderShot);
}

//------------------------------------------------------------------------
void CEditorGame::InitUIEnums(IGameToEditorInterface *pGTE)
{
	InitGlobalFileEnums(pGTE);
	InitActionEnums(pGTE);
	InitActionInputEnums(pGTE);
	InitActionMapsEnums(pGTE);
}

void CEditorGame::InitGlobalFileEnums(IGameToEditorInterface *pGTE)
{
	// Read in enums stored offline XML. Format is
	// <GlobalEnums>
	//   <EnumName>
	//     <entry enum="someName=someValue" />  <!-- displayed name != value -->
	// 	   <entry enum="someNameValue" />       <!-- displayed name == value -->
	//   </EnumName>
	// </GlobalEnums>
	//
	XmlNodeRef rootNode = GetISystem()->LoadXmlFromFile("Libs/GlobalEnums.xml");

	if (!rootNode || !rootNode->getTag() || stricmp(rootNode->getTag(), "GlobalEnums") != 0)
	{
		// GameWarning("CEditorGame::InitUIEnums: File 'Libs/GlobalEnums.xml' is not a GlobalEnums file");
		return;
	}

	for (int i = 0; i < rootNode->getChildCount(); ++i)
	{
		XmlNodeRef enumNameNode = rootNode->getChild(i);
		const char *enumId = enumNameNode->getTag();

		if (enumId == 0 || *enumId == '\0')
		{
			continue;
		}

		int maxChilds = enumNameNode->getChildCount();

		if (maxChilds > 0)
		{
			// allocate enough space to hold all strings
			const char **nameValueStrings = new const char*[maxChilds];
			int curEntryIndex = 0;

			for (int j = 0; j < maxChilds; ++j)
			{
				XmlNodeRef enumNode = enumNameNode->getChild(j);
				const char *nameValue = enumNode->getAttr("enum");

				if (nameValue != 0 && *nameValue != '\0')
				{
					// put in the nameValue pair
					nameValueStrings[curEntryIndex++] = nameValue;
				}
			}

			// if we found some entries inform CUIDataBase about it
			if (curEntryIndex > 0)
			{
				pGTE->SetUIEnums(enumId, nameValueStrings, curEntryIndex);
			}

			// be nice and free our array
			delete[] nameValueStrings;
		}
	}
}

void CEditorGame::InitActionEnums(IGameToEditorInterface *pGTE)
{
	// init ActionFilter enums
	IActionMapManager *pActionMapMgr = m_pGame->GetIGameFramework()->GetIActionMapManager();

	if (pActionMapMgr)
	{
		std::vector<string> filterNames;
		filterNames.push_back(""); // empty
		IActionFilterIteratorPtr pFilterIter = pActionMapMgr->CreateActionFilterIterator();

		while (IActionFilter *pFilter = pFilterIter->Next())
		{
			filterNames.push_back(pFilter->GetName());
		}

		size_t numFilters = 0;
		const char **allFilters = new const char*[filterNames.size()];
		std::vector<string>::const_iterator iter = filterNames.begin();
		std::vector<string>::const_iterator iterEnd = filterNames.end();

		while (iter != iterEnd)
		{
			allFilters[numFilters++] = iter->c_str();
			++iter;
		}

		pGTE->SetUIEnums("action_filter", allFilters, numFilters);
		delete[] allFilters;
	}
}

void CEditorGame::InitActionMapsEnums(IGameToEditorInterface *pGTE)
{
	IActionMapManager *pAM = m_pGame->GetIGameFramework()->GetIActionMapManager();
	IActionMapIteratorPtr iter = pAM->CreateActionMapIterator();
	const int numActionMaps = pAM->GetActionMapsCount();

	if(numActionMaps == 0)
	{
		return;
	}

	const char **nameValueStrings = new const char*[numActionMaps];
	int curEntryIndex = 0;

	while (IActionMap *pMap = iter->Next())
	{
		nameValueStrings[curEntryIndex++] = pMap->GetName();

		if (curEntryIndex > numActionMaps)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "[InitActionMapsEnums] Wrong number of Action Maps.");
			break;
		}
	}

	pGTE->SetUIEnums("action_maps", nameValueStrings, numActionMaps);
	delete[] nameValueStrings;
}

void CEditorGame::InitActionInputEnums( IGameToEditorInterface *pGTE )
{
	CRY_ASSERT(pGTE);
	IActionMapManager *pActionMapManager = g_pGame->GetIGameFramework()->GetIActionMapManager();

	if (pActionMapManager)
	{
		struct SActionList : public IActionMapPopulateCallBack
		{
			explicit SActionList(int actionCount)
				: m_maxNumNames(actionCount)
				, m_nameCount(0)
			{
				m_allActionNames = new const char*[actionCount];
			}

			~SActionList()
			{
				SAFE_DELETE_ARRAY(m_allActionNames);
			}

			//IActionMapPopulateCallBack
			virtual void AddActionName( const char *const pName )
			{
				assert(m_nameCount < m_maxNumNames);

				if (m_nameCount < m_maxNumNames)
				{
					m_allActionNames[m_nameCount] = pName;
					m_nameCount++;
				}
			}
			//~IActionMapPopulateCallBack

			const char **m_allActionNames;
			int m_maxNumNames;
			int m_nameCount;
		};
		int actionCount = pActionMapManager->GetActionsCount();

		if (actionCount > 0)
		{
			SActionList actionList(actionCount);
			pActionMapManager->EnumerateActions(&actionList);
			pGTE->SetUIEnums("input_actions", actionList.m_allActionNames, actionList.m_nameCount);
		}
	}
}

IEquipmentSystemInterface *CEditorGame::GetIEquipmentSystemInterface()
{
	return m_pEquipmentSystemInterface;
}

void CEditorGame::SetGameRules()
{
	IGameFramework *pGameFramework = g_pGame->GetIGameFramework();
	IConsole *pConsole = gEnv->pConsole;
	const char *szGameRules = NULL;
	const char *szLevelName = pConsole->GetCVar("sv_map")->GetString();
	ILevelInfo *pLevelInfo = pGameFramework->GetILevelSystem()->GetLevelInfo(szLevelName);

	if (pLevelInfo)
	{
		szGameRules = pLevelInfo->GetDefaultGameRules();
	}

	if (!szGameRules)
	{
		szGameRules = s_pEditorGame->m_bUsingMultiplayerGameRules ? "DeathMatch" : "SinglePlayer";
	}

	pGameFramework->GetIGameRulesSystem()->CreateGameRules(szGameRules);
	pConsole->GetCVar("sv_gamerules")->Set(szGameRules);
}
