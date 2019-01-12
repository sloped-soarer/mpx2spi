//
 
void PROTOCOL_SetBindState(u32 msec);
void CLOCK_StartTimer(u16 us, u16 (*cb)(void));
u32 CLOCK_getms(void);
void CLOCK_delayms(u32 delay_ms);
void CLOCK_StopTimer();
u32 Crc(const void *buffer, u32 size);
u8 read_bind_sw(void);
