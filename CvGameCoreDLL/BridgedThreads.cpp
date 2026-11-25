#include "CvGameCoreDLL.h"
#include "BridgedThreads.h"
#include "CvGlobals.h"

//
// The pool behind BridgedThreads::parallelFor(): see BridgedThreads.h.
//
// The workers are made the first time they are needed and live as long as the process. Each
// waits on a semaphore; a burst sets out the range, releases the semaphore once for each
// worker, and works on the range itself. A piece is taken by an interlocked add on the next
// index, so no two threads take the same one. The last thread to finish sets an event the
// main thread waits on. The interlocked operations are full barriers: what the main thread
// wrote before the burst is seen by the workers, and what they wrote is seen after it.
//
namespace
{
	const int MAX_WORKERS = 15;

	DWORD s_uiTlsIndex = TLS_OUT_OF_INDEXES;	// per thread: 0 not in a burst, 1 the main thread in one, 2 + n worker n
	HANDLE s_hWorkerHeap = NULL;
	HANDLE s_hStart = NULL;		// semaphore: one release per worker per burst
	HANDLE s_hDone = NULL;		// event: the burst's last thread has finished
	CRITICAL_SECTION s_kLock;
	int s_iWorkers = -1;		// -1 until the pool is set up
	int s_iBackgroundWorkers = 0;	// how many of them a burst in the background uses
	HANDLE s_ahThreads[MAX_WORKERS + 1];
	int s_iPriority = THREAD_PRIORITY_NORMAL;	// the workers' now

	// A burst in the background gives way to the game: the workers below normal priority, so that
	// the exe's threads (drawing, sound, input) come first. A burst the main thread waits for has
	// them at normal priority.
	void setWorkerPriority(int iPriority)
	{
		if (iPriority == s_iPriority)
		{
			return;
		}

		for (int iI = 1; iI <= s_iWorkers; iI++)
		{
			SetThreadPriority(s_ahThreads[iI], iPriority);
		}

		s_iPriority = iPriority;
	}
	BridgedThreads::Need s_aNeed[MAX_WORKERS + 1];	// per thread: see needMainThread()

	// the burst under way
	BridgedThreads::RangeFunction s_pfnRange = NULL;
	void* s_pContext = NULL;
	int s_iCount = 0;
	int s_iChunk = 1;
	volatile long s_lNext = 0;		// the next index to hand out
	volatile long s_lRunning = 0;	// threads still working, the main thread included
	volatile long s_lStop = 0;		// requestStop(): no more pieces are started
	bool s_bBackground = false;		// a burst in the background is under way (the main thread's to know)

	void runPieces(int iThread)
	{
		while (s_lStop == 0)
		{
			const long lBegin = InterlockedExchangeAdd(&s_lNext, s_iChunk);

			if (lBegin >= s_iCount)
			{
				break;
			}

			s_pfnRange(s_pContext, (int)lBegin, std::min(s_iCount, (int)lBegin + s_iChunk), iThread);
		}

		if (InterlockedDecrement(&s_lRunning) == 0)
		{
			SetEvent(s_hDone);
		}
	}

	DWORD WINAPI workerMain(LPVOID pParam)
	{
		const int iThread = (int)(size_t)pParam;
		TlsSetValue(s_uiTlsIndex, (LPVOID)(size_t)(iThread + 1));

		while (true)
		{
			WaitForSingleObject(s_hStart, INFINITE);
			runPieces(iThread);
		}

		return 0;
	}

	void setUp()
	{
		s_iWorkers = 0;

		if (GC.getDefineINT("WORKER_THREADS_OFF") != 0)
		{
			return;
		}

		int iWanted = GC.getDefineINT("WORKER_THREADS");
		if (iWanted <= 0)
		{
			SYSTEM_INFO kInfo;
			GetSystemInfo(&kInfo);
			iWanted = (int)kInfo.dwNumberOfProcessors - 1;
		}
		iWanted = std::min(MAX_WORKERS, iWanted);

		if (iWanted <= 0)
		{
			return;
		}

		s_uiTlsIndex = TlsAlloc();
		s_hWorkerHeap = HeapCreate(0, 0, 0);
		s_hStart = CreateSemaphoreA(NULL, 0, MAX_WORKERS, NULL);
		s_hDone = CreateEventA(NULL, FALSE, FALSE, NULL);

		if ((s_uiTlsIndex == TLS_OUT_OF_INDEXES) || (s_hWorkerHeap == NULL) || (s_hStart == NULL) || (s_hDone == NULL))
		{
			return;
		}

		InitializeCriticalSection(&s_kLock);

		for (int iI = 1; iI <= iWanted; iI++)
		{
			DWORD uiThreadId;
			HANDLE hThread = CreateThread(NULL, 0, workerMain, (LPVOID)(size_t)iI, 0, &uiThreadId);

			if (hThread == NULL)
			{
				break;
			}

			s_ahThreads[iI] = hThread;
			s_iWorkers = iI;
		}

		// WORKER_THREADS_BACKGROUND: how many work in the background (unset or 0: one fewer than
		// work in a burst the main thread waits for, which leaves the exe a processor of its own)
		s_iBackgroundWorkers = GC.getDefineINT("WORKER_THREADS_BACKGROUND");
		if (s_iBackgroundWorkers <= 0)
		{
			s_iBackgroundWorkers = s_iWorkers - 1;
		}
		s_iBackgroundWorkers = std::max(1, std::min(s_iWorkers, s_iBackgroundWorkers));
	}
}

volatile long BridgedThreads::g_lBurstActive = 0;

