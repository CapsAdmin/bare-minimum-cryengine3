/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2005.
  -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description:  Register the factory templates used to create classes from names
                e.g. REGISTER_FACTORY(pFramework, "Player", CPlayer, false);

                Since overriding this function creates template based linker errors,
                it's been replaced by a standalone function in its own cpp file.

  -------------------------------------------------------------------------
  History:
  - 17:8:2005   Created by Nick Hesketh - Refactor'd from Game.cpp/h

*************************************************************************/

#include "StdAfx.h"
#include "Game.h"
#include "Player.h"

#include "GameRules.h"

#include <IVehicleSystem.h>
#include <IGameRulesSystem.h>

#include "GameRulesManager.h"

#define HIDE_FROM_EDITOR(className)																																				\
	{ IEntityClass *pItemClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(className);\
		pItemClass->SetFlags(pItemClass->GetFlags() | ECLF_INVISIBLE); }																				\
	 
#define REGISTER_GAME_OBJECT(framework, name, script)\
	{\
		IEntityClassRegistry::SEntityClassDesc clsDesc;\
		clsDesc.sName = #name;\
		clsDesc.sScriptFile = script;\
		struct C##name##Creator : public IGameObjectExtensionCreatorBase\
		{\
			C##name *Create()\
			{\
				return new C##name();\
			}\
			void GetGameObjectExtensionRMIData( void ** ppRMI, size_t * nCount )\
			{\
				C##name::GetGameObjectExtensionRMIData( ppRMI, nCount );\
			}\
		};\
		static C##name##Creator _creator;\
		framework->GetIGameObjectSystem()->RegisterExtension(#name, &_creator, &clsDesc);\
	}

#define REGISTER_GAME_OBJECT_EXTENSION(framework, name)\
	{\
		struct C##name##Creator : public IGameObjectExtensionCreatorBase\
		{\
			C##name *Create()\
			{\
				return new C##name();\
			}\
			void GetGameObjectExtensionRMIData( void ** ppRMI, size_t * nCount )\
			{\
				C##name::GetGameObjectExtensionRMIData( ppRMI, nCount );\
			}\
		};\
		static C##name##Creator _creator;\
		framework->GetIGameObjectSystem()->RegisterExtension(#name, &_creator, NULL);\
	}

// Register the factory templates used to create classes from names. Called via CGame::Init()
void InitGameFactory(IGameFramework *pFramework)
{
	assert(pFramework);
	REGISTER_FACTORY(pFramework, "NullAI", CPlayer, true);
	HIDE_FROM_EDITOR("NullAI");
	REGISTER_FACTORY(pFramework, "Player", CPlayer, false);
	REGISTER_FACTORY(pFramework, "GameRules", CGameRules, false);
	CGameRulesManager::GetInstance()->Init();
}
