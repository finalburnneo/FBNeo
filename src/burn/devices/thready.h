// thready spent most of the 1900's in a little old lady's sewing basket..
// thready had big dreams, this is one of them!                    - dink 2022

#define THREADY_WINDOWS		1 // we're on Windows
#define THREADY_WINDOWSVC	2 // Fix some bugs in the msvc compiler ...
#define THREADY_PTHREAD		3 // anything that supports pthreads (linux, android, not apple)
#define THREADY_PTHREADAPL	4 // apple pthreads
#define THREADY_0THREAD		5 // neither of the above.. (no threading!)

#define STARTUP_FRAMES 180
#define THREADY_CHECK_CB_TIME 0

#if ((defined _MSC_VER) || (defined WIN32))
#ifndef _MSC_VER
#define THREADY THREADY_WINDOWS
#else
#define THREADY THREADY_WINDOWSVC	// _MSC_VER
#endif
#if !defined(UNICODE) && defined(BUILD_WIN32)
#define UNICODE
#endif
#include "windows.h"
#endif

#if !defined(WIN32) && ( defined(__linux__) || defined(__ANDROID__) || defined(__APPLE__) )
  #if defined(__APPLE__)
  #define THREADY THREADY_PTHREADAPL
  #else
  #define THREADY THREADY_PTHREAD
  #endif
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#ifndef THREADY
// system not windows and without pthreads
#define THREADY THREADY_0THREAD
#endif

#if (THREADY == THREADY_WINDOWS)
static long unsigned int __stdcall ThreadyProc(void*);

struct threadystruct
{
	INT32 thready_ok;
	INT32 ok_to_thread;
	INT32 ok_to_wait;
	INT32 end_thread;
	HANDLE our_thread;
	HANDLE our_event;
	HANDLE wait_event;
	DWORD our_threadid;
	void (*our_callback)();
	INT32 startup_frame;

