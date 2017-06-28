void mb87078_init(void (*callback)(INT32, INT32));
void mb87078_exit();
void mb87078_scan();
void mb87078_reset();
void mb87078_write(INT32 dsel, INT32 data);
float mb87078_gain_decibel_r(INT32 channel);
INT32 mb87078_gain_percent_r(INT32 channel);

