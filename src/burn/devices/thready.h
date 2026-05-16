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
#include <atomic>
static long unsigned int __stdcall ThreadyProc(void*);

struct threadystruct
{
private:
	// Atomic variables for thread-safe state management
	std::atomic<INT32> thready_ok{ 0 };
	std::atomic<INT32> ok_to_thread{ 0 };
	std::atomic<INT32> ok_to_wait{ 0 };
	std::atomic<INT32> end_thread{ 0 };
	std::atomic<INT32> init_complete{ 0 };

	// Windows kernel objects for thread and synchronization
	HANDLE our_thread{ nullptr };
	HANDLE our_event{ nullptr };
	HANDLE wait_event{ nullptr };
	DWORD  our_threadid{ 0 };

	// Thread-safe callback function pointer
	std::atomic<void(*)()> our_callback{ nullptr };

	// Frame counter for synchronous startup phase
	INT32 startup_frame{ 0 };

	// Check if the component is fully initialized and valid
	bool is_fully_initialized() const {
		return (0 != init_complete.load(std::memory_order_acquire) && nullptr != our_event && nullptr != wait_event && nullptr != our_thread);
	}

	// Allow the thread procedure to access private members
	friend long unsigned int __stdcall ThreadyProc(void*);

public:
	// Default constructor
	threadystruct() = default;

	// Initialize the thread system with a user callback
	void init(void(*thread_callback)()) {
		// If already initialized, attempt safe cleanup before reinitializing
		if (thready_ok.load(std::memory_order_acquire)) {
			end_thread.store(1, std::memory_order_release);
			if (our_event)
				SetEvent(our_event);
			if (our_thread)
				WaitForSingleObject(our_thread, 200);	// Short wait for safe thread exit
		}

		// Reset all states to initial values
		thready_ok.store(   0, std::memory_order_relaxed);
		ok_to_thread.store( 0, std::memory_order_relaxed);
		ok_to_wait.store(   0, std::memory_order_relaxed);
		end_thread.store(   0, std::memory_order_relaxed);
		init_complete.store(0, std::memory_order_relaxed);
		startup_frame = 0;

		// Store the user callback atomically
		our_callback.store(thread_callback, std::memory_order_relaxed);

		// Create auto-reset events for task signaling
		our_event  = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		wait_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);

		if (!our_event || !wait_event) {
			if (our_event)
				CloseHandle(our_event);
			if (wait_event)
				CloseHandle(wait_event);
			our_event = wait_event = nullptr;
			return;
		}

		// Create thread in suspended state to avoid race conditions during init
		our_thread = CreateThread(nullptr, 0, ThreadyProc, this, CREATE_SUSPENDED, &our_threadid);

		if (!our_thread) {
			CloseHandle(our_event);
			CloseHandle(wait_event);
			our_event = wait_event = nullptr;
			return;
		}

		// Memory barrier to ensure all writes are visible to the worker thread
		std::atomic_thread_fence(std::memory_order_release);

		// Mark initialization complete and start the thread
		init_complete.store(1, std::memory_order_release);
		ResumeThread(our_thread);

		if (is_fully_initialized()) {
			thready_ok.store(  1, std::memory_order_release);
			ok_to_thread.store(1, std::memory_order_release);
		}
	}

	// Safely shut down the thread and release all resources
	void exit() {
		INT32 ok = thready_ok.load(std::memory_order_acquire);
		if (!ok)
			return;

		// Signal thread to exit
		end_thread.store(1, std::memory_order_release);

		// Wake up the thread to process the exit signal
		if (our_event)
			SetEvent(our_event);

		// Wait for thread to exit with timeout to prevent hanging
		if (our_thread) {
			const DWORD timeout_ms = 5000;
			DWORD wait_result = WaitForSingleObject(our_thread, timeout_ms);

			if (WAIT_TIMEOUT == wait_result)
				TerminateThread(our_thread, 0);			// Last-resort termination

			CloseHandle(our_thread);
			our_thread = nullptr;
		}

		// Release event handles
		if (our_event) {
			CloseHandle(our_event);
			our_event = nullptr;
		}
		if (wait_event) {
			CloseHandle(wait_event);
			wait_event = nullptr;
		}

		// Reset state flags
		thready_ok.store(   0, std::memory_order_release);
		init_complete.store(0, std::memory_order_release);
	}

	// Serialization helper for state saving
	void scan() {
		SCAN_VAR(startup_frame);
	}

	// Reset the startup frame counter
	void reset() {
		startup_frame = STARTUP_FRAMES;
	}

	// Enable or disable threaded execution mode
	void set_threading(INT32 value) {
		ok_to_thread.store(value ? 1 : 0, std::memory_order_release);
	}

	// Notify the worker thread to execute the callback
	void notify() {
		if (startup_frame > 0) {
			startup_frame--;
			void(*cb)() = our_callback.load(std::memory_order_acquire);
			if (cb)
				cb();
			return;
		}

		if (thready_ok.load(std::memory_order_acquire) && ok_to_thread.load(std::memory_order_acquire) && our_event) {
			SetEvent(our_event);
			ok_to_wait.store(1, std::memory_order_release);
		} else {
			void(*cb)() = our_callback.load(std::memory_order_acquire);
			if (cb)
				cb();
		}
	}

	// Wait until the worker thread completes the current task
	void notify_wait() {
		if (ok_to_wait.load(std::memory_order_acquire) && wait_event) {
			WaitForSingleObject(wait_event, INFINITE);
			ok_to_wait.store(0, std::memory_order_release);
		}
	}

	// Destructor ensures safe cleanup
	~threadystruct() {
		exit();
	}

	// Disable copy and move to prevent handle duplication and undefined behavior
	threadystruct(const threadystruct&)            = delete;
	threadystruct& operator=(const threadystruct&) = delete;
	threadystruct(threadystruct&&)                 = delete;
	threadystruct& operator=(threadystruct&&)      = delete;
};

// Global singleton instance
static threadystruct thready;

// Main worker thread procedure
static long unsigned int __stdcall ThreadyProc(void* param) {
	threadystruct* pThready = static_cast<threadystruct*>(param);
	if (!pThready)
		return 0;

	// Wait until initialization is fully complete
	while (!pThready->init_complete)
		Sleep(1);

	do {
		// Defensive check: exit immediately if shutdown is requested
		if (0 != pThready->end_thread)
			break;

		// Wait indefinitely for a task signal (0% CPU idle)
		DWORD dwWaitResult = WaitForSingleObject(pThready->our_event, INFINITE);

		if (WAIT_OBJECT_0 == dwWaitResult && 0 == pThready->end_thread) {
			// Execute the user callback safely
			void(*cb)() = pThready->our_callback.load(std::memory_order_acquire);
			if (cb)
				cb();

			// Signal task completion
			if (pThready->wait_event)
				SetEvent(pThready->wait_event);
		} else
			// Exit on signal or error
			break;
	} while (true);

	// Ensure no waiting thread is left blocked
	if (pThready->wait_event)
		SetEvent(pThready->wait_event);

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
