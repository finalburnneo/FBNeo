//$ nobt

/**
 * @file r8bbase.h
 *
 * @version 7.1
 *
 * @brief The "base" header file with basic classes and functions.
 *
 * This is the "base" header file for the "r8brain-free-src" sample rate
 * converter. This file contains implementations of several small utility
 * classes and functions used by the library.
 *
 * @mainpage
 *
 * @section intro_sec Introduction
 *
 * Open source (under the MIT license) high-quality professional audio sample
 * rate converter (SRC) / resampler C++ library.  Features routines for SRC,
 * both up- and downsampling, to/from any sample rate, including non-integer
 * sample rates: it can be also used for conversion to/from SACD/DSD sample
 * rates, and even go beyond that.  SRC routines were implemented in a
 * multi-platform C++ code, and have a high level of optimality. Also suitable
 * for fast general-purpose 1D time-series resampling / interpolation (with
 * relaxed filter parameters).
 *
 * For more information, please visit
 * https://github.com/avaneev/r8brain-free-src
 *
 * Email: aleksey.vaneev@gmail.com or info@voxengo.com
 *
 * @section license License
 *
 * The MIT License (MIT)
 * 
 * r8brain-free-src Copyright (c) 2013-2025 Aleksey Vaneev
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * 
 * Please credit the creator of this library in your documentation in the
 * following way: "Sample rate converter designed by Aleksey Vaneev of
 * Voxengo"
 */

#ifndef R8BBASE_INCLUDED
#define R8BBASE_INCLUDED

#define R8B_VERSION "7.1" ///< Macro defines r8brain-free-src version string.

/**
 * @def R8B_CONST
 * @brief The prefix for constant definitions, portable between C++11 and
 * earlier C++ versions.
 */

/**
 * @def R8B_NULL
 * @brief The "null pointer" value, portable between C++11 and earlier C++
 * versions.
 */

#if __cplusplus >= 201103L

	#include <cstdint>
	#include <cstring>
	#include <cmath>
	#include <mutex>

	#define R8B_CONST constexpr
	#define R8B_NULL nullptr

#else // __cplusplus >= 201103L

	#include <stdint.h>
	#include <string.h>
	#include <math.h>

	#if defined( _WIN32 )
		#include <Windows.h>
	#else // defined( _WIN32 )
		#include <pthread.h>
	#endif // defined( _WIN32 )

	#define R8B_CONST static const
	#define R8B_NULL NULL

#endif // __cplusplus >= 201103L

#include "r8bconf.h"

#if defined( __aarch64__ ) || defined( __arm64__ ) || \
	defined( _M_ARM64 ) || defined( _M_ARM64EC )

	#if defined( _MSC_VER )
		#include <arm64_neon.h>
	#else // defined( _MSC_VER )
		#include <arm_neon.h>
	#endif // defined( _MSC_VER )

	#define R8B_NEON

	#if !defined( __APPLE__ )
		#define R8B_SIMD_ISH // Shuffled interpolation is inefficient on M1.
	#endif // !defined( __APPLE__ )

#elif defined( __SSE2__ ) || defined( _M_AMD64 ) || \
	( defined( _M_IX86_FP ) && _M_IX86_FP == 2 )

	#if defined( _MSC_VER )
		#include <intrin.h>
	#else // defined( _MSC_VER )
		#include <emmintrin.h>
	#endif // defined( _MSC_VER )

	#define R8B_SSE2
	#define R8B_SIMD_ISH

#endif // SSE2

/**
 * @def R8B_EXITDTOR
 * @brief Macro that defines the attribute specifying that the exit-time
 * destructor should be called for a static variable (suppresses a compiler
 * warning).
 */

#if defined( __clang__ )

	#define R8B_EXITDTOR __attribute__((always_destroy))

#else // defined( __clang__ )

	#define R8B_EXITDTOR

#endif // defined( __clang__ )

/**
 * @brief The "r8brain-free-src" library namespace.
 *
 * The "r8brain-free-src" sample rate converter library namespace.
 */

namespace r8b {

#if __cplusplus >= 201103L

