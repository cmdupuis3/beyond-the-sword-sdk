#include "CvGameCoreDLL.h"

#include "CvGameCoreDLLUndefNew.h"

#include <new>

#include "CvGlobals.h"
#include "FProfiler.h"
#include "CvDLLInterfaceIFaceBase.h"

//
// operator global new and delete override for gamecore DLL 
//
void *__cdecl operator new(size_t size)
{
	if (gDLL)
	{
		return gDLL->newMem(size, __FILE__, __LINE__);
	}
	return malloc(size);
}

void __cdecl operator delete (void *p)
{
	if (gDLL)
	{
		gDLL->delMem(p, __FILE__, __LINE__);
	}
	else
	{
		free(p);
	}
}

void* operator new[](size_t size)
{
	if (gDLL)
		return gDLL->newMemArray(size, __FILE__, __LINE__);
	return malloc(size);
}

void operator delete[](void* pvMem)
{
	if (gDLL)
	{
		gDLL->delMemArray(pvMem, __FILE__, __LINE__);
	}
	else
	{
		free(pvMem);
	}
}

void *__cdecl operator new(size_t size, char* pcFile, int iLine)
{
	return gDLL->newMem(size, pcFile, iLine);
}

void *__cdecl operator new[](size_t size, char* pcFile, int iLine)
{
	return gDLL->newMem(size, pcFile, iLine);
}

void __cdecl operator delete(void* pvMem, char* pcFile, int iLine)
{
	gDLL->delMem(pvMem, pcFile, iLine);
}

void __cdecl operator delete[](void* pvMem, char* pcFile, int iLine)
{
	gDLL->delMem(pvMem, pcFile, iLine);
}


void* reallocMem(void* a, unsigned int uiBytes, const char* pcFile, int iLine)
{
	return gDLL->reallocMem(a, uiBytes, pcFile, iLine);
}

unsigned int memSize(void* a)
{
	return gDLL->memSize(a);
}

BOOL APIENTRY DllMain(HANDLE hModule, 
					  DWORD  ul_reason_for_call, 
					  LPVOID lpReserved)
{
	switch( ul_reason_for_call ) {
	case DLL_PROCESS_ATTACH:
		{
		// The DLL is being loaded into the virtual address space of the current process as a result of the process starting up 
		OutputDebugString("DLL_PROCESS_ATTACH\n");

		// set timer precision
		MMRESULT iTimeSet = timeBeginPeriod(1);		// set timeGetTime and sleep resolution to 1 ms, otherwise it's 10-16ms
		FAssertMsg(iTimeSet==TIMERR_NOERROR, "failed setting timer resolution to 1 ms");
		}
		break;
	case DLL_THREAD_ATTACH:
		// OutputDebugString("DLL_THREAD_ATTACH\n");
		break;
	case DLL_THREAD_DETACH:
		// OutputDebugString("DLL_THREAD_DETACH\n");
		break;
	case DLL_PROCESS_DETACH:
		OutputDebugString("DLL_PROCESS_DETACH\n");
		timeEndPeriod(1);
		GC.setDLLIFace(NULL);
		break;
	}
	
	return TRUE;	// success
}

//
// enable dll profiler if necessary, clear history
//
void startProfilingDLL()
{
	if (GC.isDLLProfilerEnabled())
	{
		gDLL->ProfilerBegin();
	}
}

//
// dump profile stats on-screen
//
void stopProfilingDLL()
{
	if (GC.isDLLProfilerEnabled())
	{
		gDLL->ProfilerEnd();
	}
}

#ifdef BRIDGED_PROFILE
#include <stdio.h>
#include <stdlib.h>

BridgedProfileSample* g_pBridgedProfileCurrent = NULL;
static BridgedProfileSample* g_pBridgedProfileHead = NULL;
static unsigned long long g_uiBridgedProfileStartTicks = 0;
static LARGE_INTEGER g_bridgedProfileStartCounter;

