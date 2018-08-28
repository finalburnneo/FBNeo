UINT8 usb_sound_status_read();
void usb_sound_data_write(UINT8 data);
UINT8 usb_sound_prgram_read(UINT16 offset);
void usb_sound_prgram_write(UINT16 offset, UINT8 data);
void usb_sound_reset();
void usb_sound_init(INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ);
void usb_sound_exit();
void usb_sound_scan(INT32 nAction, INT32 *);

void usb_timer_t1_clock(); // 7812.5 per sec / 195.3125 per frame (40hz)
INT32 usb_sound_run(INT32 cycles);
void segausb_update(INT16 *outputs, INT32 sample_len);