	using std :: memcpy;
	using std :: memset;
	using std :: floor;
	using std :: ceil;
	using std :: exp;
	using std :: log;
	using std :: fabs;
	using std :: sqrt;
	using std :: sin;
	using std :: cos;
	using std :: atan2;
	using std :: size_t;
	using std :: uintptr_t;

#endif // __cplusplus >= 201103L

R8B_CONST double R8B_PI = 3.14159265358979324; ///< Equals `pi`.
R8B_CONST double R8B_2PI = 6.28318530717958648; ///< Equals `2*pi`.
R8B_CONST double R8B_3PI = 9.42477796076937972; ///< Equals `3*pi`.
R8B_CONST double R8B_PId2 = 1.57079632679489662; ///< Equals `0.5*pi`.

/**
 * @brief Macro that defines empty copy-constructor and copy operator with
 * the "private:" prefix.
 *
 * This macro should be used in classes that cannot be copied in a standard
 * C++ way. It is also assumed that objects of such classes are
 * non-relocatable.
 *
 * This macro does not need to be defined in classes derived from a class
 * where such macro was already used.
 *
 * @param ClassName The name of the class which uses this macro.
 */

#define R8BNOCTOR( ClassName ) \
	private: \
		ClassName( const ClassName& ) { } \
		ClassName& operator = ( const ClassName& ) { return( *this ); }

/**
 * @brief The default base class for objects created on heap.
 */

class CStdClassAllocator
{
};

/**
 * @brief The default base class for objects that allocate blocks of memory.
 *
 * Memory buffer allocator that uses standard C++ "new" to allocate memory.
 */

class CStdMemAllocator : public CStdClassAllocator
{
public:
	/**
	 * @brief Allocates a memory block.
	 *
	 * @param Size The size of the block, in bytes.
	 * @return The pointer to the allocated block.
	 */

	static void* allocmem( const size_t Size )
	{
		return( new char[ Size ]);
	}

	/**
	 * @brief Frees a previously allocated memory block.
	 *
	 * @param p Pointer to the allocated block, can be `nullptr`.
	 */

	static void freemem( void* const p )
	{
		delete[] (char*) p;
	}
};

/**
 * @brief Forces the provided `ptr` pointer to be aligned to `align` bytes.
 *
 * Works with power-of-2 alignments only.
 *
 * @param ptr Pointer to align.
 * @param align Alignment, in bytes, power-of-2.
 * @tparam T Pointer's element type.
 * @return Aligned pointer.
 */

template< typename T >
inline T* alignptr( T* const ptr, const uintptr_t align )
{
	return( (T*) (( (uintptr_t) ptr + align - 1 ) & ~( align - 1 )));
}

/**
 * @brief Templated memory buffer class for element buffers of fixed capacity.
 *
 * Fixed memory buffer object. Supports allocation of a fixed amount of
 * memory. Does not store buffer's capacity - the user should know the actual
 * capacity of the buffer. Does not feature "internal" storage, memory is
 * always allocated via the `R8B_MEMALLOCCLASS` class's functions. Thus the
 * object of this class can be moved in memory.
 *
 * This class manages memory space only - it does not perform element class
 * construction nor destruction operations.
 *
 * This class applies 64-byte memory address alignment to the allocated data
 * block.
 *
 * @tparam T The type of the stored elements (e.g., `double`).
 */

template< typename T >
class CFixedBuffer : public R8B_MEMALLOCCLASS
{
	R8BNOCTOR( CFixedBuffer )

public:
	CFixedBuffer()
		: Data0( R8B_NULL )
		, Data( R8B_NULL )
	{
	}

	/**
	 * @brief Constructor allocates memory so that the specified number of
	 * elements of type T can be stored in *this* buffer object.
	 *
	 * @param Capacity Storage for this number of elements to allocate.
	 */

	CFixedBuffer( const int Capacity )
	{
		R8BASSERT( Capacity > 0 || Capacity == 0 );

		Data0 = allocmem( (size_t) Capacity * sizeof( T ) + Alignment );
		Data = (T*) alignptr( Data0, Alignment );

		R8BASSERT( Data0 != R8B_NULL || Capacity == 0 );
	}

	~CFixedBuffer()
	{
		freemem( Data0 );
	}

	/**
	 * @brief Allocates memory so that the specified number of elements of
	 * type T can be stored in *this* buffer object.
	 *
	 * @param Capacity Storage for this number of elements to allocate.
	 */

