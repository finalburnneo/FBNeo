//$ nobt
//$ nocpp

/**
 * @file CDSPSincFilterGen.h
 *
 * @brief Sinc function-based FIR filter generator class.
 *
 * This file includes the CDSPSincFilterGen class implementation that
 * generates FIR filters.
 *
 * r8brain-free-src Copyright (c) 2013-2025 Aleksey Vaneev
 *
 * See the "LICENSE" file for license.
 */

#ifndef R8B_CDSPSINCFILTERGEN_INCLUDED
#define R8B_CDSPSINCFILTERGEN_INCLUDED

#include "r8bbase.h"

namespace r8b {

/**
 * @brief Sinc function-based FIR filter generator class.
 *
 * Structure that holds state used to perform generation of sinc functions of
 * various types, windowed by the Blackman window by default (but the window
 * function can be changed if necessary).
 */

class CDSPSincFilterGen
{
public:
	double Len2; ///< Required half filter kernel's length in samples (can be
		///< a fractional value). Final physical kernel length will be
		///< provided in the `KernelLen` variable. `Len2` should be >= 2.
	double Len2i; ///< Equals `1.0 / Len2`, initialized and used by some window
		///< functions for optimization (should not be initialized by the
		///< caller).
	int KernelLen; ///< Resulting length of the filter kernel, this variable
		///< is set after the call to one of the "init" functions.
	int fl2; ///< Internal "half kernel length" value. This value can be used
		///< as filter's latency in samples (taps), this variable is set after
		///< the call to one of the "init" functions.

	double Freq1; ///< Required corner circular frequency 1 [0; pi]. Used only
		///< in the generateBand() function.
	double Freq2; ///< Required corner circular frequency 2 [0; pi]. Used only
		///< in the generateBand() function. The range [Freq1; Freq2] defines
		///< a pass band for the generateBand() function.
	double FracDelay; ///< Fractional delay in the range [0; 1], used only in
		///< the generateFrac() function. Note that the `FracDelay` parameter
		///< is actually inversed. At 0.0 value it produces 1 sample delay
		///< (with the latency equal to fl2), at 1.0 value it produces 0
		///< sample delay (with the latency equal to fl2 - 1).

	/**
	 * @brief Window function type.
	 */

	enum EWindowFunctionType
	{
		wftCosine, ///< Generalized cosine window function. No parameters
			///< required. The "Power" parameter is optional.
		wftKaiser, ///< Kaiser window function. Requires the "Beta" parameter.
			///< The "Power" parameter is optional.
		wftGaussian ///< Gaussian window function. Requires the "Sigma"
			///< parameter. The "Power" parameter is optional.
	};

	typedef double( CDSPSincFilterGen ::* CWindowFunc )(); ///< Window
		///< calculation function pointer type.

	/**
	 * @brief Initializes *this* structure for generation of a window
	 * function, odd-sized.
	 *
	 * @param WinType Window function type.
	 * @param Params Window function's parameters. If `nullptr`, the table
	 * values may be used.
	 * @param UsePower `true` if the power factor should be used to raise the
	 * window function. If `true`, the power factor should be specified as the
	 * last value in the `Params` array. If `Params` is `nullptr`, the table
	 * or default value of -1.0 (off) will be used.
	 */

	void initWindow( const EWindowFunctionType WinType = wftCosine,
		const double* const Params = R8B_NULL, const bool UsePower = false )
	{
		R8BASSERT( Len2 >= 2.0 );

		fl2 = (int) floor( Len2 );
		KernelLen = fl2 + fl2 + 1;

		setWindow( WinType, Params, UsePower, true );
	}

	/**
	 * @brief Initializes *this* structure for generation of band-limited
	 * sinc filter kernel.
	 *
	 * The generateBand() function should be used to calculate the filter.
	 *
	 * @param WinType Window function type.
	 * @param Params Window function's parameters. If `nullptr`, the table
	 * values may be used.
	 * @param UsePower `true` if the power factor should be used to raise the
	 * window function. If `true`, the power factor should be specified as the
	 * last value in the `Params` array. If `Params` is `nullptr`, the table
	 * or default value of -1.0 (off) will be used.
	 */

