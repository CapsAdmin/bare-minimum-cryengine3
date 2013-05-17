/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description:

 -------------------------------------------------------------------------
  History:
  - 3:8:2004   11:23 : Created by Marcio Martins

*************************************************************************/
#ifndef __GAME_H__
#define __GAME_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <IGame.h>
#include <IGameFramework.h>
#include <IGameObjectSystem.h>
#include <IGameObject.h>
#include <IActorSystem.h>
#include <StlUtils.h>
#include "RayCastQueue.h"
#include "ClientSynchedStorage.h"
#include "ServerSynchedStorage.h"
#include "ClientGameTokenSynch.h"
#include "ServerGameTokenSynch.h"
#include <IntersectionTestQueue.h>
#include "ILevelSystem.h"

#define GAME_NAME				"CryENGINE3"
#define GAME_LONGNAME		"CryENGINE3_Sample"

#define ACTIONMAP_DEFAULT_PROFILE	"libs/config/defaultProfile.xml"

struct ISystem;
struct IConsole;

class CScriptBind_Actor;
class CScriptBind_GameRules;
class CScriptBind_Game;

class CWeaponSystem;

class CGameMechanismManager;

struct IActionMap;
struct IActionFilter;
class  CGameActions;
class CGameRules;
class CSynchedStorage;
class CClientSynchedStorage;
class CServerSynchedStorage;
class CClientGameTokenSynch;
class CServerGameTokenSynch;
struct SCVars;
struct SItemStrings;
class CItemSharedParamsList;
class CWeaponSharedParamsList;
class CCameraManager;

// when you add stuff here, also update in CGame::RegisterGameObjectEvents
enum ECryGameEvent
{
	eCGE_PreFreeze = eGFE_PreFreeze,	// this is really bad and must be fixed
	eCGE_PreShatter = eGFE_PreShatter,
	eCGE_PostFreeze = 256,
	eCGE_PostShatter,
	eCGE_OnShoot,
	eCGE_Recoil,
	eCGE_BeginReloadLoop,
	eCGE_EndReloadLoop,
	eCGE_ActorRevive,
	eCGE_VehicleDestroyed,
	eCGE_TurnRagdoll,
	eCGE_EnableFallAndPlay,
	eCGE_DisableFallAndPlay,
	eCGE_VehicleTransitionEnter,
	eCGE_VehicleTransitionExit,
	eCGE_TextArea,
	eCGE_InitiateAutoDestruction,
	eCGE_Event_Collapsing,
	eCGE_Event_Collapsed,
	eCGE_MultiplayerChatMessage,
	eCGE_ResetMovementController,
	eCGE_AnimateHands,
	eCGE_Ragdoll,
	eCGE_EnablePhysicalCollider,
	eCGE_DisablePhysicalCollider,
	eCGE_RebindAnimGraphInputs,
	eCGE_OpenParachute,
	eCGE_Turret_LockedTarget,
	eCGE_Turret_LostTarget,
	eCGE_ReactionEnd
};

enum EInviteAcceptedState
{
	eIAS_None = 0,
	eIAS_Init,										// initialisation
	eIAS_StartAcceptInvite,				// begin the process
	eIAS_InitProfile,							// progress to profile loading screen, user might not have created profile yet
	eIAS_WaitForInitProfile,			// wait for profile creation to be finished
	eIAS_WaitForLoadToFinish,			// waiting for loading to finish
	eIAS_DisconnectGame,					// disconnect user from game
	eIAS_DisconnectLobby,					// disconnect user from lobby session
	eIAS_WaitForSessionDelete,		// waiting for game session to be deleted
	eIAS_ConfirmInvite,
	eIAS_WaitForInviteConfirmation,
	eIAS_InitSinglePlayer,
	eIAS_WaitForInitSinglePlayer,
	eIAS_WaitForSplashScreen,			// return user to splash screen
	eIAS_WaitForValidUser,				// PS3 - need user to select controller
	eIAS_InitMultiplayer,					// init the multiplayer gamemode
	eIAS_WaitForInitMultiplayer,	// wait for multiplayer game mode to be initialised
	eIAS_InitOnline,							// init online functionality
	eIAS_WaitForInitOnline,				// wait for online mode to be initialised
	eIAS_WaitForSquadManagerEnabled,
	eIAS_Accept,									// accept the invite
	eIAS_Error,										// we recieved an invite that had an error attached to it
};