	void init(void (*thread_callback)()) {
		thready_ok = 0;
		ok_to_thread = 0;
		ok_to_wait = 0;
		startup_frame = 0;

		our_callback = thread_callback;

#if 0
		SYSTEM_INFO info;
		GetSystemInfo(&info);
		INT32 maxproc = (info.dwNumberOfProcessors > 4) ? 4 : info.dwNumberOfProcessors;
		INT32 thready_proc = rand() % maxproc;
#endif

		//bprintf(0, _T("Thready: processors available: %d, blitter processor: %d\n"), info.dwNumberOfProcessors, thready_proc);

		end_thread = 0; // good to go!

		our_event = CreateEvent(NULL, FALSE, FALSE, NULL);
		wait_event = CreateEvent(NULL, FALSE, FALSE, NULL);
		our_thread = CreateThread(NULL, 0, ThreadyProc, NULL, 0, &our_threadid);

#if 0
		SetThreadIdealProcessor(our_thread, thready_proc);
#endif

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

	void scan() {
		SCAN_VAR(startup_frame);
	}

	void reset() { // only call this if using for epic12
		startup_frame = STARTUP_FRAMES;
	}

	void set_threading(INT32 value) {
		ok_to_thread = value;
	}

	void notify() {
		if (startup_frame > 0) {
			// run in synchronous mode for startup_frame frames
			startup_frame--;
			our_callback();
			return;
		}
		if (thready_ok && ok_to_thread) {
			SetEvent(our_event);
			ok_to_wait = 1;
		} else {
			// fallback to single-threaded mode
			our_callback();
		}
	}
	void notify_wait() {
		if (ok_to_thread && ok_to_wait) {
			WaitForSingleObject(wait_event, INFINITE);
			ok_to_wait = 0;
		}
	}
};

static threadystruct thready;

static long unsigned int __stdcall ThreadyProc(void*) {
	do {
		DWORD dwWaitResult = WaitForSingleObject(thready.our_event, INFINITE);

		if (dwWaitResult == WAIT_OBJECT_0 && thready.end_thread == 0) {
#if THREADY_CHECK_CB_TIME
			time_t begin_time = clock();
#endif
			thready.our_callback();
#if THREADY_CHECK_CB_TIME
			time_t end_time = clock();
			double duration = (double)(end_time - begin_time) / CLOCKS_PER_SEC;
			bprintf(0, _T("thready: callback took %2.3f seconds\n"), duration);
#endif
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


#if (THREADY == THREADY_WINDOWSVC)
static long unsigned int __stdcall ThreadyProc(void*);
/*
	Because these two variables (our_event / wait_event) are not properly synchronized, the thread locks upand is hung indefinitely.
	The vc compiler doesn't seem to be able to automatically add/unlock static variables of classes in threads when handling them,
	as the gcc compiler does, thus ensuring the security of data in threads.
*/
static HANDLE our_event;
static HANDLE wait_event;
static INT32 end_thread;
static void (*our_callback)();

struct threadystruct
{
	INT32 thready_ok;
	INT32 ok_to_thread;
	INT32 ok_to_wait;
	HANDLE our_thread;
	DWORD our_threadid;
	INT32 startup_frame;

	void init(void (*thread_callback)()) {
		thready_ok = 0;
		ok_to_thread = 0;
		ok_to_wait = 0;
		startup_frame = 0;

		our_callback = thread_callback;

#if 0
		SYSTEM_INFO info;
		GetSystemInfo(&info);
		INT32 maxproc = (info.dwNumberOfProcessors > 4) ? 4 : info.dwNumberOfProcessors;
		INT32 thready_proc = rand() % maxproc;
#endif

		//bprintf(0, _T("Thready: processors available: %d, blitter processor: %d\n"), info.dwNumberOfProcessors, thready_proc);

		end_thread = 0; // good to go!

		our_event	= CreateEvent(NULL, FALSE, FALSE, NULL);
		wait_event	= CreateEvent(NULL, FALSE, FALSE, NULL);
		our_thread	= CreateThread(NULL, 0, ThreadyProc, NULL, 0, &our_threadid);
		// bprintf(0, _T("Init: our_event: %d, wait_event: %d\n"), our_event, wait_event);

#if 0
		SetThreadIdealProcessor(our_thread, thready_proc);
#endif

		if (our_event && wait_event && our_thread) {
			//bprintf(0, _T("Thready: we're gonna git 'r dun!\n"));
			CloseHandle(our_thread); // After that, no operation of this handle is required.

			thready_ok = 1;
			ok_to_thread = 1;
		} else {
			//bprintf(0, _T("Thready: failure to create thread!\n"));
		}
	}

	void exit() {
		if (thready_ok) {
			if (0x0101 != end_thread) {
				end_thread = 1;
				SetEvent(our_event);
			} else {																// ThreadyProc has returned 0
				if (INVALID_HANDLE_VALUE != our_event) CloseHandle(our_event);		// Close it before it becomes an invalid handle.
				if (INVALID_HANDLE_VALUE != wait_event) CloseHandle(wait_event);	// Id.
			}
			do {
				Sleep(42);					// let thread realize it's time to die
			} while (~end_thread & 0x100);
			thready_ok = 0;
		}
	}

	void scan() {
		SCAN_VAR(startup_frame);
	}

	void reset() { // only call this if using for epic12
		startup_frame = STARTUP_FRAMES;
	}

	void set_threading(INT32 value)	{
		ok_to_thread = value;
	}

	void notify() {
		if (startup_frame > 0) {
			// run in synchronous mode for startup_frame frames
			startup_frame--;
			our_callback();
			return;
		}
		if (thready_ok && ok_to_thread) {
			SetEvent(our_event);
			ok_to_wait = 1;
		} else {
			// fallback to single-threaded mode
			our_callback();
		}
	}
	void notify_wait() {
		if (ok_to_thread && ok_to_wait) {
			WaitForSingleObject(wait_event, INFINITE);
			ok_to_wait = 0;
		}
	}
};

static threadystruct thready;

static long unsigned int __stdcall ThreadyProc(void*) {
	do {
		// bprintf(0, _T("Proc: our_event: %d, wait_event: %d\n"), our_event, wait_event);
		if ((WAIT_OBJECT_0 == WaitForSingleObject(our_event, INFINITE)) && (end_thread == 0)) {
#if THREADY_CHECK_CB_TIME
			time_t begin_time = clock();
#endif
			our_callback();
#if THREADY_CHECK_CB_TIME
			time_t end_time = clock();
			double duration = (double)(end_time - begin_time) / CLOCKS_PER_SEC;
			bprintf(0, _T("thready: callback took %2.3f seconds\n"), duration);
#endif
			SetEvent(wait_event);
		} else {
			SetEvent(wait_event);
			end_thread |= 0x100;
			//bprintf(0, _T("Thready: thread-event thread ending..\n"));
			return 0;
		}
	} while (1);

	return 0;
}
#endif // THREADY_WINDOWSVC


#if (THREADY == THREADY_PTHREAD)
static void *ThreadyProc(void*);

struct threadystruct
{
	INT32 thready_ok;
	INT32 ok_to_thread;
	INT32 ok_to_wait;
	INT32 end_thread;
	sem_t our_event;
	sem_t wait_event;
	pthread_t our_thread;
	void (*our_callback)();
	INT32 startup_frame;

	void init(void (*thread_callback)()) {
		thready_ok = 0;
		ok_to_thread = 0;
		ok_to_wait = 0;
		end_thread = 0;

		our_callback = thread_callback;

		INT32 our_thread_rv = pthread_create(&our_thread, NULL, ThreadyProc, NULL);
		INT32 our_event_rv = sem_init(&our_event, 0, 0);
		INT32 wait_event_rv = sem_init(&wait_event, 0, 0);

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
		}
	}

	void scan() {
		SCAN_VAR(startup_frame);
	}

	void reset() {
		startup_frame = STARTUP_FRAMES;
	}

	void set_threading(INT32 value)
	{
		ok_to_thread = value;
	}

	void notify() {
		if (startup_frame > 0) {
			// run in synchronous mode for startup_frame frames
			startup_frame--;
			our_callback();
			return;
		}
		if (thready_ok && ok_to_thread) {
			sem_post(&our_event);
			ok_to_wait = 1;
		} else {
			// fallback to single-threaded mode
			our_callback();
		}
	}
	void notify_wait() {
		if (ok_to_wait) {
			sem_wait(&wait_event);
			ok_to_wait = 0;
		}
	}
};

static threadystruct thready;

static void *ThreadyProc(void*) {
	do {
		sem_wait(&thready.our_event);

		if (thready.end_thread == 0) {
			thready.our_callback();
			sem_post(&thready.wait_event);
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


#if (THREADY == THREADY_PTHREADAPL)
static void *ThreadyProc(void*);

struct threadystruct
{
	INT32 thready_ok;
	INT32 ok_to_thread;
	INT32 ok_to_wait;
	INT32 end_thread;
	sem_t *our_event;
	sem_t *wait_event;
	pthread_t our_thread;
	void (*our_callback)();
	INT32 startup_frame;
	char our_event_str[40];
	char wait_event_str[40];

	void init(void (*thread_callback)()) {
		thready_ok = 0;
		ok_to_thread = 0;
		ok_to_wait = 0;
		end_thread = 0;

		our_callback = thread_callback;

		INT32 our_thread_rv = pthread_create(&our_thread, NULL, ThreadyProc, NULL);

		sprintf(our_event_str, "/fbn_our_event%x", getpid());
		sprintf(wait_event_str, "/fbn_wait_event%x", getpid());

		INT32 our_event_rv = ((our_event = sem_open(our_event_str, O_CREAT, 0644, 1)) == SEM_FAILED) ? -1 : 0;
		INT32 wait_event_rv = ((wait_event = sem_open(wait_event_str, O_CREAT, 0644, 1)) == SEM_FAILED) ? -1 : 0;

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
			sem_post(our_event);
			do {
				sleep(0.10); // let thread realize it's time to die
			} while (~end_thread & 0x100);
			pthread_join(our_thread, NULL);

			sem_close(our_event);
			sem_unlink(our_event_str);
			sem_close(wait_event);
			sem_unlink(wait_event_str);

			thready_ok = 0;
		}
	}

	void scan() {
		SCAN_VAR(startup_frame);
	}

	void reset() {
		startup_frame = STARTUP_FRAMES;
	}

	void set_threading(INT32 value)
	{
		ok_to_thread = value;
	}

	void notify() {
		if (startup_frame > 0) {
			// run in synchronous mode for startup_frame frames
			startup_frame--;
			our_callback();
			return;
		}
		if (thready_ok && ok_to_thread) {
			sem_post(our_event);
			ok_to_wait = 1;
		} else {
			// fallback to single-threaded mode
			our_callback();
		}
	}
	void notify_wait() {
		if (ok_to_wait) {
			sem_wait(wait_event);
			ok_to_wait = 0;
		}
	}
};

static threadystruct thready;

static void *ThreadyProc(void*) {
	do {
		sem_wait(thready.our_event);

		if (thready.end_thread == 0) {
			thready.our_callback();
			sem_post(thready.wait_event);
		} else {
			sem_post(thready.wait_event);
			thready.end_thread |= 0x100;
			bprintf(0, _T("Thready: thread-event thread ending..\n"));
			return 0;
		}
	} while (1);

	return 0;
}
#endif // THREADY_PTHREADAPL


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

	void scan() {
	}

	void reset() {
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
