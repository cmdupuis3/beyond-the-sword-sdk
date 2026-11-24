#include "CvGameCoreDLL.h"

#include "CvGameCoreDLLUndefNew.h"

#include <new>

#include "CvGlobals.h"
#include "FProfiler.h"
#include "CvDLLInterfaceIFaceBase.h"
#include "BridgedThreads.h"

// a worker thread in a burst allocates from the DLL's own heap: see BridgedThreads.h
static inline bool onBurstWorker()
{
	return (BridgedThreads::g_lBurstActive != 0) && BridgedThreads::isWorkerThread();
}

//
// operator global new and delete override for gamecore DLL 
//
void *__cdecl operator new(size_t size)
{
	if (onBurstWorker())
	{
		return BridgedThreads::workerAllocate(size);
	}
	if (gDLL)
	{
		return gDLL->newMem(size, __FILE__, __LINE__);
	}
	return malloc(size);
}

void __cdecl operator delete (void *p)
{
	if (onBurstWorker())
	{
		BridgedThreads::workerFree(p);
		return;
	}
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
	if (onBurstWorker())
	{
		return BridgedThreads::workerAllocate(size);
	}
	if (gDLL)
		return gDLL->newMemArray(size, __FILE__, __LINE__);
	return malloc(size);
}

void operator delete[](void* pvMem)
{
	if (onBurstWorker())
	{
		BridgedThreads::workerFree(pvMem);
		return;
	}
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
	if (onBurstWorker())
	{
		return BridgedThreads::workerAllocate(size);
	}
	return gDLL->newMem(size, pcFile, iLine);
}

void *__cdecl operator new[](size_t size, char* pcFile, int iLine)
{
	if (onBurstWorker())
	{
		return BridgedThreads::workerAllocate(size);
	}
	return gDLL->newMem(size, pcFile, iLine);
}

void __cdecl operator delete(void* pvMem, char* pcFile, int iLine)
{
	if (onBurstWorker())
	{
		BridgedThreads::workerFree(pvMem);
		return;
	}
	gDLL->delMem(pvMem, pcFile, iLine);
}