int BridgedThreads::workerCount()
{
	if (s_iWorkers < 0)
	{
		setUp();
	}

	return s_iWorkers;
}

bool BridgedThreads::isWorkerThread()
{
	return (s_iWorkers > 0) && ((size_t)TlsGetValue(s_uiTlsIndex) >= 2);
}

bool BridgedThreads::isInBurst()
{
	return (g_lBurstActive != 0) && (s_iWorkers > 0) && ((size_t)TlsGetValue(s_uiTlsIndex) >= 1);
}

int BridgedThreads::threadIndex()
{
	if (s_iWorkers <= 0)
	{
		return 0;
	}

	const size_t uiValue = (size_t)TlsGetValue(s_uiTlsIndex);
	return ((uiValue >= 2) ? (int)(uiValue - 1) : 0);
}

void BridgedThreads::needMainThread(int eType, int iData1, int iData2)
{
	Need& kNeed = s_aNeed[threadIndex()];

	if (kNeed.eType == NEED_NONE)
	{
		kNeed.eType = eType;
		kNeed.iData1 = iData1;
		kNeed.iData2 = iData2;
	}
}

bool BridgedThreads::hasNeedMainThread()
{
	return (s_aNeed[threadIndex()].eType != NEED_NONE);
}

bool BridgedThreads::takeNeedMainThread(Need* pNeed)
{
	Need& kNeed = s_aNeed[threadIndex()];
	const bool bNeed = (kNeed.eType != NEED_NONE);

	if (pNeed != NULL)
	{
		*pNeed = kNeed;
	}

	kNeed.eType = NEED_NONE;
	return bNeed;
}

void BridgedThreads::lock()
{
	if (s_iWorkers > 0)
	{
		EnterCriticalSection(&s_kLock);
	}
}

void BridgedThreads::unlock()
{
	if (s_iWorkers > 0)
	{
		LeaveCriticalSection(&s_kLock);
	}
}

void* BridgedThreads::workerAllocate(size_t uiSize)
{
	return HeapAlloc(s_hWorkerHeap, 0, (uiSize > 0) ? uiSize : 1);
}

void BridgedThreads::workerFree(void* p)
{
	if (p != NULL)
	{
		HeapFree(s_hWorkerHeap, 0, p);
	}
}

bool BridgedThreads::isBurstUnderWay()
{
	return (g_lBurstActive != 0) && (s_iWorkers > 0);
}

void BridgedThreads::requestStop()
{
	InterlockedExchange(&s_lStop, 1);
}

bool BridgedThreads::isStopRequested()
{
	return (s_lStop != 0);
}

bool BridgedThreads::startInBackground(int iCount, int iChunk, RangeFunction fn, void* pContext)
{
	if ((iCount <= 0) || (workerCount() == 0) || (g_lBurstActive != 0))
	{
		return false;
	}

	s_pfnRange = fn;
	s_pContext = pContext;
	s_iCount = iCount;
	s_iChunk = std::max(1, iChunk);
	s_lNext = 0;
	s_lStop = 0;
	s_lRunning = s_iBackgroundWorkers;
	ResetEvent(s_hDone);
	s_bBackground = true;
	setWorkerPriority(THREAD_PRIORITY_BELOW_NORMAL);

	InterlockedExchange(&g_lBurstActive, 1);
	ReleaseSemaphore(s_hStart, s_iBackgroundWorkers, NULL);
	return true;
}

bool BridgedThreads::isBackgroundUnderWay()
{
	return s_bBackground;
}

bool BridgedThreads::isBackgroundDone()
{
	if (!s_bBackground)
	{
		return true;
	}

	if (WaitForSingleObject(s_hDone, 0) != WAIT_OBJECT_0)
	{
		return false;
	}

	s_bBackground = false;
	InterlockedExchange(&g_lBurstActive, 0);
	return true;
}

bool BridgedThreads::waitBackgroundFor(unsigned int uiMs)
{
	if (!s_bBackground)
	{
		return true;
	}

	if (WaitForSingleObject(s_hDone, uiMs) != WAIT_OBJECT_0)
	{
		return false;
	}

	s_bBackground = false;
	InterlockedExchange(&g_lBurstActive, 0);
	return true;
}

void BridgedThreads::waitBackground()
{
	if (s_bBackground)
	{
		WaitForSingleObject(s_hDone, INFINITE);
		s_bBackground = false;
		InterlockedExchange(&g_lBurstActive, 0);
	}
}

void BridgedThreads::parallelFor(int iCount, int iChunk, RangeFunction fn, void* pContext)
{
	if (iCount <= 0)
	{
		return;
	}

	// no workers, or called from within a burst: the whole range here
	if ((workerCount() == 0) || (g_lBurstActive != 0))
	{
		fn(pContext, 0, iCount, 0);
		return;
	}

	s_pfnRange = fn;
	s_pContext = pContext;
	s_iCount = iCount;
	s_iChunk = std::max(1, iChunk);
	s_lNext = 0;
	s_lStop = 0;
	s_lRunning = s_iWorkers + 1;
	ResetEvent(s_hDone);
	setWorkerPriority(THREAD_PRIORITY_NORMAL);

	TlsSetValue(s_uiTlsIndex, (LPVOID)1);
	InterlockedExchange(&g_lBurstActive, 1);

	ReleaseSemaphore(s_hStart, s_iWorkers, NULL);
	runPieces(0);
	WaitForSingleObject(s_hDone, INFINITE);

	InterlockedExchange(&g_lBurstActive, 0);
	TlsSetValue(s_uiTlsIndex, (LPVOID)0);
}
