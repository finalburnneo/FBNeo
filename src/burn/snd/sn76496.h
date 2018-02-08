void SN76496Update(INT32 Num, INT16* pSoundBuf, INT32 Length);
void SN76496UpdateToBuffer(INT32 Num, INT16* pSoundBuf, INT32 Length); // output mono
void SN76496Write(INT32 Num, INT32 Data);
void SN76496StereoWrite(INT32 Num, INT32 Data);
void SN76489Init(INT32 Num, INT32 Clock, INT32 SignalAdd);
void SN76489AInit(INT32 Num, INT32 Clock, INT32 SignalAdd);
void SN76494Init(INT32 Num, INT32 Clock, INT32 SignalAdd);
void SN76496Init(INT32 Num, INT32 Clock, INT32 SignalAdd);
void SN76496SetRoute(INT32 Num, double nVolume, INT32 nRouteDir);
void SN76496Reset();
void SN76496Exit();
void SN76496Scan(INT32 nAction, INT32 *pnMin);
