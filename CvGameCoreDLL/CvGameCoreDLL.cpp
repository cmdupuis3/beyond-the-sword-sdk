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

BridgedProfileSample* bridgedProfilePythonSample(const char* szKind, const char* szModule, const char* szName)
{
	// The rows are found by their name, not by the address of szName: most callers pass a
	// string literal, but an event's name is in a Python string that is made for the call.
	// No memory is taken from the heap, so that a profile build asks for the same blocks
	// in the same order as a release build.
	enum
	{
		NUM_ROWS = 512,		// a power of 2; the stock game calls about 120 functions and events
	};
	static BridgedProfileSample aRows[NUM_ROWS];
	static BridgedProfileSample kOther = { "python: calls with no row left for them" };

	// the module without "Cv" in front and "Interface" behind
	if (szModule == NULL)
	{
		szModule = "";
	}
	if ((szModule[0] == 'C') && (szModule[1] == 'v'))
	{
		szModule += 2;
	}
	int iModuleLength = (int)strlen(szModule);
	if ((iModuleLength > 9) && (strcmp(szModule + iModuleLength - 9, "Interface") == 0))
	{
		iModuleLength -= 9;
	}

	char szRow[sizeof(aRows[0].szNameCopy)];
	if (iModuleLength > 0)
	{
		_snprintf(szRow, sizeof(szRow), "%s%.*s.%s", szKind, iModuleLength, szModule, ((szName != NULL) ? szName : "?"));
	}
	else
	{
		_snprintf(szRow, sizeof(szRow), "%s%s", szKind, ((szName != NULL) ? szName : "?"));
	}
	szRow[sizeof(szRow) - 1] = '\0';

	unsigned int uiHash = 2166136261u;
	for (char* pChar = szRow; *pChar != '\0'; pChar++)
	{
		// the name as bridgedProfileRegister() keeps it, so that it can be compared below
		if ((*pChar < ' ') || (*pChar > '~'))
		{
			*pChar = '?';
		}
		uiHash = (uiHash ^ (unsigned char)(*pChar)) * 16777619u;
	}

	unsigned int uiSlot = uiHash & (NUM_ROWS - 1);

	for (int iProbe = 0; iProbe < NUM_ROWS; iProbe++)
	{
		BridgedProfileSample& kRow = aRows[uiSlot];

		if (!kRow.bRegistered)
		{
			kRow.szName = szRow;
			bridgedProfileRegister(&kRow);	// copies the name
			return &kRow;
		}

		if (strcmp(kRow.szNameCopy, szRow) == 0)
		{
			return &kRow;
		}

		uiSlot = (uiSlot + 1) & (NUM_ROWS - 1);
	}

	return &kOther;
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
// The instrumented code also reports two kinds of value as it runs: the function an
// indirect call goes to, and the size of a memory copy or fill whose size is not fixed.
// It does that by calling the runtime library, so the two functions it calls are here
// too. They keep, for each place in the code, a short list of the values seen and how
// often, as the runtime library does.
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
	unsigned int uiValues;				// an array with a list of values for each value site, or 0
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

// a value seen at a value site (ValueProfNode)
struct BridgedPgoNode
{
	unsigned long long uiValue;
	unsigned long long uiCount;
	BridgedPgoNode* pNext;
};

static const unsigned int BRIDGED_PGO_SITE_VALUES = 16;	// kept for each site, as the runtime library keeps by default
static const int BRIDGED_PGO_VALUE_KINDS = 3;

static unsigned int bridgedPgoNumSites(const BridgedPgoData& kData)
{
	return (unsigned int)kData.ausNumValueSites[0] + kData.ausNumValueSites[1] + kData.ausNumValueSites[2];
}

extern "C"
{
	//
	// Called by instrumented code at an indirect call, with the address called. The compiler
	// gives every function that has value sites an array of list heads, one for each site.
	// A new value takes the place of the least seen one when the list is full and that one
	// has only been seen once; otherwise the least seen one loses a count.
	//
	// These two must not be instrumented themselves: they would call themselves.
	//
	__attribute__((no_profile_instrument_function))
	void __llvm_profile_instrument_target(unsigned long long uiValue, void* pData, unsigned int uiSite)
	{
		BridgedPgoData* pRecord = (BridgedPgoData*)pData;

		if (pRecord == NULL || uiSite >= bridgedPgoNumSites(*pRecord))
		{
			return;
		}

		if (pRecord->uiValues == 0)
		{
			pRecord->uiValues = (unsigned int)calloc(bridgedPgoNumSites(*pRecord), sizeof(BridgedPgoNode*));
			if (pRecord->uiValues == 0)
			{
				return;
			}
		}

		BridgedPgoNode** ppHead = &(((BridgedPgoNode**)pRecord->uiValues)[uiSite]);
		BridgedPgoNode* pLast = NULL;
		BridgedPgoNode* pLeast = NULL;
		unsigned int uiNodes = 0;

		for (BridgedPgoNode* pNode = *ppHead; pNode != NULL; pNode = pNode->pNext)
		{
			if (pNode->uiValue == uiValue)
			{
				pNode->uiCount++;
				return;
			}
			if (pLeast == NULL || pNode->uiCount < pLeast->uiCount)
			{
				pLeast = pNode;
			}
			pLast = pNode;
			uiNodes++;
		}

		if (uiNodes >= BRIDGED_PGO_SITE_VALUES)
		{
			if (pLeast->uiCount <= 1)
			{
				pLeast->uiValue = uiValue;
				pLeast->uiCount = 1;
			}
			else
			{
				pLeast->uiCount--;
			}
			return;
		}

		BridgedPgoNode* pNew = (BridgedPgoNode*)malloc(sizeof(BridgedPgoNode));
		if (pNew == NULL)
		{
			return;
		}
		pNew->uiValue = uiValue;
		pNew->uiCount = 1;
		pNew->pNext = NULL;
		if (pLast == NULL)
		{
			*ppHead = pNew;
		}
		else
		{
			pLast->pNext = pNew;
		}
	}

	// Called with the size of a memory copy or fill. Sizes are counted in the ranges of
	// InstrProfGetRangeRepValue(): up to 8 each on its own, a power of two on its own, the
	// sizes between two powers of two together, and everything from 513 up together.
	__attribute__((no_profile_instrument_function))
	void __llvm_profile_instrument_memop(unsigned long long uiValue, void* pData, unsigned int uiSite)
	{
		if (uiValue >= 513)
		{
			uiValue = 513;
		}
		else if (uiValue > 8 && (uiValue & (uiValue - 1)) != 0)
		{
			unsigned long long uiPower = 8;
			while ((uiPower * 2) < uiValue)
			{
				uiPower *= 2;
			}
			uiValue = uiPower + 1;
		}
		__llvm_profile_instrument_target(uiValue, pData, uiSite);
	}

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
		if (kData.uiNumBitmapBytes != 0 || kData.uiUniformCounterPtr != 0)
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

	// The values, for each function that has value sites, in the order of the records: a
	// ValueProfData, which is its size and the number of kinds, and for each kind that has
	// sites a ValueProfRecord: the kind, the number of sites, a byte for each site with the
	// number of values kept there, padding to eight bytes, and then the values and counts.
	// A function's sites are numbered through the kinds in order.
	for (unsigned int uiI = 0; uiI < uiNumData; uiI++)
	{
		const BridgedPgoData& kData = pDataBegin[uiI];
		BridgedPgoNode* const* ppSites = (BridgedPgoNode* const*)kData.uiValues;
		unsigned int uiKinds = 0;
		unsigned int uiTotalSize = 8;
		unsigned int uiFirstSite = 0;

		for (int iKind = 0; iKind < BRIDGED_PGO_VALUE_KINDS; iKind++)
		{
			const unsigned int uiSites = kData.ausNumValueSites[iKind];
			if (uiSites != 0)
			{
				uiKinds++;
				uiTotalSize += ((8 + uiSites + 7) & ~7u);
				for (unsigned int uiSite = 0; uiSite < uiSites && ppSites != NULL; uiSite++)
				{
					unsigned int uiNodes = 0;
					for (const BridgedPgoNode* pNode = ppSites[uiFirstSite + uiSite]; pNode != NULL && uiNodes < 255; pNode = pNode->pNext)
					{
						uiNodes++;
					}
					uiTotalSize += 16 * uiNodes;
				}
			}
			uiFirstSite += uiSites;
		}

		if (uiKinds == 0)
		{
			continue;
		}

		const unsigned int auiValueHeader[2] = { uiTotalSize, uiKinds };
		fwrite(auiValueHeader, sizeof(auiValueHeader), 1, pFile);

		uiFirstSite = 0;
		for (int iKind = 0; iKind < BRIDGED_PGO_VALUE_KINDS; iKind++)
		{
			const unsigned int uiSites = kData.ausNumValueSites[iKind];
			if (uiSites != 0)
			{
				const unsigned int auiRecordHeader[2] = { (unsigned int)iKind, uiSites };
				fwrite(auiRecordHeader, sizeof(auiRecordHeader), 1, pFile);

				for (unsigned int uiSite = 0; uiSite < uiSites; uiSite++)
				{
					unsigned char ucNodes = 0;
					for (const BridgedPgoNode* pNode = ((ppSites != NULL) ? ppSites[uiFirstSite + uiSite] : NULL); pNode != NULL && ucNodes < 255; pNode = pNode->pNext)
					{
						ucNodes++;
					}
					fwrite(&ucNodes, 1, 1, pFile);
				}
				fwrite(acPadding, 1, ((8 + uiSites + 7) & ~7u) - (8 + uiSites), pFile);

				for (unsigned int uiSite = 0; uiSite < uiSites && ppSites != NULL; uiSite++)
				{
					unsigned int uiNodes = 0;
					for (const BridgedPgoNode* pNode = ppSites[uiFirstSite + uiSite]; pNode != NULL && uiNodes < 255; pNode = pNode->pNext)
					{
						fwrite(pNode, 16, 1, pFile);	// the value and its count
						uiNodes++;
					}
				}
			}
			uiFirstSite += uiSites;
		}
	}

	fclose(pFile);
}
#endif	// BRIDGED_PGO_TRAIN
