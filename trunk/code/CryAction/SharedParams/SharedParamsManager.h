/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2010.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Shared parameters manager.
             System for managing storage and retrieval of shared parameters.

-------------------------------------------------------------------------
History:
- 29:04:2010: Created by Paul Slinger

*************************************************************************/

////////////////////////////////////////////////////////////////////////////////////////////////////
// Using the Shared Parameters System
// ----------------------------------
//
// 1) Declare a shared parameters structure
//
//      BEGIN_SHARED_PARAMS(SMySharedParams)
//
//        inline SMySharedParams(const string &_someData) : someData(_someData)
//        {
//        }
//
//        string	someData;
//
//      END_SHARED_PARAMS
//
// 2) Define type information for the shared parameters structure
//
//      DEFINE_SHARED_PARAMS_TYPE_INFO(SMySharedParams)
//
// 2) Generate a 32 bit crc (optional)
//
//      uint32 crc32 = pSharedParamsManager->GenerateCRC32("Name goes here");
//
// 3) Register the shared parameters
//
//      pSharedParamsManager->Register(crc32, SMySharedParams("I like chocolate milk"));
//
//        or
//					
//      pSharedParamsManager->Register("MySharedParams", SMySharedParams("I like chocolate milk"));
//
// 4) Retrieve the shared parameters
//
//      SMySharedParamsConstPtr	pSharedParams = CastSharedParamsPtr<SMySharedParams>pSharedParamsManager->Get(crc32);
//
//        or
//
//      SMySharedParamsConstPtr	pSharedParams = CastSharedParamsPtr<SMySharedParams>pSharedParamsManager->Get("Name goes here");
//			
// Additional Info
// ---------------
// The shared parameters manager stores a map of names internally in order to catch crc collisions.
// This means that in the unlikely event of a name crc collision, GenerateCRC32() will return 0.
// Register() and Get() will both return a null pointer.
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __SHARED_PARAMS_MANAGER_H__
#define __SHARED_PARAMS_MANAGER_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <ISystem.h>
#include <STLPoolAllocator_ManyElems.h>
#ifdef LINUX
#include <tr1/unordered_map>


#else
#include <unordered_map>
#endif
#include "ISharedParamsManager.h"
#include "SharedParams/ISharedParams.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Shared parameters manager class.
// System for managing storage and retrieval of game parameters.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CSharedParamsManager : public ISharedParamsManager
{
	public:

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Default constructor.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		CSharedParamsManager();

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Destructor.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		~CSharedParamsManager();

		// ISharedParamsManager

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Generate unique 32 bit crc.
		//
		// Params: pName - unique name
		//
		// Return: unique 32 bit crc
		////////////////////////////////////////////////////////////////////////////////////////////////////
		VIRTUAL uint32 GenerateCRC32(const char *pName);

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Register shared parameters.
		//
		// Params: crc32        - unique 32 bit crc
		//				 sharedParams - shared parameters
		//
		// Return: pointer to shared parameters
		////////////////////////////////////////////////////////////////////////////////////////////////////
		VIRTUAL ISharedParamsConstPtr Register(uint32 crc32, const ISharedParams &sharedParams);

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Register shared parameters.
		//
		// Params: pName        - unique name
		//				 sharedParams - shared parameters
		//
		// Return: pointer to shared parameters
		////////////////////////////////////////////////////////////////////////////////////////////////////
		VIRTUAL ISharedParamsConstPtr Register(const char *pName, const ISharedParams &sharedParams);

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Remove shared parameters.
		//
		// Params: crc32 - unique 32 bit crc
		////////////////////////////////////////////////////////////////////////////////////////////////////
		VIRTUAL void Remove(uint32 crc32);

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Remove shared parameters.
		//
		// Params: pName - unique name
		////////////////////////////////////////////////////////////////////////////////////////////////////
		VIRTUAL void Remove(const char *pName);

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Remove all shared parameters by type.
		//
		// Params: typeInfo - type information of to be removed
		////////////////////////////////////////////////////////////////////////////////////////////////////
		VIRTUAL void RemoveByType(const CSharedParamsTypeInfo &typeInfo);

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Get shared parameters.
		//
		// Params: crc32 - unique 32 bit crc
		//
		// Return: pointer to shared parameters
		////////////////////////////////////////////////////////////////////////////////////////////////////
		VIRTUAL ISharedParamsConstPtr Get(uint32 crc32) const;

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Get shared parameters.
		//
		// Params: pName - unique name
		//
		// Return: pointer to shared parameters
		////////////////////////////////////////////////////////////////////////////////////////////////////
		VIRTUAL ISharedParamsConstPtr Get(const char *pName) const;

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Reset.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		VIRTUAL void Reset();

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Release.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		VIRTUAL void Release();

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Get memory statistics.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		VIRTUAL void GetMemoryStatistics(ICrySizer *pSizer);

		// ~ISharedParamsManager

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Lock.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		void Lock();

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Unlock.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		void Unlock();

	private:

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Verify unlocked.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		bool VerifyUnlocked() const;

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Hashing function.
		////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(STLPORT) || defined(CAFE)
		typedef std::hash<uint32> THashUInt32;
#else
		typedef std::tr1::hash<uint32> THashUInt32;
#endif //STLPORT

#ifndef CAFE
		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Map of names.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		typedef std::tr1::unordered_map<uint32, string, THashUInt32, std::equal_to<uint32>, stl::STLPoolAllocator_ManyElems<std::pair<uint32, string> > > TNameMap;

		////////////////////////////////////////////////////////////////////////////////////////////////////
		// Map of shared parameters.
		////////////////////////////////////////////////////////////////////////////////////////////////////
		typedef std::tr1::unordered_map<uint32, ISharedParamsPtr, THashUInt32, std::equal_to<uint32>, stl::STLPoolAllocator_ManyElems<std::pair<uint32, ISharedParamsPtr> > > TRecordMap;



#endif

		TNameMap			m_names;

		TRecordMap		m_records;

		size_t				m_sizeOfSharedParams;

		bool					m_lock;
		
		mutable bool	m_outputLockWarning;
};

#endif //__SHARED_PARAMS_MANAGER_H__