	void initBand( const EWindowFunctionType WinType = wftCosine,
		const double* const Params = R8B_NULL, const bool UsePower = false )
	{
		R8BASSERT( Len2 >= 2.0 );

		fl2 = (int) floor( Len2 );
		KernelLen = fl2 + fl2 + 1;

		setWindow( WinType, Params, UsePower, true );
	}

	/**
	 * @brief Initializes *this* structure for Hilbert transformation filter
	 * calculation.
	 *
	 * `Freq1` and `Freq2` variables are not used. The generateHilbert()
	 * function should be used to calculate the filter.
	 *
	 * @param WinType Window function type.
	 * @param Params Window function's parameters. If `nullptr`, the table
	 * values may be used.
	 * @param UsePower `true` if the power factor should be used to raise the
	 * window function. If `true`, the power factor should be specified as the
	 * last value in the `Params` array. If `Params` is `nullptr`, the table
	 * or default value of -1.0 (off) will be used.
	 */

	void initHilbert( const EWindowFunctionType WinType = wftCosine,
		const double* const Params = R8B_NULL, const bool UsePower = false )
	{
		R8BASSERT( Len2 >= 2.0 );

		fl2 = (int) floor( Len2 );
		KernelLen = fl2 + fl2 + 1;

		setWindow( WinType, Params, UsePower, true );
	}

	/**
	 * @brief Initializes *this* structure for generation of full-bandwidth
	 * fractional delay sinc filter kernel.
	 * 
	 * `Freq1` and `Freq2` variables are not used. The generateFrac() function
	 * should be used to calculate the filter.
	 *
	 * @param WinType Window function type.
	 * @param Params Window function's parameters. If `nullptr`, the table
	 * values may be used.
	 * @param UsePower `true` if the power factor should be used to raise the
	 * window function. If `true`, the power factor should be specified as the
	 * last value in the `Params` array. If `Params` is `nullptr`, the table
	 * or default value of -1.0 (off) will be used.
	 */

	void initFrac( const EWindowFunctionType WinType = wftCosine,
		const double* const Params = R8B_NULL, const bool UsePower = false )
	{
		R8BASSERT( Len2 >= 2.0 );

		fl2 = (int) ceil( Len2 );
		KernelLen = fl2 + fl2;

		setWindow( WinType, Params, UsePower, false, FracDelay );
	}

	/**
	 * @brief Returns the next "Hann" window function coefficient.
	 */

	double calcWindowHann()
	{
		return( 0.5 + 0.5 * w1.generate() );
	}

	/**
	 * @brief Returns the next "Hamming" window function coefficient.
	 */

	double calcWindowHamming()
	{
		return( 0.54 + 0.46 * w1.generate() );
	}

	/**
	 * @brief Returns the next "Blackman" window function coefficient.
	 */

	double calcWindowBlackman()
	{
		return( 0.42 + 0.5 * w1.generate() + 0.08 * w2.generate() );
	}

	/**
	 * @brief Returns the next "Nuttall" window function coefficient.
	 */

	double calcWindowNuttall()
	{
		return( 0.355768 + 0.487396 * w1.generate() +
			0.144232 * w2.generate() + 0.012604 * w3.generate() );
	}

	/**
	 * @brief Returns the next "Blackman-Nuttall" window function coefficient.
	 */

	double calcWindowBlackmanNuttall()
	{
		return( 0.3635819 + 0.4891775 * w1.generate() +
			0.1365995 * w2.generate() + 0.0106411 * w3.generate() );
	}

	/**
	 * @brief Returns the next "Kaiser" window function coefficient.
	 */

	double calcWindowKaiser()
	{
		const double n = 1.0 - sqr( wn * Len2i + KaiserLen2Frac );
		wn++;

		if( n <= 0.0 )
		{
			return( 0.0 );
		}

		return( besselI0( KaiserBeta * sqrt( n )) * KaiserMul );
	}

	/**
	 * @brief Returns the next "Gaussian" window function coefficient.
	 */

	double calcWindowGaussian()
	{
		const double f = exp( -0.5 * sqr( wn * GaussianSigmaI +
			GaussianSigmaFrac ));

		wn++;

		return( f );
	}

