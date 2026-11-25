#pragma once

#ifndef BRIDGED_THREADS_H
#define BRIDGED_THREADS_H

//
// Work spread over threads in bursts: the main thread hands a range of independent items to
// a pool of worker threads, takes a share of it itself, and waits until all are done. Nothing
// else runs in the game while a burst is under way, so the work only has to be safe against
// itself, not against the game changing under it. A burst in the background
// (startInBackground()) runs on the workers alone while the main thread goes on; the caller
// then keeps the game still until it is done.
//
// What the work may do, on any thread taking part in a burst (isInBurst()):
//  - read the game;
//  - write its own results, into storage made before the burst;
//  - allocate and free memory for its own use. On a worker thread operator new and delete use
//    a heap of the DLL's own (see CvGameCoreDLL.cpp), not the exe's, which is not known to be
//    safe from other threads; memory a worker allocates must be freed before its share ends.
//
// What it may not do:
//  - change the game, or fill a cache whose value depends on when it is filled (a value the
//    stock code works out once on first use and keeps for the turn). Such work is left for the
//    main thread after the burst: see the callers;
//  - call the exe or Python (gDLL, CyArgsList, CvEventReporter), or draw a random number;
//  - start a burst of its own (a nested call runs its range on the calling thread).
// A define looked up for the first time is fetched under a lock (CvGlobals::getDefineINT()),
// and profile scopes do nothing on a worker (FProfiler.h).
//
// The results do not depend on how the work was split: the callers write them back on the
// main thread in a fixed order. WORKER_THREADS sets how many worker threads there are (unset
// or 0, one fewer than the computer's processors), at most 15. WORKER_THREADS_OFF=1 makes
// none: every range runs on the main thread.
//
namespace BridgedThreads
{
	// iBegin to iEnd (excluded) of the range, on the thread numbered iThread: 0 for the main
	// thread, 1 to workerCount() for the workers
	typedef void (*RangeFunction)(void* pContext, int iBegin, int iEnd, int iThread);

	// The number of worker threads, starting them the first time (main thread only).
	int workerCount();

	// Runs fn over 0 to iCount - 1, in pieces of iChunk, on the main thread and the workers,
	// and returns when every piece is done.
	void parallelFor(int iCount, int iChunk, RangeFunction fn, void* pContext);

	// A burst in the background: fn over 0 to iCount - 1 on the workers alone, while the main
	// thread goes on with other work (main thread only). The workers run below normal priority,
	// and one fewer of them than in a burst the main thread waits for (WORKER_THREADS_BACKGROUND
	// sets how many). The main thread is not in the burst
	// (isInBurst() is false there), so it must not change what the work reads: the caller
	// keeps the game still until the burst is done. false if there are no workers or a burst is
	// under way; nothing was started then.
	bool startInBackground(int iCount, int iChunk, RangeFunction fn, void* pContext);
	bool isBackgroundUnderWay();	// main thread
	bool isBackgroundDone();		// main thread: true once every worker has stopped (the burst is then over)
	void waitBackground();			// main thread: until it is done
	bool waitBackgroundFor(unsigned int uiMs);	// main thread: until it is done or the time is up; whether it is done

	// Asks the threads of the burst under way to start no more pieces: a piece already started
	// is finished. Cleared when the next burst starts.
	void requestStop();
	bool isStopRequested();

	// true while a burst, in the background or not, is under way, on any thread
	bool isBurstUnderWay();

	// true on a worker thread
	bool isWorkerThread();

	// true on a thread doing a burst's work: a worker, or the main thread during its share
	bool isInBurst();

	// the thread's number within a burst: 0 for the main thread, 1 to workerCount() for the workers
	int threadIndex();

	// Work a burst's thread may not do (fill a cache that depends on when it is filled, ask
	// Python) hands its item back: the code that would do it calls needMainThread() instead
	// and returns any value, and the item's runner (takeNeedMainThread()) leaves the item for
	// the main thread to do again after the burst.
	// What a handed-back item needs, where the main thread can do just that and the item can then
	// be done again in another burst: the first need of the item is kept.
	enum NeedTypes
	{
		NEED_NONE,
		NEED_OTHER,				// nothing the main thread can do for it alone: the item is done there
		NEED_BONUS_VALUE,		// iData1 the player, iData2 the bonus: CvPlayerAI::AI_baseBonusVal()
		NEED_UPGRADE_GOLD,		// iData1 the player, iData2 the experience threshold: CvPlayerAI::AI_goldToUpgradeAllUnits()
	};
	struct Need
	{
		int eType;
		int iData1;
		int iData2;
	};
	void needMainThread(int eType = NEED_OTHER, int iData1 = 0, int iData2 = 0);
	bool takeNeedMainThread(Need* pNeed = NULL);
	bool hasNeedMainThread();	// whether the thread's item has been handed back, without clearing it

	// the lock for work a burst's threads must not do at once (a define's first lookup)
	void lock();
	void unlock();

	// the worker heap's allocation and freeing, for operator new and delete
	void* workerAllocate(size_t uiSize);
	void workerFree(void* p);

	// true while a burst is under way (read without a lock: it only gates the thread checks)
	extern volatile long g_lBurstActive;
}

#endif