void __cdecl operator delete[](void* pvMem, char* pcFile, int iLine)
{
	if (onBurstWorker())
	{
		BridgedThreads::workerFree(pvMem);
		return;
	}
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
// Where the exe's time goes between two calls of CvGame::update().
//
// The exe has no PROFILE scopes and cannot be read (the Steam one is encrypted on disk). So
// while the exe has the game's thread, a second thread stops it about once a millisecond
// and notes where it is: in which module, whether Python is on its stack, and which
// PROFILE scope is open if the exe has called back into this DLL. The counts are written
// with the log, to the file named by BRIDGED_PROFILE_LOG with ".exe.tsv" added.
//
// Between stopping the thread and letting it go on, the sampler only reads memory: the
// stopped thread may hold the heap or loader lock.
//
struct BridgedExeModule
{
	DWORD uiBase;
	DWORD uiEnd;
	char szName[48];
	unsigned int uiSamples;
	unsigned int uiPythonSamples;	// of those, with Python on the stack
	unsigned int uiScopeSamples;	// of those, with a PROFILE scope of this DLL open
};

struct BridgedExeAddress
{
	DWORD uiAddress;
	unsigned int uiSamples;
	unsigned int uiPythonSamples;
};

struct BridgedExeScope
{
	const BridgedProfileSample* pSample;
	unsigned int uiSamples;
};

static const int BRIDGED_EXE_MODULES = 96;
static const int BRIDGED_EXE_ADDRESS_BITS = 15;
static const int BRIDGED_EXE_SCOPES = 256;
static const DWORD BRIDGED_EXE_STACK_BYTES = 0x10000;

static volatile LONG g_lBridgedExeSampling = 0;
static HANDLE g_hBridgedExeThread = NULL;		// the game's thread
static DWORD g_uiBridgedExeStackBase = 0;
static DWORD g_auiBridgedExePythonText[2] = { 0, 0 };
static BridgedExeModule g_aBridgedExeModules[BRIDGED_EXE_MODULES];
static int g_iBridgedExeModules = 0;
static BridgedExeAddress g_aBridgedExeAddresses[1 << BRIDGED_EXE_ADDRESS_BITS];
static BridgedExeScope g_aBridgedExeScopes[BRIDGED_EXE_SCOPES];
static unsigned int g_uiBridgedExeSamples = 0;
static unsigned int g_uiBridgedExeNoScopeSamples = 0;
static unsigned int g_uiBridgedExeDropped = 0;

// Each sample's stack: the words on it that point into the code of the exe or of this DLL,
// from the top down. Most are return addresses, so they say what the exe was doing, and which
// function of this DLL it had called. Written raw, to be counted outside the game.
static const int BRIDGED_EXE_STACK_WORDS = 62;
static const int BRIDGED_EXE_STACKS = 30000;
struct BridgedExeStack
{
	DWORD uiAddress;
	DWORD uiWords;
	DWORD auiWords[BRIDGED_EXE_STACK_WORDS];
};
static BridgedExeStack g_aBridgedExeStacks[BRIDGED_EXE_STACKS];
static volatile LONG g_lBridgedExeStacks = 0;
static DWORD g_auiBridgedExeExeText[2] = { 0, 0 };
static DWORD g_auiBridgedExeDllText[2] = { 0, 0 };
static DWORD g_uiBridgedExeDllBase = 0;

// The code section of a loaded module, from its headers.
static void bridgedExeTextRange(DWORD uiBase, DWORD* puiStart, DWORD* puiEnd)
{
	const IMAGE_DOS_HEADER* pDos = (const IMAGE_DOS_HEADER*)uiBase;
	const IMAGE_NT_HEADERS* pNt = (const IMAGE_NT_HEADERS*)(uiBase + pDos->e_lfanew);
	*puiStart = uiBase + pNt->OptionalHeader.BaseOfCode;
	*puiEnd = *puiStart + pNt->OptionalHeader.SizeOfCode;
}

static int bridgedExeModule(DWORD uiAddress)
{
	for (int iI = 0; iI < g_iBridgedExeModules; iI++)
	{
		if (uiAddress >= g_aBridgedExeModules[iI].uiBase && uiAddress < g_aBridgedExeModules[iI].uiEnd)
		{
			return iI;
		}
	}
	if (g_iBridgedExeModules == BRIDGED_EXE_MODULES)
	{
		return -1;
	}

	BridgedExeModule& kModule = g_aBridgedExeModules[g_iBridgedExeModules];
	memset(&kModule, 0, sizeof(kModule));

	MEMORY_BASIC_INFORMATION kInfo;
	char szPath[MAX_PATH];
	if (VirtualQuery((LPCVOID)uiAddress, &kInfo, sizeof(kInfo)) == 0)
	{
		return -1;
	}
	if (kInfo.Type == MEM_IMAGE && GetModuleFileNameA((HMODULE)kInfo.AllocationBase, szPath, MAX_PATH) != 0)
	{
		const IMAGE_DOS_HEADER* pDos = (const IMAGE_DOS_HEADER*)kInfo.AllocationBase;
		const IMAGE_NT_HEADERS* pNt = (const IMAGE_NT_HEADERS*)((DWORD)kInfo.AllocationBase + pDos->e_lfanew);
		const char* szName = strrchr(szPath, '\\');
		kModule.uiBase = (DWORD)kInfo.AllocationBase;
		kModule.uiEnd = kModule.uiBase + pNt->OptionalHeader.SizeOfImage;
		strncpy(kModule.szName, (szName != NULL) ? (szName + 1) : szPath, sizeof(kModule.szName) - 1);
	}
	else
	{
		// code that is in no module: made at run time
		kModule.uiBase = (DWORD)kInfo.AllocationBase;
		kModule.uiEnd = (DWORD)kInfo.BaseAddress + (DWORD)kInfo.RegionSize;
		strncpy(kModule.szName, "(no module)", sizeof(kModule.szName) - 1);
	}
	return g_iBridgedExeModules++;
}

static DWORD WINAPI bridgedExeSampler(LPVOID)
{
	for (;;)
	{
		Sleep(1);
		if (g_lBridgedExeSampling == 0)
		{
			continue;
		}
		if (SuspendThread(g_hBridgedExeThread) == (DWORD)-1)
		{
			continue;
		}

		CONTEXT kContext;
		kContext.ContextFlags = CONTEXT_CONTROL;
		const bool bSampled = ((g_lBridgedExeSampling != 0) && GetThreadContext(g_hBridgedExeThread, &kContext));
		const BridgedProfileSample* pScope = NULL;
		int iPythonPointers = 0;
		BridgedExeStack* pStack = ((g_lBridgedExeStacks < BRIDGED_EXE_STACKS) ? &g_aBridgedExeStacks[g_lBridgedExeStacks] : NULL);
		if (pStack != NULL)
		{
			pStack->uiWords = 0;
		}
		if (bSampled)
		{
			pScope = g_pBridgedProfileCurrent;
			if (kContext.Esp < g_uiBridgedExeStackBase && (g_uiBridgedExeStackBase - kContext.Esp) < 0x100000)
			{
				const DWORD* pWord = (const DWORD*)(kContext.Esp & ~3u);
				const DWORD* pEnd = (const DWORD*)std::min(g_uiBridgedExeStackBase, (kContext.Esp & ~3u) + BRIDGED_EXE_STACK_BYTES);
				for (; pWord < pEnd; pWord++)
				{
					if (*pWord >= g_auiBridgedExePythonText[0] && *pWord < g_auiBridgedExePythonText[1])
					{
						iPythonPointers++;
					}
					if (pStack != NULL && pStack->uiWords < (DWORD)BRIDGED_EXE_STACK_WORDS &&
						((*pWord >= g_auiBridgedExeExeText[0] && *pWord < g_auiBridgedExeExeText[1]) || (*pWord >= g_auiBridgedExeDllText[0] && *pWord < g_auiBridgedExeDllText[1])))
					{
						pStack->auiWords[pStack->uiWords++] = *pWord;
					}
				}
			}
		}
		ResumeThread(g_hBridgedExeThread);

		if (!bSampled)
		{
			continue;
		}

		const bool bPython = (iPythonPointers >= 2);
		if (pStack != NULL)
		{
			pStack->uiAddress = kContext.Eip;
			g_lBridgedExeStacks++;
		}
		g_uiBridgedExeSamples++;

		const int iModule = bridgedExeModule(kContext.Eip);
		if (iModule != -1)
		{
			g_aBridgedExeModules[iModule].uiSamples++;
			if (bPython)
			{
				g_aBridgedExeModules[iModule].uiPythonSamples++;
			}
			if (pScope != NULL)
			{
				g_aBridgedExeModules[iModule].uiScopeSamples++;
			}
		}

		if (pScope == NULL)
		{
			g_uiBridgedExeNoScopeSamples++;
		}
		else
		{
			for (int iI = 0; iI < BRIDGED_EXE_SCOPES; iI++)
			{
				if (g_aBridgedExeScopes[iI].pSample == pScope || g_aBridgedExeScopes[iI].pSample == NULL)
				{
					g_aBridgedExeScopes[iI].pSample = pScope;
					g_aBridgedExeScopes[iI].uiSamples++;
					break;
				}
			}
		}

		bool bKept = false;
		unsigned int uiSlot = (kContext.Eip * 2654435761u) >> (32 - BRIDGED_EXE_ADDRESS_BITS);
		for (int iI = 0; iI < 64; iI++)
		{
			BridgedExeAddress& kAddress = g_aBridgedExeAddresses[uiSlot];
			if (kAddress.uiAddress == kContext.Eip || kAddress.uiAddress == 0)
			{
				kAddress.uiAddress = kContext.Eip;
				kAddress.uiSamples++;
				if (bPython)
				{
					kAddress.uiPythonSamples++;
				}
				bKept = true;
				break;
			}
			uiSlot = (uiSlot + 1) & ((1u << BRIDGED_EXE_ADDRESS_BITS) - 1);
		}
		if (!bKept)
		{
			g_uiBridgedExeDropped++;
		}
	}
}

// One row for each time the exe had the thread: how long the call of CvGame::update() before
// it took, and how long the exe then kept the thread.
struct BridgedExeFrame
{
	int iGameTurn;
	int iUnits;
	unsigned int uiCallMs;
	unsigned long long uiExeTicks;
};
static const int BRIDGED_EXE_FRAMES = 8192;
static BridgedExeFrame g_aBridgedExeFrames[BRIDGED_EXE_FRAMES];
static int g_iBridgedExeFrames = 0;

void bridgedProfileExeFrame(int iGameTurn, int iUnits, unsigned int uiCallMs, unsigned long long uiExeTicks)
{
	if (g_iBridgedExeFrames < BRIDGED_EXE_FRAMES)
	{
		BridgedExeFrame& kFrame = g_aBridgedExeFrames[g_iBridgedExeFrames++];
		kFrame.iGameTurn = iGameTurn;
		kFrame.iUnits = iUnits;
		kFrame.uiCallMs = uiCallMs;
		kFrame.uiExeTicks = uiExeTicks;
	}
}

// Called by CvGame::update(): false on the way in, true on the way out.
void bridgedProfileExeHasThread(bool bExe)
{
	if (g_hBridgedExeThread == NULL)
	{
		if (getenv("BRIDGED_PROFILE_LOG") == NULL)
		{
			return;
		}
		if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_hBridgedExeThread, THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0))
		{
			g_hBridgedExeThread = NULL;
			return;
		}
		g_uiBridgedExeStackBase = (DWORD)((NT_TIB*)NtCurrentTeb())->StackBase;
		const HMODULE hPython = GetModuleHandleA("python24.dll");
		if (hPython != NULL)
		{
			bridgedExeTextRange((DWORD)hPython, &g_auiBridgedExePythonText[0], &g_auiBridgedExePythonText[1]);
		}
		bridgedExeTextRange((DWORD)GetModuleHandleA(NULL), &g_auiBridgedExeExeText[0], &g_auiBridgedExeExeText[1]);
		MEMORY_BASIC_INFORMATION kInfo;
		if (VirtualQuery((LPCVOID)&bridgedExeTextRange, &kInfo, sizeof(kInfo)) != 0)
		{
			g_uiBridgedExeDllBase = (DWORD)kInfo.AllocationBase;
			bridgedExeTextRange(g_uiBridgedExeDllBase, &g_auiBridgedExeDllText[0], &g_auiBridgedExeDllText[1]);
		}
		DWORD uiThreadId;
		CreateThread(NULL, 0, bridgedExeSampler, NULL, 0, &uiThreadId);
	}
	g_lBridgedExeSampling = (bExe ? 1 : 0);
}

