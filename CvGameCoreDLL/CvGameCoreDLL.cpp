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

#ifdef BRIDGED_PGO_TRAIN
//
// Training build for profile-guided optimisation (tools/build_dll.py --pgo-train).
//
// Clang's -fprofile-generate makes every function count how often its blocks run. The
// counters, a record for each function and the function names go into three sections of
// the DLL. LLVM's own runtime library would write them to a file when the program ends,
// but it is not shipped for 32-bit Windows and it needs a modern C runtime. So the file
// is written here instead: the raw profile format of the pinned LLVM 23.1.2
// (INSTR_PROF_RAW_VERSION 11 in its profile/InstrProfData.inc), which llvm-profdata reads.
//
// Indirect-call and memory-size profiling are switched off in the build (-disable-vp):
// they are the part that needs the runtime library.
//
// The file is named by the BRIDGED_PGO_RAW environment variable and rewritten every tenth
// game turn, because the benchmark stops the game by killing it.
//
#include <stdio.h>
#include <stdlib.h>

// INSTR_PROF_DATA of LLVM 23.1.2 for a 32-bit target
struct BridgedPgoData
{
	unsigned long long uiNameRef;
	unsigned long long uiFuncHash;
	unsigned int uiCounterPtr;			// from this record to its counters
	unsigned int uiUniformCounterPtr;
	unsigned int uiBitmapPtr;
	unsigned int uiFunctionPointer;
	unsigned int uiValues;
	unsigned int uiNumCounters;
	unsigned short ausNumValueSites[3];	// IPVK_Last + 1
	unsigned short usOffloadDeviceWaveSize;
	unsigned int uiNumBitmapBytes;
};
static_assert(sizeof(BridgedPgoData) == 56, "the function record of the raw profile is 56 bytes on i386");

// The linker puts each section's parts in the order of what follows the $, so these stand
// before and after everything the compiler put in ($M).
#pragma section(".lprfd$A", read, write)
#pragma section(".lprfd$Z", read, write)
#pragma section(".lprfc$A", read, write)
#pragma section(".lprfc$Z", read, write)
#pragma section(".lprfn$A", read)
#pragma section(".lprfn$Z", read)
__declspec(allocate(".lprfd$A")) __declspec(align(8)) BridgedPgoData g_bridgedPgoDataStart = { 0 };
__declspec(allocate(".lprfd$Z")) __declspec(align(8)) BridgedPgoData g_bridgedPgoDataEnd = { 0 };
__declspec(allocate(".lprfc$A")) __declspec(align(8)) unsigned long long g_uiBridgedPgoCountersStart = 0;
__declspec(allocate(".lprfc$Z")) __declspec(align(8)) unsigned long long g_uiBridgedPgoCountersEnd = 0;
__declspec(allocate(".lprfn$A")) extern const char g_cBridgedPgoNamesStart = 1;
__declspec(allocate(".lprfn$Z")) extern const char g_cBridgedPgoNamesEnd = 2;

extern "C"
{
	int __llvm_profile_runtime = 0;						// instrumented code refers to it to pull the runtime in
	// Set by the compiler in every instrumented unit: the format version and what kind of
	// profile it is. Declared under its link name, because a declaration under its own name
	// in a unit that is itself instrumented collides with the one the compiler adds.
	extern unsigned long long g_uiBridgedPgoRawVersion __asm__("___llvm_profile_raw_version");
}

void bridgedPgoTurn(int iGameTurn)
{
	const char* szPath = getenv("BRIDGED_PGO_RAW");
	if (szPath == NULL || (iGameTurn % 10) != 0)
	{
		return;
	}

	const BridgedPgoData* pDataBegin = &g_bridgedPgoDataStart + 1;
	const BridgedPgoData* pDataEnd = &g_bridgedPgoDataEnd;
	const unsigned long long* pCountersBegin = &g_uiBridgedPgoCountersStart + 1;
	const unsigned long long* pCountersEnd = &g_uiBridgedPgoCountersEnd;
	const char* pNamesBegin = &g_cBridgedPgoNamesStart + 1;
	const char* pNamesEnd = &g_cBridgedPgoNamesEnd;

	const unsigned int uiDataBytes = (unsigned int)((const char*)pDataEnd - (const char*)pDataBegin);
	const unsigned int uiNumData = uiDataBytes / sizeof(BridgedPgoData);
	const unsigned int uiNumCounters = (unsigned int)(pCountersEnd - pCountersBegin);
	const unsigned int uiNamesSize = (unsigned int)(pNamesEnd - pNamesBegin);

	// What this writer leaves out must not be there.
	const char* szProblem = NULL;
	if ((uiDataBytes % sizeof(BridgedPgoData)) != 0)
	{
		szProblem = "the record section is not a whole number of records";
	}
	for (unsigned int uiI = 0; uiI < uiNumData && szProblem == NULL; uiI++)
	{
		const BridgedPgoData& kData = pDataBegin[uiI];
		if (kData.ausNumValueSites[0] != 0 || kData.ausNumValueSites[1] != 0 || kData.ausNumValueSites[2] != 0)
		{
			szProblem = "a function has value sites: build with -mllvm -disable-vp";
		}
		else if (kData.uiNumBitmapBytes != 0 || kData.uiUniformCounterPtr != 0)
		{
			szProblem = "a function has bitmap bytes or uniform counters";
		}
	}

	FILE* pFile = fopen(szPath, "wb");
	if (pFile == NULL)
	{
		return;
	}
	if (szProblem != NULL)
	{
		fprintf(pFile, "no profile written: %s\n", szProblem);
		fclose(pFile);
		return;
	}

	// INSTR_PROF_RAW_HEADER of LLVM 23.1.2. The deltas are worked out in 32 bits, as the
	// runtime library does for a 32-bit program.
	const unsigned long long auiHeader[19] = {
		((unsigned long long)255 << 56) | ((unsigned long long)'l' << 48) | ((unsigned long long)'p' << 40) |
			((unsigned long long)'r' << 32) | ((unsigned long long)'o' << 24) | ((unsigned long long)'f' << 16) |
			((unsigned long long)'R' << 8) | (unsigned long long)129,	// Magic, 32-bit
		g_uiBridgedPgoRawVersion,										// Version
		0,																// BinaryIdsSize
		uiNumData,														// NumData
		0,																// PaddingBytesBeforeCounters
		uiNumCounters,													// NumCounters
		0,																// PaddingBytesAfterCounters
		0,																// NumBitmapBytes
		0,																// PaddingBytesAfterBitmapBytes
		0,																// NumUniformCounters
		0,																// PaddingBytesAfterUniformCounters
		0,																// UniformCountersDelta
		uiNamesSize,													// NamesSize
		(unsigned int)((unsigned int)pCountersBegin - (unsigned int)pDataBegin),	// CountersDelta
		0,																// BitmapDelta
		(unsigned int)pNamesBegin,										// NamesDelta
		0,																// NumVTables
		0,																// VNamesSize
		2,																// ValueKindLast
	};

	static const char acPadding[8] = { 0 };

	fwrite(auiHeader, sizeof(auiHeader), 1, pFile);
	fwrite(pDataBegin, sizeof(BridgedPgoData), uiNumData, pFile);
	fwrite(pCountersBegin, sizeof(unsigned long long), uiNumCounters, pFile);
	fwrite(pNamesBegin, 1, uiNamesSize, pFile);
	fwrite(acPadding, 1, (8 - (uiNamesSize % 8)) % 8, pFile);
	fclose(pFile);
}
#endif	// BRIDGED_PGO_TRAIN