	void alloc( const int Capacity )
	{
		R8BASSERT( Capacity > 0 || Capacity == 0 );

		freemem( Data0 );
		Data0 = allocmem( (size_t) Capacity * sizeof( T ) + Alignment );
		Data = (T*) alignptr( Data0, Alignment );

		R8BASSERT( Data0 != R8B_NULL || Capacity == 0 );
	}

	/**
	 * @brief Reallocates memory so that the specified number of elements of
	 * type T can be stored in *this* buffer object. Previously allocated data
	 * is copied to the new memory buffer.
	 *
	 * @param PrevCapacity Previous capacity of *this* buffer.
	 * @param NewCapacity Storage for this number of elements to allocate.
	 */

	void realloc( const int PrevCapacity, const int NewCapacity )
	{
		R8BASSERT( PrevCapacity >= 0 );
		R8BASSERT( NewCapacity >= 0 );

		void* const NewData0 = allocmem( (size_t) NewCapacity * sizeof( T ) +
			Alignment );

		T* const NewData = (T*) alignptr( NewData0, Alignment );
		const size_t CopySize = ( PrevCapacity > NewCapacity ?
			(size_t) NewCapacity : (size_t) PrevCapacity ) * sizeof( T );

		if( CopySize > 0 )
		{
			memcpy( NewData, Data, CopySize );
		}

		freemem( Data0 );
		Data0 = NewData0;
		Data = NewData;

		R8BASSERT( Data0 != R8B_NULL || NewCapacity == 0 );
	}

	/**
	 * @brief Deallocates a previously allocated buffer.
	 */

	void free()
	{
		freemem( Data0 );
		Data0 = R8B_NULL;
		Data = R8B_NULL;
	}

	/**
	 * @brief Returns pointer to the first element of the allocated buffer,
	 * `nullptr`, if not allocated.
	 */

	operator T* () const
	{
		return( Data );
	}

private:
	static const size_t Alignment = 64; ///< Buffer address alignment, in
		///< bytes.
	void* Data0; ///< Buffer pointer, original unaligned.
	T* Data; ///< Element buffer pointer, aligned.
};

/**
 * @brief Pointer-to-object "keeper" class with automatic deletion.
 *
 * An auxiliary class that can be used for keeping a pointer to object that
 * should be deleted together with the "keeper" by calling object's "delete"
 * operator.
 *
 * @tparam T Type of the keeped object, must not include an additional
 * asterisk.
 */

template< typename T >
class CPtrKeeper
{
	R8BNOCTOR( CPtrKeeper )

public:
	CPtrKeeper()
		: Object( R8B_NULL )
	{
	}

	/**
	 * @brief Constructor assigns a pointer to object to *this* keeper.
	 *
	 * @param aObject Pointer to object to keep, can be `nullptr`.
	 * @tparam T2 Object's pointer type.
	 */

	template< typename T2 >
	CPtrKeeper( T2 const aObject )
		: Object( aObject )
	{
	}

	~CPtrKeeper()
	{
		delete Object;
	}

	/**
	 * @brief Assigns a pointer to object to *this* keeper. A previously
	 * keeped pointer will be reset and object deleted.
	 *
	 * @param aObject Pointer to object to keep, can be `nullptr`.
	 * @tparam T2 Object's pointer type.
	 */

	template< typename T2 >
	void operator = ( T2 const aObject )
	{
		reset();
		Object = aObject;
	}

	/**
	 * @brief Returns pointer to keeped object, or `nullptr`, if no object is
	 * being kept.
	 */

	T* operator -> () const
	{
		return( Object );
	}

	/**
	 * @brief Returns pointer to keeped object, or `nullptr`, if no object is
	 * kept.
	 */

	operator T* () const
	{
		return( Object );
	}

	/**
	 * @brief Resets the keeped pointer and deletes the keeped object.
	 */

	void reset()
	{
		T* const DelObj = Object;
		Object = R8B_NULL;
		delete DelObj;
	}

	/**
	 * @brief Returns the keeped pointer and resets it in *this* keeper
	 * without object deletion.
	 */

