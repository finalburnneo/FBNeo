//$ nobt
//$ nocpp

/**
 * @file CDSPHBDownsampler.h
 *
 * @brief Half-band downsampling convolver class.
 *
 * This file includes half-band downsampling convolver class.
 *
 * r8brain-free-src Copyright (c) 2013-2025 Aleksey Vaneev
 *
 * See the "LICENSE" file for license.
 */

#ifndef R8B_CDSPHBDOWNSAMPLER_INCLUDED
#define R8B_CDSPHBDOWNSAMPLER_INCLUDED

#include "CDSPHBUpsampler.h"

namespace r8b {

/**
 * @brief Half-band downsampler class.
 *
 * Class implements brute-force half-band 2X downsampling that uses small
 * sparse symmetric FIR filters. The output has 2.0 gain.
 */

class CDSPHBDownsampler : public CDSPProcessor
{
public:
	/**
	 * @brief Initalizes the half-band downsampler.
	 *
	 * @param ReqAtten Required half-band filter attentuation.
	 * @param SteepIndex Steepness index - 0=steepest. Corresponds to general
	 * downsampling ratio, e.g., at 4x downsampling 0 is used, at 8x
	 * downsampling 1 is used, etc.
	 * @param IsThird `true` if 1/3 resampling is performed.
	 * @param PrevLatency Latency, in samples (any non-negative value), which
	 * was left in the output signal by a previous process. Whole-number
	 * latency will be consumed by *this* object while remaining fractional
	 * latency can be obtained via the getLatencyFrac() function.
	 */

	CDSPHBDownsampler( const double ReqAtten, const int SteepIndex,
		const bool IsThird, const double PrevLatency )
	{
		static const CConvolveFn FltConvFn[ 14 ] = {
			&CDSPHBDownsampler :: convolve1, &CDSPHBDownsampler :: convolve2,
			&CDSPHBDownsampler :: convolve3, &CDSPHBDownsampler :: convolve4,
			&CDSPHBDownsampler :: convolve5, &CDSPHBDownsampler :: convolve6,
			&CDSPHBDownsampler :: convolve7, &CDSPHBDownsampler :: convolve8,
			&CDSPHBDownsampler :: convolve9, &CDSPHBDownsampler :: convolve10,
			&CDSPHBDownsampler :: convolve11, &CDSPHBDownsampler :: convolve12,
			&CDSPHBDownsampler :: convolve13,
			&CDSPHBDownsampler :: convolve14 };

		const double* fltp0;
		int fltt;
		double att;

		if( IsThird )
		{
			CDSPHBUpsampler :: getHBFilterThird( ReqAtten, SteepIndex, fltp0,
				fltt, att );
		}
		else
		{
			CDSPHBUpsampler :: getHBFilter( ReqAtten, SteepIndex, fltp0, fltt,
				att );
		}

		// Copy obtained filter to address-aligned buffer.

		fltp = alignptr( FltBuf, 16 );
		memcpy( fltp, fltp0, (size_t) fltt * sizeof( fltp[ 0 ]));

		convfn = FltConvFn[ fltt - 1 ];
		fll = fltt;
		fl2 = fltt - 1;
		flo = fll + fl2;
		flb = BufLen - fll;
		BufRP1 = Buf1 + fll;
		BufRP2 = Buf2 + fll - 1;

		LatencyFrac = PrevLatency * 0.5;
		Latency = (int) LatencyFrac;
		LatencyFrac -= Latency;

		R8BASSERT( Latency >= 0 );

		R8BCONSOLE( "CDSPHBDownsampler: taps=%i third=%i att=%.1f io=1/2\n",
			fltt, (int) IsThird, att );

		clear();
	}

	virtual int getInLenBeforeOutPos( const int ReqOutPos ) const
	{
		return( flo + (int) (( Latency + LatencyFrac + ReqOutPos ) * 2.0 ));
	}

	virtual int getLatency() const
	{
		return( 0 );
	}

	virtual double getLatencyFrac() const
	{
		return( LatencyFrac );
	}

	virtual int getMaxOutLen( const int MaxInLen ) const
	{
		R8BASSERT( MaxInLen >= 0 );

		return(( MaxInLen + 1 ) >> 1 );
	}

	virtual void clear()
	{
		LatencyLeft = Latency;
		BufLeft = 0;
		WritePos1 = 0;
		WritePos2 = 0;
		ReadPos = flb; // Set "read" position to account for filter's latency.

		memset( &Buf1[ ReadPos ], 0,
			(size_t) ( BufLen - flb ) * sizeof( Buf1[ 0 ]));

		memset( &Buf2[ ReadPos ], 0,
			(size_t) ( BufLen - flb ) * sizeof( Buf2[ 0 ]));
	}

