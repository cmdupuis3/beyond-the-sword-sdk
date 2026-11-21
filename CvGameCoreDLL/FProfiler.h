// ---------------------------------------------------------------------------------------------------
// FProfiler - DLL wrapper to FireEngine/FProfiler.cpp
//
// Author: tomw
//---------------------------------------------------------------------------------------------------------------------

#ifndef	__PROFILE_H__
#define __PROFILE_H__
#pragma once


#include "CvDLLEntityIFaceBase.h"
#include "CvDLLUtilityIFaceBase.h"
#include "CvGlobals.h"	// for gDLL

#ifndef FINAL_RELEASE
#ifndef FP_PROFILE_ENABLE 
	#define FP_PROFILE_ENABLE 
#endif
#endif

//NOTE: This struct must be identical ot the same struct in  FireEngine/FProfiler.h
//---------------------------------------------------------------------------------------------------------------------
struct ProfileSample
{
	ProfileSample(char *name)					{ strcpy(Name, name); Added=false; Parent=-1; }

	char	Name[256];						// Name of sample;

	unsigned int	ProfileInstances;		// # of times ProfileBegin Called
	int				OpenProfiles;			// # of time ProfileBegin called w/o ProfileEnd
	double			StartTime;				// The current open profile start time
	double			Accumulator;			// All samples this frame added together

	double			ChildrenSampleTime;		// Time taken by all children
	unsigned int	NumParents;				// Number of profile Parents
	bool			Added;					// true when added to the list
	int				Parent;
};

//---------------------------------------------------------------------------------------------------------------------
// Allows us to Profile based on Scope, to limit intrusion into code.
// Simply use PROFLE("funcname") instead having to insert begin()/end() pairing
class CProfileScope
{
public:
	CProfileScope() { bValid= false;};
	CProfileScope(ProfileSample *pSample)
	{
		m_pSample = pSample;
		bValid = true;
		gDLL->BeginSample(m_pSample);
	};
	~CProfileScope()
	{
		if(bValid )
		{
			gDLL->EndSample(m_pSample);
			bValid = false;
		}	
	};

private:
	bool bValid;
	ProfileSample *m_pSample;
};

//---------------------------------------------------------------------------------------------------------------------

#ifdef BRIDGED_PROFILE
// In-DLL profiler. The exe's profiler (gDLL->BeginSample) only draws on screen in debug
// builds, so with BRIDGED_PROFILE the same PROFILE scopes are timed here instead and
// written to the file named by the BRIDGED_PROFILE_LOG environment variable.
// Single-threaded: the current-sample pointer is one global.
struct BridgedProfileSample
{
	const char* szName;					// may point at a stack buffer; valid only until first use
	unsigned long long uiTicks;			// inclusive; counted for the outermost entry only
	unsigned long long uiChildTicks;	// spent in samples opened inside this one
	unsigned long long uiStart;			// PROFILE_BEGIN / PROFILE_END only
	BridgedProfileSample* pParent;		// PROFILE_BEGIN / PROFILE_END only
	BridgedProfileSample* pNext;		// registry of samples that have run
	unsigned int uiCalls;
	int iOpen;							// recursion depth
	bool bRegistered;
	char szNameCopy[64];				// taken from szName when the sample first runs
};

extern BridgedProfileSample* g_pBridgedProfileCurrent;
void bridgedProfileRegister(BridgedProfileSample* pSample);
void bridgedProfileTurn(int iGameTurn);	// call once per game turn; writes the log

inline unsigned long long bridgedProfileTicks()
{
	return __builtin_ia32_rdtsc();
}

inline BridgedProfileSample* bridgedProfileEnter(BridgedProfileSample* pSample)
{
	if (!pSample->bRegistered)
	{
		bridgedProfileRegister(pSample);
	}
	BridgedProfileSample* pParent = g_pBridgedProfileCurrent;
	g_pBridgedProfileCurrent = pSample;
	pSample->iOpen++;
	return pParent;
}

inline void bridgedProfileLeave(BridgedProfileSample* pSample, BridgedProfileSample* pParent, unsigned long long uiStart)
{
	const unsigned long long uiElapsed = bridgedProfileTicks() - uiStart;
	pSample->uiCalls++;
	if (--pSample->iOpen == 0)
	{
		pSample->uiTicks += uiElapsed;
	}
	if (pParent != NULL && pParent != pSample)
	{
		pParent->uiChildTicks += uiElapsed;
	}
	g_pBridgedProfileCurrent = pParent;
}

// For counts and timings taken by hand rather than through a scope.
inline void bridgedProfileAdd(BridgedProfileSample* pSample, unsigned long long uiTicks, unsigned int uiCalls = 1)
{
	if (!pSample->bRegistered)
	{
		bridgedProfileRegister(pSample);
	}
	pSample->uiTicks += uiTicks;
	pSample->uiCalls += uiCalls;
}
// Times a stretch of code into a sample chosen at run time. Unlike a scope it does not take
// part in the parent and child bookkeeping, so the time also stays in whatever scope is open.
class BridgedProfileManual
{
public:
	BridgedProfileManual(BridgedProfileSample* pSample) : m_pSample(pSample), m_uiStart(bridgedProfileTicks()) {}
	~BridgedProfileManual() { bridgedProfileAdd(m_pSample, bridgedProfileTicks() - m_uiStart); }

private:
	BridgedProfileSample* m_pSample;
	unsigned long long m_uiStart;
};

class BridgedProfileScope
{
public:
	BridgedProfileScope(BridgedProfileSample* pSample) : m_pSample(pSample)
	{
		m_pParent = bridgedProfileEnter(pSample);
		m_uiStart = bridgedProfileTicks();
	}
	~BridgedProfileScope()
	{
		bridgedProfileLeave(m_pSample, m_pParent, m_uiStart);
	}

private:
	BridgedProfileSample* m_pSample;
	BridgedProfileSample* m_pParent;
	unsigned long long m_uiStart;
};

#define PROFILE(name)\
	static BridgedProfileSample sample = { name };\
	BridgedProfileScope ProfileScope(&sample);

#define PROFILE_BEGIN(name)\
	static BridgedProfileSample sample__ = { name };\
	sample__.pParent = bridgedProfileEnter(&sample__);\
	sample__.uiStart = bridgedProfileTicks();
#define PROFILE_END()\
	bridgedProfileLeave(&sample__, sample__.pParent, sample__.uiStart);

#define PROFILE_FUNC()\
	static BridgedProfileSample sample = { __FUNCTION__ };\
	BridgedProfileScope ProfileScope(&sample);

#else	// BRIDGED_PROFILE

// Main Interface for Profile
#ifdef FP_PROFILE_ENABLE				// Turn Profiling On or Off .. 
#define PROFILE(name)\
	static ProfileSample sample(name);\
	CProfileScope ProfileScope(&sample);		

//BEGIN & END macros:		Only needed if you don't want to use the scope macro above. 
// Macros must be in the same scope
#define PROFILE_BEGIN(name)\
	static ProfileSample sample__(name);\
	gDLL->BeginSample(&sample__);
#define PROFILE_END()\
	gDLL->EndSample(&sample__);

#define PROFILE_FUNC()\
	static ProfileSample sample(__FUNCTION__);\
	CProfileScope ProfileScope(&sample);		

#else
#define PROFILE(name)				// Remove profiling code
#define PROFILE_BEGIN(name)
#define PROFILE_END()
#define PROFILE_FUNC()
#endif

#endif	// BRIDGED_PROFILE


#endif //__PROFILE_H__