static int __cdecl bridgedExeAddressCompare(const void* pFirst, const void* pSecond)
{
	const unsigned int uiFirst = ((const BridgedExeAddress*)pFirst)->uiSamples;
	const unsigned int uiSecond = ((const BridgedExeAddress*)pSecond)->uiSamples;
	return (uiFirst < uiSecond) ? 1 : ((uiFirst > uiSecond) ? -1 : 0);
}

static void bridgedExeWrite(const char* szLogPath, double fTicksPerMs)
{
	if (g_hBridgedExeThread == NULL || g_uiBridgedExeSamples == 0)
	{
		return;
	}

	{
		char szStackPath[MAX_PATH + 16];
		_snprintf(szStackPath, sizeof(szStackPath) - 1, "%s.stacks.bin", szLogPath);
		szStackPath[sizeof(szStackPath) - 1] = 0;
		FILE* pStackFile = fopen(szStackPath, "wb");
		if (pStackFile != NULL)
		{
			const DWORD auiHeader[8] = { (DWORD)GetModuleHandleA(NULL), g_auiBridgedExeExeText[0], g_auiBridgedExeExeText[1],
				g_uiBridgedExeDllBase, g_auiBridgedExeDllText[0], g_auiBridgedExeDllText[1], (DWORD)g_lBridgedExeStacks, (DWORD)sizeof(BridgedExeStack) };
			fwrite(auiHeader, sizeof(auiHeader), 1, pStackFile);
			fwrite(g_aBridgedExeStacks, sizeof(BridgedExeStack), g_lBridgedExeStacks, pStackFile);
			fclose(pStackFile);
		}
	}

	{
		char szFramePath[MAX_PATH + 16];
		_snprintf(szFramePath, sizeof(szFramePath) - 1, "%s.frames.tsv", szLogPath);
		szFramePath[sizeof(szFramePath) - 1] = 0;
		FILE* pFrameFile = fopen(szFramePath, "w");
		if (pFrameFile != NULL)
		{
			fprintf(pFrameFile, "game_turn\tunits\tcall_ms\texe_ms\n");
			for (int iI = 0; iI < g_iBridgedExeFrames; iI++)
			{
				const BridgedExeFrame& kFrame = g_aBridgedExeFrames[iI];
				fprintf(pFrameFile, "%d\t%d\t%u\t%.1f\n", kFrame.iGameTurn, kFrame.iUnits, kFrame.uiCallMs, (double)kFrame.uiExeTicks / fTicksPerMs);
			}
			fclose(pFrameFile);
		}
	}

	char szPath[MAX_PATH + 16];
	_snprintf(szPath, sizeof(szPath) - 1, "%s.exe.tsv", szLogPath);
	szPath[sizeof(szPath) - 1] = 0;
	FILE* pFile = fopen(szPath, "w");
	if (pFile == NULL)
	{
		return;
	}

	fprintf(pFile, "samples\t%u\tnot_kept_by_address\t%u\n", g_uiBridgedExeSamples, g_uiBridgedExeDropped);
	fprintf(pFile, "\nmodule\tsamples\twith_python_on_stack\twith_dll_scope_open\n");
	const int iModules = g_iBridgedExeModules;
	for (int iI = 0; iI < iModules; iI++)
	{
		const BridgedExeModule& kModule = g_aBridgedExeModules[iI];
		fprintf(pFile, "%s\t%u\t%u\t%u\n", kModule.szName, kModule.uiSamples, kModule.uiPythonSamples, kModule.uiScopeSamples);
	}

	fprintf(pFile, "\ndll_scope_open\tsamples\n(none)\t%u\n", g_uiBridgedExeNoScopeSamples);
	for (int iI = 0; iI < BRIDGED_EXE_SCOPES && g_aBridgedExeScopes[iI].pSample != NULL; iI++)
	{
		fprintf(pFile, "%s\t%u\n", g_aBridgedExeScopes[iI].pSample->szNameCopy, g_aBridgedExeScopes[iI].uiSamples);
	}

	// the addresses sampled most often, as module and offset
	const int iAddresses = 1 << BRIDGED_EXE_ADDRESS_BITS;
	BridgedExeAddress* pAddresses = (BridgedExeAddress*)malloc(iAddresses * sizeof(BridgedExeAddress));
	if (pAddresses != NULL)
	{
		memcpy(pAddresses, g_aBridgedExeAddresses, iAddresses * sizeof(BridgedExeAddress));
		qsort(pAddresses, iAddresses, sizeof(BridgedExeAddress), bridgedExeAddressCompare);
		fprintf(pFile, "\nmodule\toffset\tsamples\twith_python_on_stack\n");
		for (int iI = 0; iI < 400 && pAddresses[iI].uiSamples != 0; iI++)
		{
			const char* szName = "?";
			DWORD uiOffset = pAddresses[iI].uiAddress;
			for (int iJ = 0; iJ < iModules; iJ++)
			{
				if (uiOffset >= g_aBridgedExeModules[iJ].uiBase && uiOffset < g_aBridgedExeModules[iJ].uiEnd)
				{
					szName = g_aBridgedExeModules[iJ].szName;
					uiOffset -= g_aBridgedExeModules[iJ].uiBase;
					break;
				}
			}
			fprintf(pFile, "%s\t%08x\t%u\t%u\n", szName, (unsigned int)uiOffset, pAddresses[iI].uiSamples, pAddresses[iI].uiPythonSamples);
		}
		free(pAddresses);
	}
	fclose(pFile);
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

	// PROFILE_LAST_TURN: a global define the benchmark sets to the last game turn it plays.
	// The game goes on for some seconds before the benchmark stops it, more turns on a small
	// map than on a large one; the log is not written again in them, so that it covers the
	// same turns in every run.
	static int iLastTurn = -1;
	if (iLastTurn == -1)
	{
		iLastTurn = std::max(0, GC.getDefineINT("PROFILE_LAST_TURN"));
	}
	if (iLastTurn > 0 && iGameTurn > iLastTurn)
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

	bridgedExeWrite(szPath, fTicksPerMs);
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