	T* unkeep()
	{
		T* const ResObject = Object;
		Object = R8B_NULL;
		return( ResObject );
	}

private:
	T* Object; ///< Pointer to keeped object.
};

#if __cplusplus >= 201103L

typedef std :: mutex CSyncObject; ///< Mutex class.
typedef std :: lock_guard< std :: mutex > CSyncKeeper; ///< Mutex keeper class.

#else // __cplusplus >= 201103L

/**
 * @brief Multi-threaded synchronization object class.
 *
 * This class uses standard OS thread-locking (mutex) mechanism which is
 * fairly efficient in most cases.
 *
 * The acquire() function is re-enterant, and can be called recursively, in
 * the same thread, for this kind of thread-locking mechanism. This will not
 * produce a dead-lock.
 */

class CSyncObject
{
	R8BNOCTOR( CSyncObject )

public:
	CSyncObject()
	{
		#if defined( _WIN32 )
			InitializeCriticalSectionAndSpinCount( &CritSec, 2000 );
		#else // defined( _WIN32 )
			pthread_mutexattr_t MutexAttrs;
			pthread_mutexattr_init( &MutexAttrs );
			pthread_mutexattr_settype( &MutexAttrs, PTHREAD_MUTEX_RECURSIVE );
			pthread_mutex_init( &Mutex, &MutexAttrs );
			pthread_mutexattr_destroy( &MutexAttrs );
		#endif // defined( _WIN32 )
	}

	~CSyncObject()
	{
		#if defined( _WIN32 )
			DeleteCriticalSection( &CritSec );
		#else // defined( _WIN32 )
			pthread_mutex_destroy( &Mutex );
		#endif // defined( _WIN32 )
	}

	/**
	 * @brief Acquires *this* thread synchronizer object immediately or
	 * waits until another thread releases it.
	 */

	void acquire()
	{
		#if defined( _WIN32 )
			EnterCriticalSection( &CritSec );
		#else // defined( _WIN32 )
			pthread_mutex_lock( &Mutex );
		#endif // defined( _WIN32 )
	}

	/**
	 * @brief Releases *this*, previously acquired, thread synchronizer
	 * object.
	 */

	void release()
	{
		#if defined( _WIN32 )
			LeaveCriticalSection( &CritSec );
		#else // defined( _WIN32 )
			pthread_mutex_unlock( &Mutex );
		#endif // defined( _WIN32 )
	}

private:
	#if defined( _WIN32 )
		CRITICAL_SECTION CritSec; ///< Standard Windows critical section
			///< structure.
	#else // defined( _WIN32 )
		pthread_mutex_t Mutex; ///< pthread.h mutex object.
	#endif // defined( _WIN32 )
};

/**
 * @brief A "keeper" class for CSyncObject-based synchronization.
 *
 * Sync keeper class. The object of this class can be used as auto-init and
 * auto-deinit object for calling the acquire() and release() functions of an
 * object of the CSyncObject class. This "keeper" object is best used in
 * functions as an "automatic" object allocated on the stack, possibly via the
 * R8BSYNC() macro.
 */

class CSyncKeeper
{
	R8BNOCTOR( CSyncKeeper )

public:
	CSyncKeeper()
		: SyncObj( R8B_NULL )
	{
	}

	/**
	 * @brief Constructor acquires a specified synchronization object.
 	 *
	 * @param aSyncObj Pointer to the sync object which should be used for
	 * sync'ing, can be `nullptr`.
	 */

	CSyncKeeper( CSyncObject* const aSyncObj )
		: SyncObj( aSyncObj )
	{
		if( SyncObj != R8B_NULL )
		{
			SyncObj -> acquire();
		}
	}

	/**
	 * @brief Constructor acquires a specified synchronization object.
	 *
	 * @param aSyncObj Reference to the sync object which should be used for
	 * sync'ing.
	 */

	CSyncKeeper( CSyncObject& aSyncObj )
		: SyncObj( &aSyncObj )
	{
		SyncObj -> acquire();
	}

