/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 14:08:2006   11:29 : Created by AlexL

*************************************************************************/
#include "StdAfx.h"
#include "ScriptBind_Game.h"
#include "Game.h"
#include "IAIActor.h"
#include "IAIObject.h"


#ifdef WIN64
#pragma warning(disable: 4244)
#endif


//------------------------------------------------------------------------
CScriptBind_Game::CScriptBind_Game(ISystem *pSystem, IGameFramework *pGameFramework)
	: m_pSystem(pSystem),
	  m_pSS(pSystem->GetIScriptSystem()),
	  m_pGameFW(pGameFramework)
{
	Init(m_pSS);
	SetGlobalName("Game");
	RegisterMethods();
	RegisterGlobals();
}

//------------------------------------------------------------------------
CScriptBind_Game::~CScriptBind_Game()
{
}

//------------------------------------------------------------------------
void CScriptBind_Game::RegisterGlobals()
{
}

//------------------------------------------------------------------------
void CScriptBind_Game::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_Game::
	SCRIPT_REG_TEMPLFUNC(QueryBattleStatus, "");
	SCRIPT_REG_TEMPLFUNC(IsPlayer, "entityId");
	m_pSS->SetGlobalValue("eGameCacheResourceFlag_TextureNoStream", FT_DONT_STREAM);
	m_pSS->SetGlobalValue("eGameCacheResourceFlag_TextureReplicateAllSides", FT_REPLICATE_TO_ALL_SIDES);
	m_pSS->SetGlobalValue("eGameCacheResourceType_Texture", CScriptBind_Game::eGCRT_Texture);
	m_pSS->SetGlobalValue("eGameCacheResourceType_TextureDeferredCubemap", CScriptBind_Game::eGCRT_TextureDeferredCubemap);
	m_pSS->SetGlobalValue("eGameCacheResourceType_StaticObject", CScriptBind_Game::eGCRT_StaticObject);
	m_pSS->SetGlobalValue("eGameCacheResourceType_Material", CScriptBind_Game::eGCRT_Material);
	SCRIPT_REG_TEMPLFUNC(CacheResource, "whoIsRequesting, resourceName, resourceType, resourceFlags");
#undef SCRIPT_REG_CLASSNAME
}

//------------------------------------------------------------------------


//------------------------------------------------------------------------


//------------------------------------------------------------------------
int CScriptBind_Game::PauseGame( IFunctionHandler *pH, bool pause )
{
	bool forced = false;

	if (pH->GetParamCount() > 1)
	{
		pH->GetParam(2, forced);
	}

	m_pGameFW->PauseGame(pause, forced);
	return pH->EndFunction();
}

//------------------------------------------------------------------------


//------------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
int CScriptBind_Game::QueryBattleStatus(IFunctionHandler *pH)
{
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////

int CScriptBind_Game::IsPlayer(IFunctionHandler *pH, ScriptHandle entityId)
{
	EntityId eId = (EntityId) entityId.n;

	if (eId == LOCAL_PLAYER_ENTITY_ID)
	{
		return pH->EndFunction(true);
	}

	IActor *pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(eId);
	return pH->EndFunction(pActor && pActor->IsPlayer());
}

int CScriptBind_Game::CacheResource(IFunctionHandler *pH, const char *whoIsRequesting, const char *resourceName, int resourceType, int resourceFlags)
{
	// to be implemented soon
	return pH->EndFunction();
}
