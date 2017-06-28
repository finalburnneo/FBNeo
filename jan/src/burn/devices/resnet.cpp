// Based on MAME sources by Couriersud

#include "burnint.h"
#include "resnet.h"

double compute_resistor_weights(INT32 minval, INT32 maxval, double scaler, INT32 count_1, const INT32 * resistances_1, double * weights_1, INT32 pulldown_1, INT32 pullup_1, INT32 count_2, const INT32 * resistances_2, double * weights_2, INT32 pulldown_2, INT32 pullup_2, INT32 count_3, const INT32 * resistances_3, double * weights_3, INT32 pulldown_3, INT32 pullup_3)
{
	INT32 networks_no;

	INT32 rescount[MAX_NETS];     /* number of resistors in each of the nets */
	double r[MAX_NETS][MAX_RES_PER_NET];        /* resistances */
	double w[MAX_NETS][MAX_RES_PER_NET];        /* calulated weights */
	double ws[MAX_NETS][MAX_RES_PER_NET];   /* calulated, scaled weights */
	INT32 r_pd[MAX_NETS];         /* pulldown resistances */
	INT32 r_pu[MAX_NETS];         /* pullup resistances */

	double max_out[MAX_NETS];
	double * out[MAX_NETS];

	INT32 i,j,n;
	double scale;
	double max;

	/* parse input parameters */

	networks_no = 0;
	for (n = 0; n < MAX_NETS; n++)
	{
		INT32 count, pd, pu;
		const INT32 * resistances;
		double * weights;

		switch(n){
		case 0:
				count       = count_1;
				resistances = resistances_1;
				weights     = weights_1;
				pd          = pulldown_1;
				pu          = pullup_1;
				break;
		case 1:
				count       = count_2;
				resistances = resistances_2;
				weights     = weights_2;
				pd          = pulldown_2;
				pu          = pullup_2;
				break;
		case 2:
		default:
				count       = count_3;
				resistances = resistances_3;
				weights     = weights_3;
				pd          = pulldown_3;
				pu          = pullup_3;
				break;
		}

		/* parameters validity check */
		if (count > MAX_RES_PER_NET)
			bprintf(PRINT_ERROR, _T("compute_resistor_weights(): too many resistors in net #%i. The maximum allowed is %i, the number requested was: %i\n"), n, MAX_RES_PER_NET, count);

		if (count > 0)
		{
			rescount[networks_no] = count;
			for (i=0; i < count; i++)
			{
				r[networks_no][i] = 1.0 * resistances[i];
			}
			out[networks_no] = weights;
			r_pd[networks_no] = pd;
			r_pu[networks_no] = pu;
			networks_no++;
		}
	}
	if (networks_no < 1)
		bprintf(PRINT_ERROR, _T("compute_resistor_weights(): no input data\n"));

	/* calculate outputs for all given networks */
	for( i = 0; i < networks_no; i++ )
	{
		double R0, R1, Vout, dst;

		/* of n resistors */
		for(n = 0; n < rescount[i]; n++)
		{
			R0 = ( r_pd[i] == 0 ) ? 1.0/1e12 : 1.0/r_pd[i];
			R1 = ( r_pu[i] == 0 ) ? 1.0/1e12 : 1.0/r_pu[i];

			for( j = 0; j < rescount[i]; j++ )
			{
				if( j==n )  /* only one resistance in the network connected to Vcc */
				{
					if (r[i][j] != 0.0)
						R1 += 1.0/r[i][j];
				}
				else
					if (r[i][j] != 0.0)
						R0 += 1.0/r[i][j];
			}

			/* now determine the voltage */
			R0 = 1.0/R0;
			R1 = 1.0/R1;
			Vout = (maxval - minval) * R0 / (R1 + R0) + minval;

			/* and convert it to a destination value */
			dst = (Vout < minval) ? minval : (Vout > maxval) ? maxval : Vout;

			w[i][n] = dst;
		}
	}

	/* calculate maximum outputs for all given networks */
	j = 0;
	max = 0.0;
	for( i = 0; i < networks_no; i++ )
	{
		double sum = 0.0;

		/* of n resistors */
		for( n = 0; n < rescount[i]; n++ )
			sum += w[i][n]; /* maximum output, ie when each resistance is connected to Vcc */

		max_out[i] = sum;
		if (max < sum)
		{
			max = sum;
			j = i;
		}
	}


	if (scaler < 0.0)   /* use autoscale ? */
		/* calculate the output scaler according to the network with the greatest output */
		scale = ((double)maxval) / max_out[j];
	else                /* use scaler provided on entry */
		scale = scaler;

	/* calculate scaled output and fill the output table(s)*/
	for(i = 0; i < networks_no;i++)
	{
		for (n = 0; n < rescount[i]; n++)
		{
			ws[i][n] = w[i][n]*scale;   /* scale the result */
			(out[i])[n] = ws[i][n];     /* fill the output table */
		}
	}

	return (scale);

}
