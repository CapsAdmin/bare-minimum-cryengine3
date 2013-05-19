#include "StdAfx.h"
#include "GameActions.h"
#include "Game.h"

#define DECL_ACTION(name) name = #name;
CGameActions::CGameActions()
	: m_pFilterNoMove(0)
	, m_pFilterNoMouse(0)
	, m_pFilterFreezeTime(0)
	, m_pFilterCutscene(0)
	, m_pFilterCutsceneNoPlayer(0)
	, m_pFilterNoConnectivity(0)
	, m_pFilterUIOnly(0)
{
#include "GameActions.actions"
}
#undef DECL_ACTION

void CGameActions::Init()
{
	CreateFilterNoMove();
	CreateFilterNoMouse();
	CreateFilterFreezeTime();
	CreateFilterCutscene();
	CreateFilterCutsceneNoPlayer();
	CreateFilterNoConnectivity();
	CreateFilterUIOnly();
}

void CGameActions::CreateFilterNoMove()
{
	IActionMapManager *pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();
	m_pFilterNoMove = pActionMapMan->CreateActionFilter("no_move", eAFT_ActionFail);
	m_pFilterNoMove->Filter(leanleft);
	m_pFilterNoMove->Filter(leanright);
	m_pFilterNoMove->Filter(crouch);
	m_pFilterNoMove->Filter(prone);
	m_pFilterNoMove->Filter(jump);
	m_pFilterNoMove->Filter(moveleft);
	m_pFilterNoMove->Filter(moveright);
	m_pFilterNoMove->Filter(moveforward);
	m_pFilterNoMove->Filter(moveback);
	m_pFilterNoMove->Filter(sprint);
	m_pFilterNoMove->Filter(xi_movey);
	m_pFilterNoMove->Filter(xi_movex);
}

void CGameActions::CreateFilterNoMouse()
{
	IActionMapManager *pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();
	m_pFilterNoMouse = pActionMapMan->CreateActionFilter("no_mouse", eAFT_ActionFail);
	m_pFilterNoMouse->Filter(attack1);
	m_pFilterNoMouse->Filter(v_attack1);
	m_pFilterNoMouse->Filter(v_attack2);
	m_pFilterNoMouse->Filter(rotateyaw);
	m_pFilterNoMouse->Filter(v_rotateyaw);
	m_pFilterNoMouse->Filter(rotatepitch);
	m_pFilterNoMouse->Filter(v_rotatepitch);
	m_pFilterNoMouse->Filter(nextitem);
	m_pFilterNoMouse->Filter(previtem);
	m_pFilterNoMouse->Filter(small);
	m_pFilterNoMouse->Filter(medium);
	m_pFilterNoMouse->Filter(heavy);
	m_pFilterNoMouse->Filter(handgrenade);
	m_pFilterNoMouse->Filter(explosive);
	m_pFilterNoMouse->Filter(utility);
	m_pFilterNoMouse->Filter(zoom);
	m_pFilterNoMouse->Filter(reload);
	m_pFilterNoMouse->Filter(use);
	m_pFilterNoMouse->Filter(xi_use);
	m_pFilterNoMouse->Filter(xi_grenade);
	m_pFilterNoMouse->Filter(xi_handgrenade);
	m_pFilterNoMouse->Filter(xi_zoom);
	m_pFilterNoMouse->Filter(jump);
}

