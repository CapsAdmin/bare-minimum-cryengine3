/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Stores all item parameters that don't mutate in instances...
						 Allows for some nice memory savings...

-------------------------------------------------------------------------
History:
- 3:4:2007   10:54 : Created by Marcio Martins

*************************************************************************/
#ifndef __ITEMSHAREDPARAMS_H__
#define __ITEMSHAREDPARAMS_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Item.h"


class CItemSharedParams
{
	protected:
		mutable uint32	m_refs;
		bool					m_valid;
	public:
		CItemSharedParams(): m_refs(0), m_valid(false) {};
		virtual ~CItemSharedParams()
		{
			int x = 0;
			(void)x;
		};

		virtual void AddRef() const
		{
			++m_refs;
		};
		virtual uint32 GetRefCount() const
		{
			return m_refs;
		};
		virtual void Release() const
		{
			if (--m_refs <= 0)
			{
				delete this;
			}
		};

		virtual bool Valid() const
		{
			return m_valid;
		};
		virtual void SetValid(bool valid)
		{
			m_valid = valid;
		};

		void GetMemoryUsage(ICrySizer *s) const;

		CItem::TActionMap						actions;
		CItem::TAccessoryParamsMap	accessoryparams;
		CItem::THelperVector				helpers;
		CItem::TLayerMap						layers;
		CItem::TDualWieldSupportMap	dualWieldSupport;
		CItem::SParams							params;
};


class CItemSharedParamsList
{
		typedef std::map<string, _smart_ptr<CItemSharedParams> > TSharedParamsMap;
	public:
		CItemSharedParamsList() {};
		virtual ~CItemSharedParamsList() {};

		void Reset()
		{
			m_params.clear();
		};
		CItemSharedParams *GetSharedParams(const char *className, bool create);

		void GetMemoryUsage(ICrySizer *s) const;

		TSharedParamsMap m_params;
};

#endif //__ITEMSHAREDPARAMS_H__