	virtual int process( double* ip, int l, double*& op0 )
	{
		R8BASSERT( l >= 0 );

		double* op = op0;

		while( l > 0 )
		{
			// Copy new input samples to 2 ring buffers.

			if( WritePos1 != WritePos2 )
			{
				// If previous fill was asymmetrical, put a single sample to
				// Buf2.

				double* const wp2 = Buf2 + WritePos2;
				*wp2 = *ip;

				if( WritePos2 < flo )
				{
					wp2[ BufLen ] = *ip;
				}

				ip++;
				WritePos2 = WritePos1;
				l--;
				BufLeft++;
			}

			const int b1 = min(( l + 1 ) >> 1,
				min( BufLen - WritePos1, flb - BufLeft ));

			const int b2 = b1 - ( b1 * 2 > l );

			double* wp1 = Buf1 + WritePos1;
			double* wp2 = Buf2 + WritePos1;
			double* const ipe = ip + b2 * 2;

			while( ip != ipe )
			{
				*wp1 = ip[ 0 ];
				*wp2 = ip[ 1 ];
				wp1++;
				wp2++;
				ip += 2;
			}

			if( b1 != b2 )
			{
				*wp1 = *ip;
				ip++;
			}

			const int ec = flo - WritePos1;

			if( ec > 0 )
			{
				wp1 = Buf1 + WritePos1;
				memcpy( wp1 + BufLen, wp1,
					(size_t) min( b1, ec ) * sizeof( wp1[ 0 ]));

				wp2 = Buf2 + WritePos1;
				memcpy( wp2 + BufLen, wp2,
					(size_t) min( b2, ec ) * sizeof( wp2[ 0 ]));
			}

			WritePos1 = ( WritePos1 + b1 ) & BufLenMask;
			WritePos2 = ( WritePos2 + b2 ) & BufLenMask;
			l -= b1 + b2;
			BufLeft += b2;

			// Produce output.

			const int c = BufLeft - fl2;

			if( c > 0 )
			{
				double* const opend = op + c;
				( *convfn )( op, opend, fltp, BufRP1, BufRP2, ReadPos );

				op = opend;
				ReadPos = ( ReadPos + c ) & BufLenMask;
				BufLeft -= c;
			}
		}

		int ol = (int) ( op - op0 );

		if( LatencyLeft != 0 )
		{
			if( LatencyLeft >= ol )
			{
				LatencyLeft -= ol;
				return( 0 );
			}

			ol -= LatencyLeft;
			op0 += LatencyLeft;
			LatencyLeft = 0;
		}

		return( ol );
	}

private:
	static const int BufLenBits = 10; ///< The length of the ring buffer,
		///< expressed as Nth power of 2. This value can be reduced if it is
		///< known that only short input buffers will be passed to the
		///< interpolator. The minimum value of this parameter is 5, and
		///< `1 << BufLenBits` should be at least 3 times larger than the
		///< FilterLen.
	static const int BufLen = 1 << BufLenBits; ///< The length of the ring
		///< buffer. The actual length is longer, to permit "beyond bounds"
		///< positioning.
	static const int BufLenMask = BufLen - 1; ///< Mask used for quick buffer
		///< position wrapping.
	double Buf1[ BufLen + 27 ]; ///< The ring buffer 1, including overrun
		///< protection for the largest filter.
	double Buf2[ BufLen + 27 ]; ///< The ring buffer 2, including overrun
		///< protection for the largest filter.
	double FltBuf[ 14 + 2 ]; ///< Holder for half-band filter taps, used with
		///< 16-byte address-aligning, for SIMD use.
	const double* BufRP1; ///< Offseted Buf1 pointer at `ReadPos` equal 0.
	const double* BufRP2; ///< Offseted Buf2 pointer at `ReadPos` equal 0.
	double* fltp; ///< Half-band filter taps, points to `FltBuf`.
	double LatencyFrac; ///< Fractional latency left on the output.
	int Latency; ///< Initial latency that should be removed from the output.
	int fll; ///< Input latency (left-hand filter length).
	int fl2; ///< Right-side filter length.
	int flo; ///< Overrun length.
	int flb; ///< Initial buffer read position.
	int LatencyLeft; ///< Latency left to remove.
	int BufLeft; ///< The number of samples left in the buffer to process.
		///< When this value is below `fl2`, the interpolation cycle ends.
	int WritePos1; ///< The current buffer 1 write position.
	int WritePos2; ///< The current buffer 2 write position. Incremented
		///< together with the `BufLeft` variable.
	int ReadPos; ///< The current buffer read position.

	typedef void( *CConvolveFn )( double* op, double* const opend,
		const double* const flt, const double* const rp01,
		const double* const rp02, int rpos ); ///<
		///< Convolution function type.
	CConvolveFn convfn; ///< Convolution function in use.

#define R8BHBC1( fn ) \
	static void fn( double* op, double* const opend, const double* const flt, \
		const double* const rp01, const double* const rp02, int rpos ) \
	{ \
		while( op != opend ) \
		{ \
			const double* const rp1 = rp01 + rpos; \
			const double* const rp = rp02 + rpos;

#define R8BHBC2 \
			rpos = ( rpos + 1 ) & BufLenMask; \
			op++; \
		} \
	}

#include "CDSPHBDownsampler.inc"

#undef R8BHBC1
#undef R8BHBC2
};

// ---------------------------------------------------------------------------

} // namespace r8b

#endif // R8B_CDSPHBDOWNSAMPLER_INCLUDED