void CGameActions::CreateFilterFreezeTime()
{
	IActionMapManager *pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();
	m_pFilterFreezeTime = pActionMapMan->CreateActionFilter("freezetime", eAFT_ActionPass);
	m_pFilterFreezeTime->Filter(reload);
	m_pFilterFreezeTime->Filter(rotateyaw);
	m_pFilterFreezeTime->Filter(rotatepitch);
	m_pFilterFreezeTime->Filter(drop);
	m_pFilterFreezeTime->Filter(modify);
	m_pFilterFreezeTime->Filter(jump);
	m_pFilterFreezeTime->Filter(crouch);
	m_pFilterFreezeTime->Filter(prone);
	m_pFilterFreezeTime->Filter(togglestance);
	m_pFilterFreezeTime->Filter(leanleft);
	m_pFilterFreezeTime->Filter(leanright);
	m_pFilterFreezeTime->Filter(rotateyaw);
	m_pFilterFreezeTime->Filter(rotatepitch);
	m_pFilterFreezeTime->Filter(reload);
	m_pFilterFreezeTime->Filter(drop);
	m_pFilterFreezeTime->Filter(modify);
	m_pFilterFreezeTime->Filter(nextitem);
	m_pFilterFreezeTime->Filter(previtem);
	m_pFilterFreezeTime->Filter(small);
	m_pFilterFreezeTime->Filter(medium);
	m_pFilterFreezeTime->Filter(heavy);
	m_pFilterFreezeTime->Filter(explosive);
	m_pFilterFreezeTime->Filter(handgrenade);
	m_pFilterFreezeTime->Filter(holsteritem);
	m_pFilterFreezeTime->Filter(utility);
	m_pFilterFreezeTime->Filter(debug);
	m_pFilterFreezeTime->Filter(firemode);
	m_pFilterFreezeTime->Filter(objectives);
	m_pFilterFreezeTime->Filter(speedmode);
	m_pFilterFreezeTime->Filter(strengthmode);
	m_pFilterFreezeTime->Filter(defensemode);
	m_pFilterFreezeTime->Filter(invert_mouse);
	m_pFilterFreezeTime->Filter(gboots);
	m_pFilterFreezeTime->Filter(lights);
	m_pFilterFreezeTime->Filter(radio_group_0);
	m_pFilterFreezeTime->Filter(radio_group_1);
	m_pFilterFreezeTime->Filter(radio_group_2);
	m_pFilterFreezeTime->Filter(radio_group_3);
	m_pFilterFreezeTime->Filter(radio_group_4);
	m_pFilterFreezeTime->Filter(voice_chat_talk);
	// XInput specific actions
	m_pFilterFreezeTime->Filter(xi_rotateyaw);
	m_pFilterFreezeTime->Filter(xi_rotatepitch);
	m_pFilterFreezeTime->Filter(xi_v_rotateyaw);
	m_pFilterFreezeTime->Filter(xi_v_rotatepitch);
	m_pFilterFreezeTime->Filter(buyammo);
}

void CGameActions::CreateFilterCutscene()
{
	IActionMapManager *pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();
	m_pFilterCutscene = pActionMapMan->CreateActionFilter("cutscene", eAFT_ActionFail);
	m_pFilterCutscene->Filter(leanleft);
	m_pFilterCutscene->Filter(leanright);
}

void CGameActions::CreateFilterCutsceneNoPlayer()
{
	IActionMapManager *pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();
	m_pFilterCutsceneNoPlayer = pActionMapMan->CreateActionFilter("cutscene_no_player", eAFT_ActionPass);
	m_pFilterCutsceneNoPlayer->Filter(loadLastSave);
	m_pFilterCutsceneNoPlayer->Filter(load);
}

void CGameActions::CreateFilterNoConnectivity()
{
	IActionMapManager *pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();
	m_pFilterNoConnectivity = pActionMapMan->CreateActionFilter("no_connectivity", eAFT_ActionPass);
	m_pFilterNoConnectivity->Filter(scores);
}

void CGameActions::CreateFilterUIOnly()
{
	IActionMapManager *pActionMapMan = g_pGame->GetIGameFramework()->GetIActionMapManager();
	m_pFilterUIOnly = pActionMapMan->CreateActionFilter("only_ui", eAFT_ActionPass);
	m_pFilterUIOnly->Filter(ui_toggle_pause);
	m_pFilterUIOnly->Filter(ui_up);
	m_pFilterUIOnly->Filter(ui_down);
	m_pFilterUIOnly->Filter(ui_left);
	m_pFilterUIOnly->Filter(ui_right);
	m_pFilterUIOnly->Filter(ui_click);
	m_pFilterUIOnly->Filter(ui_back);
	m_pFilterUIOnly->Filter(ui_confirm);
	m_pFilterUIOnly->Filter(ui_reset);
}