	/**
	 * @brief Calculates the window function only.
	 *
	 * @param[out] op Output buffer, length = KernelLen.
	 * @param wfunc Window calculation function to use.
	 */

	void generateWindow( double* op,
		CWindowFunc wfunc = &CDSPSincFilterGen :: calcWindowBlackman )
	{
		op += fl2;
		double* op2 = op;

		int l = fl2;

		if( Power < 0.0 )
		{
			*op = ( *this.*wfunc )();

			while( l > 0 )
			{
				const double v = ( *this.*wfunc )();

				op++;
				op2--;
				*op = v;
				*op2 = v;
				l--;
			}
		}
		else
		{
			*op = pow_a(( *this.*wfunc )(), Power );

			while( l > 0 )
			{
				const double v = pow_a(( *this.*wfunc )(), Power );

				op++;
				op2--;
				*op = v;
				*op2 = v;
				l--;
			}
		}
	}

	/**
	 * @brief Calculates band-limited windowed sinc function-based filter
	 * kernel.
	 *
	 * @param[out] op Output buffer, length = KernelLen.
	 * @param wfunc Window calculation function to use.
	 */

	void generateBand( double* op,
		CWindowFunc wfunc = &CDSPSincFilterGen :: calcWindowBlackman )
	{
		CSineGen f2( Freq2, 0.0, 1.0 / R8B_PI );
		f2.generate();

		op += fl2;
		double* op2 = op;
		const double pw = Power;
		int t = 1;

		if( Freq1 < 2.3e-13 )
		{
			if( pw < 0.0 )
			{
				*op = Freq2 * ( *this.*wfunc )() / R8B_PI;

				while( t <= fl2 )
				{
					const double v = f2.generate() * ( *this.*wfunc )() / t;
					op++;
					op2--;
					*op = v;
					*op2 = v;
					t++;
				}
			}
			else
			{
				*op = Freq2 * pow_a(( *this.*wfunc )(), pw ) / R8B_PI;

				while( t <= fl2 )
				{
					const double v = f2.generate() *
						pow_a(( *this.*wfunc )(), pw ) / t;

					op++;
					op2--;
					*op = v;
					*op2 = v;
					t++;
				}
			}
		}
		else
		{
			CSineGen f1( Freq1, 0.0, 1.0 / R8B_PI );
			f1.generate();

			if( pw < 0.0 )
			{
				*op = ( Freq2 - Freq1 ) * ( *this.*wfunc )() / R8B_PI;

				while( t <= fl2 )
				{
					const double v = ( f2.generate() - f1.generate() ) *
						( *this.*wfunc )() / t;

					op++;
					op2--;
					*op = v;
					*op2 = v;
					t++;
				}
			}
			else
			{
				*op = ( Freq2 - Freq1 ) *
					pow_a(( *this.*wfunc )(), pw ) / R8B_PI;

				while( t <= fl2 )
				{
					const double v = ( f2.generate() - f1.generate() ) *
						pow_a(( *this.*wfunc )(), pw ) / t;

					op++;
					op2--;
					*op = v;
					*op2 = v;
					t++;
				}
			}
		}
	}

	/**
	 * @brief Calculates windowed Hilbert transformer filter kernel.
	 *
	 * @param[out] op Output buffer, length = KernelLen.
	 * @param wfunc Window calculation function to use.
	 */

	void generateHilbert( double* op,
		CWindowFunc wfunc = &CDSPSincFilterGen :: calcWindowBlackman )
	{
		static const double fvalues[ 2 ] = { 0.0, 2.0 / R8B_PI };
		op += fl2;
		double* op2 = op;

		( *this.*wfunc )();
		*op = 0.0;

		int t = 1;

		if( Power < 0.0 )
		{
			while( t <= fl2 )
			{
				const double v = fvalues[ t & 1 ] * ( *this.*wfunc )() / t;
				op++;
				op2--;
				*op = v;
				*op2 = -v;
				t++;
			}
		}
		else
		{
			while( t <= fl2 )
			{
				const double v = fvalues[ t & 1 ] *
					pow_a(( *this.*wfunc )(), Power ) / t;

				op++;
				op2--;
				*op = v;
				*op2 = -v;
				t++;
			}
		}
	}

