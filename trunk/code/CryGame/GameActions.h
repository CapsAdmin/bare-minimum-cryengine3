#ifndef __GAMEACTIONS_H__
#define __GAMEACTIONS_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <IActionMapManager.h>

#undef small
#define DECL_ACTION(name) ActionId name;
class CGameActions
{
	public:
		CGameActions();
#include "GameActions.actions"

		void Init();
		ILINE IActionFilter	*FilterNoMove() const
		{
			return m_pFilterNoMove;
		}
		ILINE IActionFilter	*FilterNoMouse() const
		{
			return m_pFilterNoMouse;
		}
		ILINE IActionFilter	*FilterFreezeTime() const
		{
			return m_pFilterFreezeTime;
		}
		ILINE IActionFilter	*FilterCutscene() const
		{
			return m_pFilterCutscene;
		}
		ILINE IActionFilter	*FilterCutsceneNoPlayer() const
		{
			return m_pFilterCutsceneNoPlayer;
		}
		ILINE IActionFilter	*FilterNoConnectivity() const
		{
			return m_pFilterNoConnectivity;
		}
		ILINE IActionFilter	*FilterUIOnly() const
		{
			return m_pFilterUIOnly;
		}


	private:
		void	CreateFilterNoMove();
		void	CreateFilterNoMouse();
		void	CreateFilterFreezeTime();
		void	CreateFilterCutscene();
		void	CreateFilterCutsceneNoPlayer();
		void	CreateFilterNoConnectivity();
		void	CreateFilterUIOnly();

		IActionFilter	*m_pFilterNoMove;
		IActionFilter	*m_pFilterNoMouse;
		IActionFilter	*m_pFilterFreezeTime;
		IActionFilter	*m_pFilterCutscene;
		IActionFilter	*m_pFilterCutsceneNoPlayer;
		IActionFilter	*m_pFilterNoConnectivity;
		IActionFilter	*m_pFilterUIOnly;
};
#undef DECL_ACTION

extern CGameActions *g_pGameActions;

#endif //__GAMEACTIONS_H__