static const int GLOBAL_SERVER_IP_KEY						=	1000;
static const int GLOBAL_SERVER_PUBLIC_PORT_KEY	= 1001;
static const int GLOBAL_SERVER_NAME_KEY					=	1002;

#define CryInviteID CrySessionID
#define CryInvalidInvite CrySessionInvalidID

class CGame :
	public IGame, public IGameFrameworkListener, public ISystemEventListener
{
	public:
		typedef bool (*BlockingConditionFunction)();
		typedef RayCastQueue<41> GlobalRayCaster;
		typedef IntersectionTestQueue<43> GlobalIntersectionTester;

		enum EHostMigrationState
		{
			eHMS_NotMigrating,
			eHMS_WaitingForPlayers,
			eHMS_Resuming,
		};

		enum ERichPresenceType
		{
			eRPT_String = 0,
			eRPT_Param1,
			eRPT_Param2,
			eRPT_Max,
		};

	public:
		CGame();
		VIRTUAL ~CGame();

		// IGame
		VIRTUAL bool  Init(IGameFramework *pFramework);
		VIRTUAL bool  CompleteInit();
		VIRTUAL void  Shutdown();
		VIRTUAL int   Update(bool haveFocus, unsigned int updateFlags);
		VIRTUAL void  ConfigureGameChannel(bool isServer, IProtocolBuilder *pBuilder);
		VIRTUAL void  EditorResetGame(bool bStart);
		VIRTUAL void  PlayerIdSet(EntityId playerId);
		VIRTUAL string  InitMapReloading();
		VIRTUAL bool IsReloading()
		{
			return m_bReload;
		}
		VIRTUAL IGameFramework *GetIGameFramework()
		{
			return m_pFramework;
		}

		VIRTUAL const char *GetLongName();
		VIRTUAL const char *GetName();

		VIRTUAL void GetMemoryStatistics(ICrySizer *s) const;

		VIRTUAL void OnClearPlayerIds();
		//auto-generated save game file name
		VIRTUAL IGame::TSaveGameName CreateSaveGameName();
		//level names were renamed without changing the file/directory
		VIRTUAL const char *GetMappedLevelName(const char *levelName) const;
		//
		VIRTUAL IGameStateRecorder *CreateGameStateRecorder(IGameplayListener *pL);

		VIRTUAL const bool DoInitialSavegame() const
		{
			return true;
		}

		VIRTUAL void CreateLobbySession( const SGameStartParams *pGameStartParams )
		{
			;
		}
		VIRTUAL void DeleteLobbySession()
		{
			;
		}

		VIRTUAL void RegisterGameFlowNodes();
		// ~IGame

		// IGameFrameworkListener
		VIRTUAL void OnPostUpdate(float fDeltaTime);
		VIRTUAL void OnSaveGame(ISaveGame *pSaveGame);
		VIRTUAL void OnLoadGame(ILoadGame *pLoadGame);
		VIRTUAL void OnLevelEnd(const char *nextLevel) {};
		VIRTUAL void OnActionEvent(const SActionEvent &event);
		// ~IGameFrameworkListener

		void BlockingProcess(BlockingConditionFunction f);
		void GameChannelDestroyed(bool isServer);

		void SetExclusiveControllerFromPreviousInput();
		void SetPreviousExclusiveControllerDeviceIndex(unsigned int idx)
		{
			m_previousInputControllerDeviceIndex = idx;
		}
		void RemoveExclusiveController();
		bool HasExclusiveControllerIndex() const
		{
			return m_hasExclusiveController;
		}
		bool IsExclusiveControllerConnected() const
		{
			return m_bExclusiveControllerConnected;
		}

		VIRTUAL CScriptBind_Actor *GetActorScriptBind()
		{
			return m_pScriptBindActor;
		}
		VIRTUAL CScriptBind_GameRules *GetGameRulesScriptBind()
		{
			return m_pScriptBindGameRules;
		}
		VIRTUAL CWeaponSystem *GetWeaponSystem()
		{
			return m_pWeaponSystem;
		};
		VIRTUAL CItemSharedParamsList *GetItemSharedParamsList()
		{
			return m_pItemSharedParamsList;
		};
		VIRTUAL CWeaponSharedParamsList *GetWeaponSharedParamsList()
		{
			return m_pWeaponSharedParamsList;
		}

		VIRTUAL uint32 AddGameWarning(const char *stringId, const char *paramMessage, IGameWarningsListener *pListener = NULL);
		VIRTUAL void RenderGameWarnings() {};
		VIRTUAL void RemoveGameWarning(const char *stringId);

		VIRTUAL bool GameEndLevel(const char *stringId)
		{
			return false;
		}
		VIRTUAL void OnRenderScene() {}

		VIRTUAL const uint8 *GetDRMKey();
		VIRTUAL const char *GetDRMFileList();

		// ISystemEventListener
		virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);
		// ~ISystemEventListener

		CGameActions	&Actions() const
		{
			return *m_pGameActions;
		};

		CGameRules *GetGameRules() const;
		virtual IGameAudio *GetGameAudio() const
		{
			return nullptr;
		}

		// camera stuff (new tp cam)
		CCameraManager *GetCameraManager();
		ILINE GlobalRayCaster &GetRayCaster()
		{
			assert(m_pRayCaster);
			return *m_pRayCaster;
		}
		GlobalIntersectionTester &GetIntersectionTester()
		{
			assert(m_pIntersectionTester);
			return *m_pIntersectionTester;
		}

		ILINE CSynchedStorage *GetSynchedStorage() const
		{
			if (m_pServerSynchedStorage && gEnv->bServer)
			{
				return m_pServerSynchedStorage;
			}

			return m_pClientSynchedStorage;
		}

		ILINE CServerSynchedStorage *GetServerSynchedStorage() const
		{
			return m_pServerSynchedStorage;
		}

		ILINE CServerGameTokenSynch *GetServerGameTokenSynch() const
		{
			return m_pServerGameTokenSynch;
		}

		ILINE CClientGameTokenSynch *GetClientGameTokenSynch() const
		{
			return m_pClientGameTokenSynch;
		}

		void ResetServerGameTokenSynch(void);

		bool IsGameActive() const;
		void ClearGameSessionHandler();

		float GetTimeSinceHostMigrationStateChanged() const;
		float GetRemainingHostMigrationTimeoutTime() const;
		float GetHostMigrationTimeTillResume() const;

		EHostMigrationState GetHostMigrationState() const
		{
			return m_hostMigrationState;
		}
		ILINE bool IsGameSessionHostMigrating() const
		{
			return m_hostMigrationState != eHMS_NotMigrating;
		}

		void SetHostMigrationState(EHostMigrationState newState);
		void SetHostMigrationStateAndTime(EHostMigrationState newState, float timeOfChange);

		void AbortHostMigration();

		void SetUserRegion(int region)
		{
			m_cachedUserRegion = region;
		}
		int GetUserRegion(void) const
		{
			return m_cachedUserRegion;
		}

		uint32 GetRandomNumber();

		const string &GetLastSaveGame(string &levelName);
		const string &GetLastSaveGame()
		{
			string tmp;
			return GetLastSaveGame(tmp);
		}
		bool LoadLastSave();
		void PostSerialize();

		ILINE SCVars *GetCVars()
		{
			return m_pCVars;
		}
		static void DumpMemInfo(const char *format, ...) PRINTF_PARAMS(1, 2);

		VIRTUAL void LoadActionMaps(const char *filename);

		EntityId GetClientActorId() const
		{
			return m_clientActorId;
		}

		static void VerifyMaxPlayers(ICVar *pVar);

	protected:
		VIRTUAL void InitScriptBinds();
		VIRTUAL void ReleaseScriptBinds();

		VIRTUAL void CheckReloadLevel();

		// These funcs live in GameCVars.cpp
		VIRTUAL void RegisterConsoleVars();
		VIRTUAL void RegisterConsoleCommands();
		VIRTUAL void UnregisterConsoleCommands();

		VIRTUAL void RegisterGameObjectEvents();

		// marcok: this is bad and evil ... should be removed soon
		static void CmdRestartGame(IConsoleCmdArgs *pArgs);
		static void CmdDumpAmmoPoolStats(IConsoleCmdArgs *pArgs);

		static void CmdDumpSS(IConsoleCmdArgs *pArgs);

		static void CmdLastInv(IConsoleCmdArgs *pArgs);
		static void CmdName(IConsoleCmdArgs *pArgs);
		static void CmdTeam(IConsoleCmdArgs *pArgs);
		static void CmdLoadLastSave(IConsoleCmdArgs *pArgs);
		static void CmdSpectator(IConsoleCmdArgs *pArgs);
		static void CmdJoinGame(IConsoleCmdArgs *pArgs);
		static void CmdKill(IConsoleCmdArgs *pArgs);
		static void CmdVehicleKill(IConsoleCmdArgs *pArgs);
		static void CmdRestart(IConsoleCmdArgs *pArgs);
		static void CmdSay(IConsoleCmdArgs *pArgs);
		static void CmdReloadItems(IConsoleCmdArgs *pArgs);
		static void CmdLoadActionmap(IConsoleCmdArgs *pArgs);
		static void CmdReloadGameRules(IConsoleCmdArgs *pArgs);
		static void CmdNextLevel(IConsoleCmdArgs *pArgs);
		static void CmdStartKickVoting(IConsoleCmdArgs *pArgs);
		static void CmdStartNextMapVoting(IConsoleCmdArgs *pArgs);
		static void CmdVote(IConsoleCmdArgs *pArgs);

		static void CmdQuickGame(IConsoleCmdArgs *pArgs);
		static void CmdQuickGameStop(IConsoleCmdArgs *pArgs);
		static void CmdLogin(IConsoleCmdArgs *pArgs);
		static void CmdLoginProfile(IConsoleCmdArgs *pArgs);
		static void CmdCryNetConnect(IConsoleCmdArgs *pArgs);
		static void CmdTestPathfinder(IConsoleCmdArgs *pArgs);

		static void CmdNetSetOnlineMode(IConsoleCmdArgs *pArgs);

		static void OnHostMigrationNetTimeoutChanged(ICVar *pVar);















		CRndGen m_randomGenerator;

		IGameFramework			*m_pFramework;
		IConsole						*m_pConsole;

		CWeaponSystem				*m_pWeaponSystem;

		bool								m_bReload;

		// script binds
		CScriptBind_Actor		*m_pScriptBindActor;
		CScriptBind_GameRules *m_pScriptBindGameRules;
		CScriptBind_Game    *m_pScriptBindGame;

		//menus

		IActionMap					*m_pDebugAM;
		IActionMap					*m_pDefaultAM;
		IActionMap					*m_pMultiplayerAM;
		CGameActions				*m_pGameActions;

		CServerSynchedStorage	*m_pServerSynchedStorage;
		CClientSynchedStorage	*m_pClientSynchedStorage;

		CClientGameTokenSynch	*m_pClientGameTokenSynch;
		CServerGameTokenSynch *m_pServerGameTokenSynch;

		bool								m_inDevMode;

		EntityId						m_clientActorId;

		SCVars	*m_pCVars;
		SItemStrings						*m_pItemStrings;
		CItemSharedParamsList		*m_pItemSharedParamsList;
		CWeaponSharedParamsList *m_pWeaponSharedParamsList;
		string                 m_lastSaveGame;

		GlobalRayCaster *m_pRayCaster;
		GlobalIntersectionTester *m_pIntersectionTester;

		typedef std::map<string, string, stl::less_stricmp<string> > TLevelMapMap;
		TLevelMapMap m_mapNames;

		// new tp camera stuff
		CCameraManager *m_pCameraManager;

		CGameMechanismManager *m_pGameMechanismManager;
		int m_cachedUserRegion;

		float m_hostMigrationTimeStateChanged;			// Time when the host migration started (from timer->GetAsyncCurTime())
		float m_hostMigrationNetTimeoutLength;
		EHostMigrationState m_hostMigrationState;

		bool				 m_hasExclusiveController;
		bool				 m_bExclusiveControllerConnected;
		bool				 m_previousPausedGameState;

		unsigned int m_previousInputControllerDeviceIndex;
};

extern CGame *g_pGame;

#define SAFE_HARDWARE_MOUSE_FUNC(func)\
	if(gEnv->pHardwareMouse)\
		gEnv->pHardwareMouse->func


//////////////////////////////////////////////////////////////////////////////
inline CCameraManager *CGame::GetCameraManager()
{
	return m_pCameraManager;
}

extern CGame *g_pGame;

#endif //__GAME_H__