	/**
	 * @brief Calculates windowed fractional delay filter kernel.
	 *
	 * @param[out] op Output buffer, length = KernelLen.
	 * @param wfunc Window calculation function to use.
	 * @param opinc Output buffer increment, in "op" elements.
	 */

	void generateFrac( double* op,
		CWindowFunc wfunc = &CDSPSincFilterGen :: calcWindowBlackman,
		const int opinc = 1 )
	{
		R8BASSERT( opinc != 0 );

		const double pw = Power;
		const double fd = FracDelay;
		int t = -fl2;

		if( t + fd < -Len2 )
		{
			( *this.*wfunc )();
			*op = 0.0;
			op += opinc;
			t++;
		}

		double f = sin( fd * R8B_PI ) / R8B_PI;

		if(( t & 1 ) != 0 )
		{
			f = -f;
		}

		int IsZeroX = ( fabs( fd - 1.0 ) < 2.3e-13 );
		int mt = 0 - IsZeroX;
		IsZeroX = ( IsZeroX || fabs( fd ) < 2.3e-13 );

		if( pw < 0.0 )
		{
			while( t < mt )
			{
				*op = f * ( *this.*wfunc )() / ( t + fd );
				op += opinc;
				t++;
				f = -f;
			}

			if( IsZeroX ) // t+FracDelay==0
			{
				*op = ( *this.*wfunc )();
			}
			else
			{
				*op = f * ( *this.*wfunc )() / fd; // t==0
			}

			mt = fl2 - 2;

			while( t < mt )
			{
				op += opinc;
				t++;
				f = -f;
				*op = f * ( *this.*wfunc )() / ( t + fd );
			}

			op += opinc;
			t++;
			f = -f;
			const double ut = t + fd;
			*op = ( ut > Len2 ? 0.0 : f * ( *this.*wfunc )() / ut );
		}
		else
		{
			while( t < mt )
			{
				*op = f * pow_a(( *this.*wfunc )(), pw ) / ( t + fd );
				op += opinc;
				t++;
				f = -f;
			}

			if( IsZeroX ) // t+FracDelay==0
			{
				*op = pow_a(( *this.*wfunc )(), pw );
			}
			else
			{
				*op = f * pow_a(( *this.*wfunc )(), pw ) / fd; // t==0
			}

			mt = fl2 - 2;

			while( t < mt )
			{
				op += opinc;
				t++;
				f = -f;
				*op = f * pow_a(( *this.*wfunc )(), pw ) / ( t + fd );
			}

			op += opinc;
			t++;
			f = -f;
			const double ut = t + FracDelay;
			*op = ( ut > Len2 ? 0.0 :
				f * pow_a(( *this.*wfunc )(), pw ) / ut );
		}
	}

private:
	double Power; ///< The power factor used to raise the window function.
		///< Equals a negative value if the power factor should not be used.
	CSineGen w1; ///< Cosine wave 1 for window function.
	CSineGen w2; ///< Cosine wave 2 for window function.
	CSineGen w3; ///< Cosine wave 3 for window function.

	double KaiserBeta; ///< Kaiser window function's "Beta" coefficient.
	double KaiserMul; ///< Kaiser window function's divisor, inverse.
	double KaiserLen2Frac; ///< Equals `FracDelay / Len2`.
	double GaussianSigmaI; ///< Gaussian window function's "Sigma"
		///< coefficient, inverse.
	double GaussianSigmaFrac; ///< Equals `FracDelay / GaussianSigma`.

	int wn; ///< Window function integer position. 0 - center of the window
		///< function. This variable may not be used by some window functions.

	/**
	 * @brief Initializes Kaiser window function calculation.
	 * 
	 * The `FracDelay` variable should be initialized when using this window
	 * function.
	 *
	 * @param Params Function parameters. If `nullptr`, the default values
	 * will be used; otherwise, the first parameter should specify the "Beta"
	 * value.
	 * @param UsePower `true` if the power factor should be used to raise the
	 * window function.
	 * @param IsCentered `true` if centered window should be used. This
	 * parameter usually equals to `false` for fractional delay filters only.
	 */

