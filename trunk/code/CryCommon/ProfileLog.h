
//////////////////////////////////////////////////////////////////////
//
//	Crytek CryENGINE Source code
//	
//	File:Log.h
//
//	History:
//	-Feb 2,2001:Created by Pavlo Gryb
//
//////////////////////////////////////////////////////////////////////

#ifndef PROFILELOG_H
#define PROFILELOG_H

#if _MSC_VER > 1000
# pragma once
#endif

#include "ISystem.h"
#include "ITimer.h"

struct ILogElement
{
	virtual ~ILogElement(){}
	virtual ILogElement* 	Log			(const char* name, const char* message) = 0;
	virtual ILogElement*	SetTime	(float time)														= 0;
	virtual void					Flush		(stack_string& indent)									= 0;
};

struct IProfileLogSystem
{
	virtual ~IProfileLogSystem(){}
	virtual ILogElement*	Log			(const char* name, const char* msg)		= 0;
	virtual void					SetTime	(ILogElement* pElement, float time)		= 0;
	virtual void					Release	()																		=	0;
};

struct SHierProfileLogItem
{
  SHierProfileLogItem(const char* name, const char* msg, int inbDoLog)
	: m_pLogElement(NULL)
	, m_bDoLog(inbDoLog)
  {
    if( m_bDoLog )
    {
			m_pLogElement = gEnv->pProfileLogSystem->Log(name, msg);
      m_startTime = gEnv->pTimer->GetAsyncTime();
    }
  }
  ~SHierProfileLogItem()
  {
    if( m_bDoLog )
    {
			CTimeValue endTime = gEnv->pTimer->GetAsyncTime();
			gEnv->pProfileLogSystem->SetTime(m_pLogElement, (endTime - m_startTime).GetMilliSeconds());
    }
  }

private:
	int						m_bDoLog;
	CTimeValue		m_startTime;
	ILogElement*	m_pLogElement;
};

#define HPROFILE_BEGIN(msg1,msg2,doLog) { SHierProfileLogItem __hier_profile_uniq_var_in_this_scope__(msg1,msg2,doLog); 
#define HPROFILE_END() }

#define HPROFILE(msg1,msg2,doLog) SHierProfileLogItem __hier_profile_uniq_var_in_this_scope__(msg1,msg2,doLog); 

#ifndef RELEASE
class CScopedTimeLogger
{
	const char* m_sz;
	int64 m_start;
	double m_freq;

public:
	explicit CScopedTimeLogger(const char* sz) :
		m_sz(sz)
	{
		LARGE_INTEGER li;
		QueryPerformanceFrequency(&li);
		m_freq = double(li.QuadPart) / 1000.;
		QueryPerformanceCounter(&li);
		m_start = li.QuadPart;

		CryFixedStringT<256> logString;
		logString.Format(">>>>> Start: %s", m_sz);
		gEnv->pConsole->PrintLine(logString.c_str());
	}

	~CScopedTimeLogger()
	{
		LARGE_INTEGER li;
		QueryPerformanceCounter(&li);
		
		CryFixedStringT<256> logString;
		logString.Format(">>>>> End: %s (%f ms)", m_sz, double(li.QuadPart - m_start) / m_freq);
		gEnv->pConsole->PrintLine(logString.c_str());
	}
};

#define SCOPED_TIME_LOGGER(x) CScopedTimeLogger s_t_l_##__FUNCTION__##__LINE__(x)
#else
#define SCOPED_TIME_LOGGER(x)
#endif


#endif