void bridgedProfileRegister(BridgedProfileSample* pSample)
{
	if (g_pBridgedProfileHead == NULL)
	{
		g_uiBridgedProfileStartTicks = bridgedProfileTicks();
		QueryPerformanceCounter(&g_bridgedProfileStartCounter);
	}
	// Keep a printable copy of the name: some stock scopes pass a stack buffer.
	int iI = 0;
	if (pSample->szName != NULL)
	{
		for (; iI < (int)sizeof(pSample->szNameCopy) - 1 && pSample->szName[iI] != '\0'; iI++)
		{
			const char cChar = pSample->szName[iI];
			pSample->szNameCopy[iI] = (cChar >= ' ' && cChar <= '~') ? cChar : '?';
		}
	}
	pSample->szNameCopy[iI] = '\0';
	pSample->bRegistered = true;
	pSample->pNext = g_pBridgedProfileHead;
	g_pBridgedProfileHead = pSample;
}

static int bridgedProfileCompare(const void* pA, const void* pB)
{
	const BridgedProfileSample* pSampleA = *(const BridgedProfileSample* const*)pA;
	const BridgedProfileSample* pSampleB = *(const BridgedProfileSample* const*)pB;
	if (pSampleA->uiTicks == pSampleB->uiTicks)
	{
		return 0;
	}
	return (pSampleA->uiTicks > pSampleB->uiTicks) ? -1 : 1;
}

//
// Rewrites the log with the totals so far, slowest first, every tenth game turn.
// Exclusive time is inclusive time minus time in nested samples; it is approximate for
// functions that recurse through another sample.
//
void bridgedProfileTurn(int iGameTurn)
{
	const char* szPath = getenv("BRIDGED_PROFILE_LOG");
	if (szPath == NULL || (iGameTurn % 10) != 0 || g_pBridgedProfileHead == NULL)
	{
		return;
	}

	int iCount = 0;
	BridgedProfileSample* pSample;
	for (pSample = g_pBridgedProfileHead; pSample != NULL; pSample = pSample->pNext)
	{
		iCount++;
	}
	BridgedProfileSample** apSamples = (BridgedProfileSample**)malloc(iCount * sizeof(BridgedProfileSample*));
	if (apSamples == NULL)
	{
		return;
	}
	iCount = 0;
	for (pSample = g_pBridgedProfileHead; pSample != NULL; pSample = pSample->pNext)
	{
		apSamples[iCount++] = pSample;
	}
	qsort(apSamples, iCount, sizeof(BridgedProfileSample*), bridgedProfileCompare);

	LARGE_INTEGER counter;
	LARGE_INTEGER frequency;
	QueryPerformanceCounter(&counter);
	QueryPerformanceFrequency(&frequency);
	const double fWallMs = 1000.0 * (double)(counter.QuadPart - g_bridgedProfileStartCounter.QuadPart) / (double)frequency.QuadPart;
	const double fTicksPerMs = (double)(bridgedProfileTicks() - g_uiBridgedProfileStartTicks) / fWallMs;

	FILE* pFile = fopen(szPath, "w");
	if (pFile != NULL)
	{
		fprintf(pFile, "game_turn\t%d\twall_ms_since_first_sample\t%.0f\n", iGameTurn, fWallMs);
		fprintf(pFile, "inclusive_ms\texclusive_ms\tcalls\tname\n");
		for (int iI = 0; iI < iCount; iI++)
		{
			pSample = apSamples[iI];
			const double fInclusive = (double)pSample->uiTicks / fTicksPerMs;
			const double fChildren = (double)pSample->uiChildTicks / fTicksPerMs;
			fprintf(pFile, "%.1f\t%.1f\t%u\t%s\n", fInclusive, fInclusive - fChildren, pSample->uiCalls, pSample->szNameCopy);
		}
		fclose(pFile);
	}
	free(apSamples);
}
#endif	// BRIDGED_PROFILE
