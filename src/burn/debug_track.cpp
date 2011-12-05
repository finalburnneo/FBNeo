// A module to track if various support devices, cpus, sound modules are in use

#include "burnint.h"

UINT8 Debug_BurnTransferInitted;
UINT8 Debug_BurnGunInitted;
UINT8 Debug_BurnLedInitted;
UINT8 Debug_HiscoreInitted;
UINT8 Debug_GenericTilesInitted;

UINT8 DebugDev_8255PPIInitted;
UINT8 DebugDev_EEPROMInitted;
UINT8 DebugDev_PandoraInitted;
UINT8 DebugDev_SeibuSndInitted;
UINT8 DebugDev_TimeKprInitted;

UINT8 DebugSnd_AY8910Initted;
UINT8 DebugSnd_Y8950Initted;
UINT8 DebugSnd_YM2151Initted;
UINT8 DebugSnd_YM2203Initted;
UINT8 DebugSnd_YM2413Initted;
UINT8 DebugSnd_YM2608Initted;
UINT8 DebugSnd_YM2610Initted;
UINT8 DebugSnd_YM2612Initted;
UINT8 DebugSnd_YM3526Initted;
UINT8 DebugSnd_YM3812Initted;
UINT8 DebugSnd_YMF278BInitted;
UINT8 DebugSnd_DACInitted;
UINT8 DebugSnd_ES5506Initted;
UINT8 DebugSnd_ES8712Initted;
UINT8 DebugSnd_ICS2115Initted;
UINT8 DebugSnd_IremGA20Initted;
UINT8 DebugSnd_K007232Initted;
UINT8 DebugSnd_K051649Initted;
UINT8 DebugSnd_K053260Initted;
UINT8 DebugSnd_K054539Initted;
UINT8 DebugSnd_MSM5205Initted;
UINT8 DebugSnd_MSM6295Initted;
UINT8 DebugSnd_NamcoSndInitted;
UINT8 DebugSnd_RF5C68Initted;
UINT8 DebugSnd_SAA1099Initted;
UINT8 DebugSnd_SamplesInitted;
UINT8 DebugSnd_SegaPCMInitted;
UINT8 DebugSnd_SN76496Initted;
UINT8 DebugSnd_UPD7759Initted;
UINT8 DebugSnd_X1010Initted;
UINT8 DebugSnd_YMZ280BInitted;

UINT8 DebugCPU_ARM7Initted;
UINT8 DebugCPU_ARMInitted;
UINT8 DebugCPU_H6280Initted;
UINT8 DebugCPU_HD6309Initted;
UINT8 DebugCPU_KonamiInitted;
UINT8 DebugCPU_M6502Initted;
UINT8 DebugCPU_M6800Initted;
UINT8 DebugCPU_M6805Initted;
UINT8 DebugCPU_M6809Initted;
UINT8 DebugCPU_S2650Initted;
UINT8 DebugCPU_SekInitted;
UINT8 DebugCPU_VezInitted;
UINT8 DebugCPU_ZetInitted;

UINT8 DebugCPU_I8039Initted;
UINT8 DebugCPU_SH2Initted;

#define CHECK_DEVICE_EXIT(DevName)	if (DebugDev_##DevName##Initted) bprintf(PRINT_ERROR, _T("Device " #DevName " Not Exited\n"))
#define CHECK_SOUND_EXIT(SndName)	if (DebugSnd_##SndName##Initted) bprintf(PRINT_ERROR, _T("Sound Module " #SndName " Not Exited\n"))
#define CHECK_CPU_EXIT(CpuName)		if (DebugCPU_##CpuName##Initted) bprintf(PRINT_ERROR, _T("CPU " #CpuName " Not Exited\n"))

void DebugTrackerExit()
{
	if (Debug_BurnTransferInitted) 		bprintf(PRINT_ERROR, _T("BurnTransfer Not Exited\n"));
	if (Debug_BurnGunInitted) 			bprintf(PRINT_ERROR, _T("BurnGun Not Exited\n"));
	if (Debug_BurnLedInitted) 			bprintf(PRINT_ERROR, _T("BurnLed Not Exited\n"));
	if (Debug_HiscoreInitted) 			bprintf(PRINT_ERROR, _T("Hiscore Not Exited\n"));
	if (Debug_GenericTilesInitted) 		bprintf(PRINT_ERROR, _T("GenericTiles Not Exited\n"));
	
	CHECK_DEVICE_EXIT(8255PPI);
	CHECK_DEVICE_EXIT(EEPROM);
	CHECK_DEVICE_EXIT(Pandora);
	CHECK_DEVICE_EXIT(SeibuSnd);
	CHECK_DEVICE_EXIT(TimeKpr);
	
	CHECK_SOUND_EXIT(AY8910);
	CHECK_SOUND_EXIT(Y8950);
	CHECK_SOUND_EXIT(YM2151);
	CHECK_SOUND_EXIT(YM2203);
	CHECK_SOUND_EXIT(YM2413);
	CHECK_SOUND_EXIT(YM2608);
	CHECK_SOUND_EXIT(YM2610);
	CHECK_SOUND_EXIT(YM2612);
	CHECK_SOUND_EXIT(YM3526);
	CHECK_SOUND_EXIT(YM3812);
	CHECK_SOUND_EXIT(YMF278B);
	CHECK_SOUND_EXIT(DAC);
	CHECK_SOUND_EXIT(ES5506);
	CHECK_SOUND_EXIT(ES8712);
	CHECK_SOUND_EXIT(ICS2115);
	CHECK_SOUND_EXIT(IremGA20);
	CHECK_SOUND_EXIT(K007232);
	CHECK_SOUND_EXIT(K051649);
	CHECK_SOUND_EXIT(K053260);
	CHECK_SOUND_EXIT(K054539);
	CHECK_SOUND_EXIT(MSM5205);
	CHECK_SOUND_EXIT(MSM6295);
	CHECK_SOUND_EXIT(NamcoSnd);
	CHECK_SOUND_EXIT(RF5C68);
	CHECK_SOUND_EXIT(SAA1099);
	CHECK_SOUND_EXIT(Samples);
	CHECK_SOUND_EXIT(SegaPCM);
	CHECK_SOUND_EXIT(SN76496);
	CHECK_SOUND_EXIT(UPD7759);
	CHECK_SOUND_EXIT(X1010);
	CHECK_SOUND_EXIT(YMZ280B);
	
	CHECK_CPU_EXIT(ARM7);
	CHECK_CPU_EXIT(ARM);
	CHECK_CPU_EXIT(H6280);
	CHECK_CPU_EXIT(HD6309);
	CHECK_CPU_EXIT(Konami);
	CHECK_CPU_EXIT(M6502);
	CHECK_CPU_EXIT(M6800);
	CHECK_CPU_EXIT(M6805);
	CHECK_CPU_EXIT(M6809);
	CHECK_CPU_EXIT(S2650);
	CHECK_CPU_EXIT(Sek);
	CHECK_CPU_EXIT(Vez);
	CHECK_CPU_EXIT(Zet);

	CHECK_CPU_EXIT(I8039);
	CHECK_CPU_EXIT(SH2);
}