	~CSyncKeeper()
	{
		if( SyncObj != R8B_NULL )
		{
			SyncObj -> release();
		}
	}

private:
	CSyncObject* SyncObj; ///< Sync object in use (can be `nullptr`).
};

#endif // __cplusplus >= 201103L

/**
 * @brief Thread synchronization macro.
 *
 * The R8BSYNC() macro, which creates and object of the r8b::CSyncKeeper class
 * on stack, should be put before sections of the code that may potentially
 * change data asynchronously with other threads at the same time.
 * The R8BSYNC() macro "acquires" the synchronization object thus blocking
 * execution of other threads that also use the same R8BSYNC() macro.
 * The blocked section begins with the R8BSYNC() macro and finishes at the end
 * of the current C++ code block. R8BSYNC() is re-enterant within the same
 * thread.
 *
 * @param SyncObject An object of the CSyncObject type that is used for
 * synchronization.
 */

#define R8BSYNC( SyncObject ) R8BSYNC1( SyncObject, __LINE__ )
#define R8BSYNC1( SyncObject, id ) R8BSYNC2( SyncObject, id ) ///< Aux macro.
#define R8BSYNC2( SyncObject, id ) \
	const CSyncKeeper SyncKeeper##id( SyncObject ) ///< Aux macro.

/**
 * @brief Sine signal generator class.
 *
 * Class implements sine signal generator without biasing.
 */

class CSineGen
{
public:
	CSineGen()
	{
	}

	/**
	 * @brief Constructor initializes *this* sine signal generator, with unity
	 * gain output.
	 *
	 * @param si Sine function increment, in radians.
	 * @param ph Starting phase, in radians. Add R8B_PId2 for cosine function.
	 */

	CSineGen( const double si, const double ph )
		: svalue1( sin( ph ))
		, svalue2( sin( ph - si ))
		, sincr( 2.0 * cos( si ))
	{
	}

	/**
	 * @brief Constructor initializes *this* sine signal generator.
	 *
	 * @param si Sine function increment, in radians.
	 * @param ph Starting phase, in radians. Add R8B_PId2 for cosine function.
	 * @param g The overall gain factor, 1.0 for unity gain (-1.0 to 1.0
	 * amplitude).
	 */

	CSineGen( const double si, const double ph, const double g )
		: svalue1( sin( ph ) * g )
		, svalue2( sin( ph - si ) * g )
		, sincr( 2.0 * cos( si ))
	{
	}

	/**
	 * @brief Function initializes *this* sine signal generator, with unity
	 * gain output.
	 *
	 * @param si Sine function increment, in radians.
	 * @param ph Starting phase, in radians. Add R8B_PId2 for cosine function.
	 */

	void init( const double si, const double ph )
	{
		svalue1 = sin( ph );
		svalue2 = sin( ph - si );
		sincr = 2.0 * cos( si );
	}

	/**
	 * @brief Function initializes *this* sine signal generator.
	 *
	 * @param si Sine function increment, in radians.
	 * @param ph Starting phase, in radians. Add R8B_PId2 for cosine function.
	 * @param g The overall gain factor, 1.0 for unity gain (-1.0 to 1.0
	 * amplitude).
	 */

	void init( const double si, const double ph, const double g )
	{
		svalue1 = sin( ph ) * g;
		svalue2 = sin( ph - si ) * g;
		sincr = 2.0 * cos( si );
	}

	/**
	 * @brief Generates the next sample.
	 *
	 * @return Next value of the sine function, without biasing.
	 */