	void setWindowKaiser( const double* Params, const bool UsePower,
		const bool IsCentered )
	{
		wn = ( IsCentered ? 0 : -fl2 );

		if( Params == R8B_NULL )
		{
			KaiserBeta = 9.5945013206755156;
			Power = ( UsePower ? 1.9718457932433306 : -1.0 );
		}
		else
		{
			KaiserBeta = clampr( Params[ 0 ], 1.0, 350.0 );
			Power = ( UsePower ? fabs( Params[ 1 ]) : -1.0 );
		}

		KaiserMul = 1.0 / besselI0( KaiserBeta );
		Len2i = 1.0 / Len2;
		KaiserLen2Frac = FracDelay * Len2i;
	}

	/**
	 * @brief Initializes Gaussian window function calculation.
	 *
	 * The `FracDelay` variable should be initialized when using this window
	 * function.
	 *
	 * @param Params Function parameters. If `nullptr`, the table values will
	 * be used; otherwise, the first parameter should specify the "Sigma"
	 * value.
	 * @param UsePower `true` if the power factor should be used to raise the
	 * window function.
	 * @param IsCentered `true` if centered window should be used. This
	 * parameter usually equals to `false` for fractional delay filters only.
	 */

	void setWindowGaussian( const double* Params, const bool UsePower,
		const bool IsCentered )
	{
		wn = ( IsCentered ? 0 : -fl2 );

		if( Params == R8B_NULL )
		{
			GaussianSigmaI = 1.0;
			Power = -1.0;
		}
		else
		{
			GaussianSigmaI = clampr( fabs( Params[ 0 ]), 1e-1, 100.0 );
			Power = ( UsePower ? fabs( Params[ 1 ]) : -1.0 );
		}

		GaussianSigmaI *= Len2;
		GaussianSigmaI = 1.0 / GaussianSigmaI;
		GaussianSigmaFrac = FracDelay * GaussianSigmaI;
	}

	/**
	 * @brief Initializes calculation of window function of the specified
	 * type.
	 *
	 * @param WinType Window function type.
	 * @param Params Window function's parameters. If `nullptr`, the table
	 * values may be used.
	 * @param UsePower `true` if the power factor should be used to raise the
	 * window function. If `true`, the power factor should be specified as the
	 * last value in the `Params` array. If `Params` is `nullptr`, the table
	 * or default value of -1.0 (off) will be used.
	 * @param IsCentered `true` if centered window should be used. This
	 * parameter usually equals to `false` for fractional delay filters only.
	 * @param UseFracDelay Fractional delay to use.
	 */

	void setWindow( const EWindowFunctionType WinType,
		const double* const Params, const bool UsePower,
		const bool IsCentered, const double UseFracDelay = 0.0 )
	{
		FracDelay = UseFracDelay;

		if( WinType == wftCosine )
		{
			if( IsCentered )
			{
				w1.init( R8B_PI / Len2, R8B_PId2 );
				w2.init( R8B_2PI / Len2, R8B_PId2 );
				w3.init( R8B_3PI / Len2, R8B_PId2 );
			}
			else
			{
				const double step1 = R8B_PI / Len2;
				w1.init( step1, R8B_PId2 - step1 * fl2 + step1 * FracDelay );

				const double step2 = R8B_2PI / Len2;
				w2.init( step2, R8B_PId2 - step2 * fl2 + step2 * FracDelay );

				const double step3 = R8B_3PI / Len2;
				w3.init( step3, R8B_PId2 - step3 * fl2 + step3 * FracDelay );
			}

			Power = ( UsePower && Params != R8B_NULL ? Params[ 0 ] : -1.0 );
		}
		else
		if( WinType == wftKaiser )
		{
			setWindowKaiser( Params, UsePower, IsCentered );
		}
		else
		if( WinType == wftGaussian )
		{
			setWindowGaussian( Params, UsePower, IsCentered );
		}
	}
};

} // namespace r8b

#endif // R8B_CDSPSINCFILTERGEN_INCLUDED
