#define MAX_NETS 3
#define MAX_RES_PER_NET 18

#define combine_6_weights(tab,w0,w1,w2,w3,w4,w5)        ((int)(((tab)[0]*(w0) + (tab)[1]*(w1) + (tab)[2]*(w2) + (tab)[3]*(w3) + (tab)[4]*(w4) + (tab)[5]*(w5)) + 0.5))

double compute_resistor_weights(INT32 minval, INT32 maxval, double scaler, INT32 count_1, const INT32 * resistances_1, double * weights_1, INT32 pulldown_1, INT32 pullup_1, INT32 count_2, const INT32 * resistances_2, double * weights_2, INT32 pulldown_2, INT32 pullup_2, INT32 count_3, const INT32 * resistances_3, double * weights_3, INT32 pulldown_3, INT32 pullup_3);