	double generate()
	{
		const double res = svalue1;

		svalue1 = sincr * res - svalue2;
		svalue2 = res;

		return( res );
	}

private:
	double svalue1; ///< Current sine value.
	double svalue2; ///< Previous sine value.
	double sincr; ///< Sine value increment.
};

/**
 * @brief Calculate the exact number of bits a value needs for representation.
 *
 * @param v Input value.
 * @return Calculated bit occupancy of the specified input value. Bit
 * occupancy means how many significant lower bits are necessary to store a
 * specified value. Function treats the input value as unsigned.
 */

inline int getBitOccupancy( const int v )
{
	static const unsigned char OccupancyTable[] =
	{
		1, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
		5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8
	};

	const int tt = v >> 16;

	if( tt != 0 )
	{
		const int t = v >> 24;

		return( t != 0 ? 24 + OccupancyTable[ t & 0xFF ] :
			16 + OccupancyTable[ tt ]);
	}
	else
	{
		const int t = v >> 8;

		return( t != 0 ? 8 + OccupancyTable[ t ] : OccupancyTable[ v ]);
	}
}

/**
 * @brief FIR filter's frequency response calculation.
 *
 * Function calculates frequency response of the specified FIR filter at the
 * specified circular frequency. Phase can be calculated as atan2( im, re ).
 *
 * @param flt FIR filter's coefficients.
 * @param fltlen Number of coefficients (taps) in the filter.
 * @param th Circular frequency [0; pi].
 * @param[out] re0 Resulting real part of the complex frequency response.
 * @param[out] im0 Resulting imaginary part of the complex frequency response.
 * @param fltlat Filter's latency, in samples.
 */

inline void calcFIRFilterResponse( const double* flt, int fltlen,
	const double th, double& re0, double& im0, const int fltlat = 0 )
{
	const double sincr = 2.0 * cos( th );
	double cvalue1;
	double svalue1;

	if( fltlat == 0 )
	{
		cvalue1 = 1.0;
		svalue1 = 0.0;
	}
	else
	{
		cvalue1 = cos( -fltlat * th );
		svalue1 = sin( -fltlat * th );
	}

	double cvalue2 = cos( -( fltlat + 1 ) * th );
	double svalue2 = sin( -( fltlat + 1 ) * th );

	double re = 0.0;
	double im = 0.0;

	while( fltlen > 0 )
	{
		re += cvalue1 * flt[ 0 ];
		im += svalue1 * flt[ 0 ];
		flt++;
		fltlen--;

		double tmp = cvalue1;
		cvalue1 = sincr * cvalue1 - cvalue2;
		cvalue2 = tmp;

		tmp = svalue1;
		svalue1 = sincr * svalue1 - svalue2;
		svalue2 = tmp;
	}

	re0 = re;
	im0 = im;
}

/**
 * @brief FIR filter's group delay calculation function.
 *
 * Function calculates group delay of the specified FIR filter at the
 * specified circular frequency. The group delay is calculated by evaluating
 * the filter's response at close side-band frequencies of "th".
 *
 * @param flt FIR filter's coefficients.
 * @param fltlen Number of coefficients (taps) in the filter.
 * @param th Circular frequency [0; pi].
 * @return Resulting group delay at the specified frequency, in samples.
 */

inline double calcFIRFilterGroupDelay( const double* const flt,
	const int fltlen, const double th )
{
	const int Count = 2;
	const double thd2 = 1e-9;
	double ths[ Count ] = { th - thd2, th + thd2 }; // Side-band frequencies.

	if( ths[ 0 ] < 0.0 )
	{
		ths[ 0 ] = 0.0;
	}

	if( ths[ 1 ] > R8B_PI )
	{
		ths[ 1 ] = R8B_PI;
	}

	double ph1[ Count ];
	int i;

	for( i = 0; i < Count; i++ )
	{
		double re1;
		double im1;

		calcFIRFilterResponse( flt, fltlen, ths[ i ], re1, im1 );
		ph1[ i ] = atan2( im1, re1 );
	}

	if( fabs( ph1[ 1 ] - ph1[ 0 ]) > R8B_PI )
	{
		if( ph1[ 1 ] > ph1[ 0 ])
		{
			ph1[ 1 ] -= R8B_2PI;
		}
		else
		{
			ph1[ 1 ] += R8B_2PI;
		}
	}

	const double thd = ths[ 1 ] - ths[ 0 ];

	return(( ph1[ 1 ] - ph1[ 0 ]) / thd );
}

/**
 * @brief FIR filter's gain normalization.
 *
 * Function normalizes FIR filter so that its frequency response at DC is
 * equal to DCGain.
 *
 * @param[in,out] p Filter coefficients.
 * @param l Filter length.
 * @param DCGain Filter's gain at DC (linear, non-decibel value).
 * @param pstep "p" array step.
 */

inline void normalizeFIRFilter( double* const p, const int l,
	const double DCGain, const int pstep = 1 )
{
	R8BASSERT( l > 0 );
	R8BASSERT( pstep != 0 );

	double s = 0.0;
	double* pp = p;
	int i = l;

	while( i > 0 )
	{
		s += *pp;
		pp += pstep;
		i--;
	}

	s = DCGain / s;
	pp = p;
	i = l;

	while( i > 0 )
	{
		*pp *= s;
		pp += pstep;
		i--;
	}
}

/**
 * @brief Calculates 3rd order spline coefficients, using 8 points.
 *
 * Function calculates coefficients used to calculate 3rd order spline
 * (polynomial) on the equidistant lattice, using 8 points.
 *
 * @param[out] c Output coefficients buffer, length = 4.
 * @param xm3 Point at x-3 position.
 * @param xm2 Point at x-2 position.
 * @param xm1 Point at x-1 position.
 * @param x0 Point at x position.
 * @param x1 Point at x+1 position.
 * @param x2 Point at x+2 position.
 * @param x3 Point at x+3 position.
 * @param x4 Point at x+4 position.
 */

inline void calcSpline3p8Coeffs( double* const c, const double xm3,
	const double xm2, const double xm1, const double x0, const double x1,
	const double x2, const double x3, const double x4 )
{
	c[ 0 ] = x0;
	c[ 1 ] = ( 61.0 * ( x1 - xm1 ) + 16.0 * ( xm2 - x2 ) +
		3.0 * ( x3 - xm3 )) * 1.31578947368421052e-2;

	c[ 2 ] = ( 106.0 * ( xm1 + x1 ) + 10.0 * x3 + 6.0 * xm3 - 3.0 * x4 -
		29.0 * ( xm2 + x2 ) - 167.0 * x0 ) * 1.31578947368421052e-2;

	c[ 3 ] = ( 91.0 * ( x0 - x1 ) + 45.0 * ( x2 - xm1 ) +
		13.0 * ( xm2 - x3 ) + 3.0 * ( x4 - xm3 )) * 1.31578947368421052e-2;
}

/**
 * @brief Calculates 2nd order spline coefficients, using 8 points.
 *
 * Function calculates coefficients used to calculate 2nd order spline
 * (polynomial) on the equidistant lattice, using 8 points. This function is
 * based on the calcSpline3p8Coeffs() function, but without the 3rd order
 * coefficient.
 *
 * @param[out] c Output coefficients buffer, length = 3.
 * @param xm3 Point at x-3 position.
 * @param xm2 Point at x-2 position.
 * @param xm1 Point at x-1 position.
 * @param x0 Point at x position.
 * @param x1 Point at x+1 position.
 * @param x2 Point at x+2 position.
 * @param x3 Point at x+3 position.
 * @param x4 Point at x+4 position.
 */

inline void calcSpline2p8Coeffs( double* const c, const double xm3,
	const double xm2, const double xm1, const double x0, const double x1,
	const double x2, const double x3, const double x4 )
{
	c[ 0 ] = x0;
	c[ 1 ] = ( 61.0 * ( x1 - xm1 ) + 16.0 * ( xm2 - x2 ) +
		3.0 * ( x3 - xm3 )) * 1.31578947368421052e-2;

	c[ 2 ] = ( 106.0 * ( xm1 + x1 ) + 10.0 * x3 + 6.0 * xm3 - 3.0 * x4 -
		29.0 * ( xm2 + x2 ) - 167.0 * x0 ) * 1.31578947368421052e-2;
}

/**
 * @brief Calculates 3rd order spline coefficients, using 4 points.
 *
 * Function calculates coefficients used to calculate 3rd order segment
 * interpolation polynomial on the equidistant lattice, using 4 points.
 *
 * @param[out] c Output coefficients buffer, length = 4.
 * @param[in] y Equidistant point values. Value at offset 1 corresponds to
 * x=0 point.
 */

inline void calcSpline3p4Coeffs( double* const c, const double* const y )
{
	c[ 0 ] = y[ 1 ];
	c[ 1 ] = 0.5 * ( y[ 2 ] - y[ 0 ]);
	c[ 2 ] = y[ 0 ] - 2.5 * y[ 1 ] + y[ 2 ] + y[ 2 ] - 0.5 * y[ 3 ];
	c[ 3 ] = 0.5 * ( y[ 3 ] - y[ 0 ] ) + 1.5 * ( y[ 1 ] - y[ 2 ]);
}

/**
 * @brief Calculates 3rd order spline coefficients, using 6 points.
 *
 * Function calculates coefficients used to calculate 3rd order segment
 * interpolation polynomial on the equidistant lattice, using 6 points.
 *
 * @param[out] c Output coefficients buffer, length = 4.
 * @param[in] y Equidistant point values. Value at offset 2 corresponds to
 * x=0 point.
 */

inline void calcSpline3p6Coeffs( double* const c, const double* const y )
{
	c[ 0 ] = y[ 2 ];
	c[ 1 ] = ( 11.0 * ( y[ 3 ] - y[ 1 ]) + 2.0 * ( y[ 0 ] - y[ 4 ])) / 14.0;
	c[ 2 ] = ( 20.0 * ( y[ 1 ] + y[ 3 ]) + 2.0 * y[ 5 ] - 4.0 * y[ 0 ] -
		7.0 * y[ 4 ] - 31.0 * y[ 2 ]) / 14.0;

	c[ 3 ] = ( 17.0 * ( y[ 2 ] - y[ 3 ]) + 9.0 * ( y[ 4 ] - y[ 1 ]) +
		2.0 * ( y[ 0 ] - y[ 5 ])) / 14.0;
}

#if !defined( min )

/**
 * @brief Returns minimum of two values.
 *
 * @param v1 Value 1.
 * @param v2 Value 2.
 * @tparam T Values' type.
 * @return The minimum of 2 values.
 */

template< typename T >
inline T min( const T& v1, const T& v2 )
{
	return( v1 < v2 ? v1 : v2 );
}

#endif // !defined( min )

#if !defined( max )

/**
 * @brief Returns maximum of two values.
 *
 * @param v1 Value 1.
 * @param v2 Value 2.
 * @tparam T Values' type.
 * @return The maximum of 2 values.
 */

template< typename T >
inline T max( const T& v1, const T& v2 )
{
	return( v1 > v2 ? v1 : v2 );
}

#endif // !defined( max )

/**
 * @brief Clamps a value to be within the specified min-max range.
 *
 * Function "clamps" (clips) the specified value so that it is not lesser than
 * "minv", and not greater than "maxv".
 *
 * @param Value Value to clamp.
 * @param minv Minimal allowed value.
 * @param maxv Maximal allowed value.
 * @return "Clamped" value.
 */

inline double clampr( const double Value, const double minv,
	const double maxv )
{
	if( Value < minv )
	{
		return( minv );
	}

	if( Value > maxv )
	{
		return( maxv );
	}

	return( Value );
}

/**
 * @brief Returns square ot a value.
 *
 * @param x Value to square.
 * @return Squared value of the argument.
 */

inline double sqr( const double x )
{
	return( x * x );
}

/**
 * @brief Power of an absolute value.
 *
 * @param v Input value.
 * @param p Power factor.
 * @return Returns a precise, but generally approximate pow() function's value
 * of absolute of input value.
 */

inline double pow_a( const double v, const double p )
{
	return( exp( p * log( fabs( v ) + 1e-300 )));
}

/**
 * @brief Single-argument Gaussian function of a value.
 *
 * @param v Input value.
 * @return Calculated single-argument Gaussian function of the input value.
 */

inline double gauss( const double v )
{
	return( exp( -( v * v )));
}

/**
 * @brief Hyperbolic sine of a value.
 *
 * @param v Input value.
 * @return Calculated inverse hyperbolic sine of the input value.
 */

inline double asinh( const double v )
{
	return( log( v + sqrt( v * v + 1.0 )));
}

/**
 * @brief 1st kind, 0th order modified Bessel function of a value.
 *
 * @param x Input value.
 * @return Calculated zero-th order modified Bessel function of the first kind
 * of the input value. Approximate value. Coefficients by Abramowitz and
 * Stegun.
 */

inline double besselI0( const double x )
{
	const double ax = fabs( x );
	double y;

	if( ax < 3.75 )
	{
		y = x / 3.75;
		y *= y;

		return( 1.0 + y * ( 3.5156229 + y * ( 3.0899424 + y * ( 1.2067492 +
			y * ( 0.2659732 + y * ( 0.360768e-1 + y * 0.45813e-2 ))))));
	}

	y = 3.75 / ax;

	return( exp( ax ) / sqrt( ax ) * ( 0.39894228 + y * ( 0.1328592e-1 +
		y * ( 0.225319e-2 + y * ( -0.157565e-2 + y * ( 0.916281e-2 +
		y * ( -0.2057706e-1 + y * ( 0.2635537e-1 + y * ( -0.1647633e-1 +
		y * 0.392377e-2 )))))))));
}

} // namespace r8b

#endif // R8BBASE_INCLUDED
