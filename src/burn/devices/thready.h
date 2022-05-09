// thready spent most of the 1900's in a little old lady's sewing basket..
// thready had big dreams, this is one of them!                    - dink 2022

#define THREADY_WINDOWS 1 // we're on Windows
#define THREADY_PTHREAD 2 // anything that supports pthreads (linux, android, mac)
#define THREADY_0THREAD 3 // neither of the above.. (no threading!)

#if defined(WIN32)
#define THREADY THREADY_WINDOWS
#include "windows.h"
#endif

#if !defined(WIN32) && ( defined(__linux__) || defined(__ANDROID__) || defined(__APPLE__) )
#define THREADY THREADY_PTHREAD
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#endif

#ifndef THREADY
// system not windows and without pthreads
#define THREADY THREADY_0THREAD
#endif

#if (THREADY == THREADY_WINDOWS)
long unsigned int __stdcall ThreadyProc(void*);

struct threadystruct
{
	INT32 thready_ok;
	INT32 ok_to_thread;
	INT32 end_thread;
	HANDLE our_thread;
	HANDLE our_event;
	HANDLE wait_event;
	DWORD our_threadid;
	void (*our_callback)();

	void init(void (*thread_callback)()) {
		thready_ok = 0;
		ok_to_thread = 0;

		our_callback = thread_callback;

		SYSTEM_INFO info;
		GetSystemInfo(&info);
		INT32 maxproc = (info.dwNumberOfProcessors > 4) ? 4 : info.dwNumberOfProcessors;
		INT32 thready_proc = rand() % maxproc;

		//bprintf(0, _T("Thready: processors available: %d, blitter processor: %d\n"), info.dwNumberOfProcessors, thready_proc);

		end_thread = 0; // good to go!

		our_event = CreateEvent(NULL, FALSE, FALSE, NULL);
		wait_event = CreateEvent(NULL, FALSE, TRUE, NULL);
		our_thread = CreateThread(NULL, 0, ThreadyProc, NULL, 0, &our_threadid);

		SetThreadIdealProcessor(our_thread, thready_proc);

		if (our_event && wait_event && our_thread) {
			//bprintf(0, _T("Thready: we're gonna git 'r dun!\n"));
			thready_ok = 1;
			ok_to_thread = 1;
		} else {
			//bprintf(0, _T("Thready: failure to create thread!\n"));
		}
	}

	void exit() {
		if (thready_ok) {
			end_thread = 1;
			SetEvent(our_event);
			do {
				Sleep(42); // let thread realize it's time to die
			} while (~end_thread & 0x100);
			CloseHandle(our_event);
			CloseHandle(our_thread);
			CloseHandle(wait_event);
			thready_ok = 0;
		}
	}

	void set_threading(INT32 value)	{
		ok_to_thread = value;
	}

	void notify() {
		if (thready_ok && ok_to_thread) {
			SetEvent(our_event);
		} else {
			// fallback to single-threaded mode
			our_callback();
		}
	}
	void notify_wait() {
		if (ok_to_thread) {
			WaitForSingleObject(wait_event, INFINITE);
		}
	}
};

static threadystruct thready;

long unsigned int __stdcall ThreadyProc(void*) {
	do {
		DWORD dwWaitResult = WaitForSingleObject(thready.our_event, INFINITE);

		if (dwWaitResult == WAIT_OBJECT_0 && thready.end_thread == 0) {
			thready.our_callback();
			SetEvent(thready.wait_event);
		} else {
			SetEvent(thready.wait_event);
			thready.end_thread |= 0x100;
			//bprintf(0, _T("Thready: thread-event thread ending..\n"));
			return 0;
		}
	} while (1);

	return 0;
}
#endif // THREADY_WINDOWS

#if (THREADY == THREADY_PTHREAD)
static void *ThreadyProc(void*);

struct threadystruct
{
	INT32 thready_ok;
	INT32 ok_to_thread;
	INT32 end_thread;
	sem_t our_event;
	sem_t wait_event;
	INT32 is_running;
	pthread_t our_thread;
	void (*our_callback)();

	void init(void (*thread_callback)()) {
		thready_ok = 0;
		ok_to_thread = 0;
		end_thread = 0;
		is_running = 0;

		our_callback = thread_callback;

		INT32 our_event_rv = sem_init(&our_event, 0, 0);
		INT32 wait_event_rv = sem_init(&wait_event, 0, 0);
		INT32 our_thread_rv = pthread_create(&our_thread, NULL, ThreadyProc, NULL);

		if (our_thread_rv == 0 && wait_event_rv == 0 && our_event_rv == 0) {
			bprintf(0, _T("Thready: we're gonna git 'r dun!\n"));
			thready_ok = 1;
			ok_to_thread = 1;
		} else {
			bprintf(0, _T("Thready: failure to create thread - falling back to single-thread mode!\n"));
		}
	}

	void exit() {
		if (thready_ok) {
			//bprintf(0, _T("Thready: notify thread to exit..\n"));
			end_thread = 1;
			sem_post(&our_event);
			do {
				sleep(0.10); // let thread realize it's time to die
			} while (~end_thread & 0x100);
			pthread_join(our_thread, NULL);
			sem_destroy(&our_event);
			sem_destroy(&wait_event);
			thready_ok = 0;
			is_running = 0;
		}
	}

	void set_threading(INT32 value)
	{
		ok_to_thread = value;
	}

	void notify() {
		if (thready_ok && ok_to_thread) {
			sem_post(&our_event);
		} else {
			// fallback to single-threaded mode
			our_callback();
		}
	}
	void notify_wait() {
		if (ok_to_thread) {
			sem_wait(&wait_event);
		}
	}
};

static threadystruct thready;

static void *ThreadyProc(void*) {
	do {
		sem_post(&thready.wait_event);
		sem_wait(&thready.our_event);

		if (thready.end_thread == 0) {
			thready.is_running = 1;
			thready.our_callback();
			thready.is_running = 0;
		} else {
			sem_post(&thready.wait_event);
			thready.end_thread |= 0x100;
			bprintf(0, _T("Thready: thread-event thread ending..\n"));
			return 0;
		}
	} while (1);

	return 0;
}
#endif // THREADY_PTHREAD

#if (THREADY == THREADY_0THREAD)
// this isn't great
struct threadystruct
{
	void (*our_callback)();

	void init(void (*thread_callback)()) {
		our_callback = thread_callback;

		bprintf(0, _T("Thready: single-threaded on this platform, performance will suck.\n"));
	}

	void exit() {
	}

	void set_threading(INT32 value)
	{
		if (value)
			bprintf(0, _T("Thready: we can't thread on this platform yet.\n"));
	}

	void notify() {
		// fallback to single-threaded mode
		our_callback();
	}
	void notify_wait() {
	}
};

static threadystruct thready;
#endif // THREADY_0THREAD
