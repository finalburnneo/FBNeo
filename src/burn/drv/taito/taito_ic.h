extern INT32 TaitoIC_PC080SNInUse;
extern INT32 TaitoIC_PC090OJInUse;
extern INT32 TaitoIC_TC0100SCNInUse;
extern INT32 TaitoIC_TC0110PCRInUse;
extern INT32 TaitoIC_TC0140SYTInUse;
extern INT32 TaitoIC_TC0150RODInUse;
extern INT32 TaitoIC_TC0180VCUInUse;
extern INT32 TaitoIC_TC0220IOCInUse;
extern INT32 TaitoIC_TC0280GRDInUse;
extern INT32 TaitoIC_TC0360PRIInUse;
extern INT32 TaitoIC_TC0430GRWInUse;
extern INT32 TaitoIC_TC0480SCPInUse;
extern INT32 TaitoIC_TC0510NIOInUse;
extern INT32 TaitoIC_TC0640FIOInUse;

extern INT32 TaitoWatchdog;

void TaitoICReset();
void TaitoICExit();
void TaitoICScan(INT32 nAction);

// Emulated C-Chip
#include "cchip.h"

// PC080SN
#define PC080SN_MAX_CHIPS 2

extern UINT8 *PC080SNRam[PC080SN_MAX_CHIPS];

void PC080SNDrawBgLayer(INT32 Chip, INT32 Opaque, UINT8 *pSrc, UINT16 *pDest);
void PC080SNDrawFgLayer(INT32 Chip, INT32 Opaque, UINT8 *pSrc, UINT16 *pDest);
void PC080SNDrawBgLayerPrio(INT32 Chip, INT32 Opaque, UINT8 *pSrc, UINT16 *pDest, UINT16 *PriBuf, UINT16 Prio);
void PC080SNDrawFgLayerPrio(INT32 Chip, INT32 Opaque, UINT8 *pSrc, UINT16 *pDest, UINT16 *PriBuf, UINT16 Prio);
void PC080SNSetScrollX(INT32 Chip, UINT32 Offset, UINT16 Data);
void PC080SNSetScrollY(INT32 Chip, UINT32 Offset, UINT16 Data);
void PC080SNCtrlWrite(INT32 Chip, UINT32 Offset, UINT16 Data);
void PC080SNOverrideFgScroll(INT32 Chip, INT32 xScroll, INT32 yScroll);
void PC080SNReset();
void PC080SNInit(INT32 Chip, INT32 nNumTiles, INT32 xOffset, INT32 yOffset, INT32 yInvert, INT32 DblWidth);
void PC080SNSetFgTransparentPen(INT32 Chip, INT32 Pen);
void PC080SNExit();
void PC080SNScan(INT32 nAction);
void TopspeedDrawBgLayer(INT32 Chip, UINT8 *pSrc, UINT16 *pDest, UINT16 *ColourCtrlRam, UINT16 *PriBuf, UINT16 Prio);
void TopspeedDrawFgLayer(INT32 Chip, UINT8 *pSrc, UINT16 *pDest, UINT16 *ColourCtrlRam, UINT16 *PriBuf, UINT16 Prio);

// PC090OJ
extern UINT8 *PC090OJRam;
extern INT32 PC090OJSpriteCtrl;

void PC090OJDrawSprites(UINT8 *pSrc);
void PC090OJBufferSprites();
void PC090OJReset();
void PC090OJInit(INT32 nNumTiles, INT32 xOffset, INT32 yOffset, INT32 UseBuffer);
void PC090OJSetDisableFlipping(INT32 val);
void PC090OJSetPaletteOffset(INT32 Offset);
INT32 PC090OJGetFlipped();
void PC090OJExit();
void PC090OJScan(INT32 nAction);

// TC0100SCN
#define TC0100SCN_MAX_CHIPS 3

extern UINT8 *TC0100SCNRam[TC0100SCN_MAX_CHIPS];
extern UINT16 TC0100SCNCtrl[TC0100SCN_MAX_CHIPS][8];
extern UINT8 TC0100SCNBgLayerUpdate[TC0100SCN_MAX_CHIPS];
extern UINT8 TC0100SCNFgLayerUpdate[TC0100SCN_MAX_CHIPS];
extern UINT8 TC0100SCNCharLayerUpdate[TC0100SCN_MAX_CHIPS];
extern UINT8 TC0100SCNCharRamUpdate[TC0100SCN_MAX_CHIPS];
extern INT32 TC0100SCNDblWidth[TC0100SCN_MAX_CHIPS];

void TC0100SCNCtrlWordWrite(INT32 Chip, UINT32 Offset, UINT16 Data);
INT32 TC0100SCNBottomLayer(INT32 Chip);
void TC0100SCNRenderBgLayer(INT32 Chip, INT32 Opaque, UINT8 *pSrc, INT32 Priority = 1);
void TC0100SCNRenderFgLayer(INT32 Chip, INT32 Opaque, UINT8 *pSrc, INT32 Priority = 2);
void TC0100SCNRenderCharLayer(INT32 Chip, INT32 Priority = 4);
void TC0100SCNReset();
void TC0100SCNInit(INT32 Chip, INT32 nNumTiles, INT32 xOffset, INT32 yOffset, INT32 xFlip, UINT8 *PriorityMap);
void TC0100SCNSetColourDepth(INT32 Chip, INT32 ColourDepth);
void TC0100SCNSetGfxMask(INT32 Chip, INT32 Mask);
void TC0100SCNSetGfxBank(INT32 Chip, INT32 Bank);
void TC0100SCNSetCharLayerGranularity(INT32 nGranularity);
void TC0100SCNSetClipArea(INT32 Chip, INT32 ClipWidth, INT32 ClipHeight, INT32 ClipStartX);
void TC0100SCNSetFlippedOffsets(INT32 XOffs, INT32 YOffs); // see note in tc0100scn.cpp
INT32 TC0100SCNGetFlipped(INT32 Chip);
void TC0100SCNSetPaletteOffset(INT32 Chip, INT32 PaletteOffset);
void TC0100SCNExit();
void TC0100SCNScan(INT32 nAction);
void TC0100SCNLiquidKludge();

// TC0110PCR
extern UINT32 *TC0110PCRPalette;
extern INT32 TC0110PCRTotalColours;

UINT16 TC0110PCRWordRead(INT32 Chip);
void TC0110PCRWordWrite(INT32 Chip, INT32 Offset, UINT16 Data);
void TC0110PCRStep1WordWrite(INT32 Chip, INT32 Offset, UINT16 Data);
void TC0110PCRStep1WordWriteAJ(INT32 Chip, INT32 Offset, UINT16 Data);
void TC0110PCRStep1RBSwapWordWrite(INT32 Chip, INT32 Offset, UINT16 Data);
void TC0110PCRStep14rbgWordWrite(INT32 Chip, INT32 Offset, UINT16 Data);
void TC0110PCRRecalcPalette(); // this is _not_ generic! (choose one based on game requirements)
void TC0110PCRRecalcPaletteStep1();
void TC0110PCRRecalcPaletteStep1RBSwap();
void TC0110PCRRecalcPaletteStep14rbg();
void TC0110PCRReset();
void TC0110PCRInit(INT32 Num, INT32 nNumColours);
void TC0110PCRExit();
void TC0110PCRScan(INT32 nAction);

// TC0140SYT
void TC0140SYTPortWrite(UINT8 Data);
UINT8 TC0140SYTCommRead();
void TC0140SYTCommWrite(UINT8 Data);
void TC0140SYTSlavePortWrite(UINT8 Data);
UINT8 TC0140SYTSlaveCommRead();
void TC0140SYTSlaveCommWrite(UINT8 Data);
void TC0140SYTReset();
void TC0140SYTInit(INT32 nCpu);
void TC0140SYTExit();
void TC0140SYTScan(INT32 nAction);

// TC0150ROD
extern UINT8 *TC0150RODRom;
extern UINT8 *TC0150RODRam;

void TC0150RODDraw(INT32 yOffs, INT32 pOffs, INT32 Type, INT32 RoadTrans, INT32 LowPriority, INT32 HighPriority);
void TC0150RODReset();
void TC0150RODInit(INT32 nRomSize, INT32 xFlip);
void TC0150RODSetPriMap(UINT8 *PriMap);
void TC0150RODExit();
void TC0150RODScan(INT32 nAction);

// TC0180VCU
extern UINT8 *TC0180VCURAM;
extern UINT8 *TC0180VCUScrollRAM;
extern UINT8 *TC0180VCUFbRAM;

void TC0180VCUInit(UINT8 *gfx0, INT32 mask0, UINT8 *gfx1, INT32 mask1, INT32 global_x, INT32 global_y);
void TC0180VCUReset();
void TC0180VCUExit();
void TC0180VCUScan(INT32 nAction);

void TC0180VCUDrawCharLayer(INT32 colorbase);
void TC0180VCUDrawLayer(INT32 colorbase, INT32 ctrl_offset, INT32 transparent);

void TC0180VCUFramebufferDraw(INT32 priority, INT32 color_base);
void TC0180VCUDrawSprite(UINT16 *dest);
void TC0180VCUBufferSprites();

UINT16 TC0180VCUFramebufferRead(INT32 offset);
void TC0180VCUFramebufferWrite(INT32 offset);

UINT8 TC0180VCUReadRegs(INT32 offset);
void TC0180VCUWriteRegs(INT32 offset, INT32 data);
UINT8 TC0180VCUReadControl();

// TC0220IOC
extern UINT8 TC0220IOCInputPort0[8];
extern UINT8 TC0220IOCInputPort1[8];
extern UINT8 TC0220IOCInputPort2[8];
extern UINT8 TC0220IOCDip[2];
extern UINT8 TC0220IOCInput[6];

UINT8 TC0220IOCPortRead();
UINT8 TC0220IOCHalfWordPortRead();
UINT8 TC0220IOCPortRegRead();
UINT8 TC0220IOCHalfWordRead(INT32 Offset);
UINT8 TC0220IOCRead(UINT8 Port);
void TC0220IOCWrite(UINT8 Port, UINT8 Data);
void TC0220IOCHalfWordPortRegWrite(UINT16 Data);
void TC0220IOCHalfWordPortWrite(UINT16 Data);
void TC0220IOCHalfWordWrite(INT32 Offset, UINT16 Data);
void TC0220IOCReset();
void TC0220IOCInit();
void TC0220IOCExit();
void TC0220IOCScan(INT32 nAction);

// TC0280GRD
extern UINT8 *TC0280GRDRam;
extern INT32 TC0280GRDBaseColour;

void TC0280GRDRenderLayer(INT32 Priority);
void TC0280GRDCtrlWordWrite(UINT32 Offset, UINT16 Data);
void TC0280GRDReset();
void TC0280GRDInit(INT32 xOffs, INT32 yOffs, UINT8 *pSrc);
void TC0430GRWInit(INT32 xOffs, INT32 yOffs, UINT8 *pSrc);
void TC0280GRDExit();
void TC0280GRDScan(INT32 nAction);
void TC0280GRDSetPriMap(UINT8 *PriMap);

#define TC0430GRWRam		TC0280GRDRam
#define TC0430GRWRenderLayer	TC0280GRDRenderLayer
#define TC0430GRWCtrlWordWrite	TC0280GRDCtrlWordWrite
#define TC0430GRWReset		TC0280GRDReset
#define TC0430GRWExit		TC0280GRDExit
#define TC0430GRWScan		TC0280GRDScan
#define TC0430GRDSetPriMap  TC0280GRDSetPriMap

// TC0360PRI
extern UINT8 TC0360PRIRegs[16];

void TC0360PRIWrite(UINT32 Offset, UINT8 Data);
void TC0360PRIHalfWordWrite(UINT32 Offset, UINT16 Data);
void TC0360PRIHalfWordSwapWrite(UINT32 Offset, UINT16 Data);
void TC0360PRIReset();
void TC0360PRIInit();
void TC0360PRIExit();
void TC0360PRIScan(INT32 nAction);

// TC0480SCP
extern UINT8 *TC0480SCPRam;
extern UINT16 TC0480SCPCtrl[0x18];

void TC0480SCPCtrlWordWrite(INT32 Offset, UINT16 Data);
void TC0480SCPTilemapRender(INT32 Layer, INT32 Opaque, UINT8 *pSrc);
void TC0480SCPTilemapRenderPrio(INT32 Layer, INT32 Opaque, INT32 Prio, UINT8 *pSrc);
void TC0480SCPRenderCharLayer(INT32 Prio = -1);
void TC0480SCPReset();
INT32 TC0480SCPGetBgPriority();
INT32 TC0480SCPGetBgPriReg();
void TC0480SCPSetPriMap(UINT8 *PriMap);
void TC0480SCPInit(INT32 nNumTiles, INT32 Pixels, INT32 xOffset, INT32 yOffset, INT32 xTextOffset, INT32 yTextOffset, INT32 VisYOffset);
void TC0480SCPSetColourBase(INT32 Base); // color base must be shifted back 4.  f.ex 0x1000 -> 0x100
void TC0480SCPExit();
void TC0480SCPScan(INT32 nAction);

// TC0510NIO
extern UINT8 TC0510NIOInputPort0[8];
extern UINT8 TC0510NIOInputPort1[8];
extern UINT8 TC0510NIOInputPort2[8];
extern UINT8 TC0510NIODip[2];
extern UINT8 TC0510NIOInput[3];

UINT16 TC0510NIOHalfWordRead(INT32 Offset);
UINT16 TC0510NIOHalfWordSwapRead(INT32 Offset);
void TC0510NIOHalfWordWrite(INT32 Offset, UINT16 Data);
void TC0510NIOHalfWordSwapWrite(INT32 Offset, UINT16 Data);
void TC0510NIOReset();
void TC0510NIOInit();
void TC0510NIOExit();
void TC0510NIOScan(INT32 nAction);

// TC0640FIO
extern UINT8 TC0640FIOInputPort0[8];
extern UINT8 TC0640FIOInputPort1[8];
extern UINT8 TC0640FIOInputPort2[8];
extern UINT8 TC0640FIOInputPort3[8];
extern UINT8 TC0640FIOInputPort4[8];
extern UINT8 TC0640FIOInput[5];

UINT8 TC0640FIORead(UINT32 Offset);
void TC0640FIOWrite(UINT32 Offset, UINT8 Data);
UINT16 TC0640FIOHalfWordRead(UINT32 Offset);
void TC0640FIOHalfWordWrite(UINT32 Offset, UINT16 Data);
UINT16 TC0640FIOHalfWordByteswapRead(UINT32 Offset);
void TC0640FIOHalfWordByteswapWrite(UINT32 Offset, UINT16 Data);
void TC0640FIOReset();
void TC0640FIOInit();
void TC0640FIOExit();
void TC0640FIOScan(INT32 nAction);

#define TC0100SCN0CtrlWordWrite_Map(base_address)						\
	if (a >= base_address && a <= base_address + 0x0f) {				\
		TC0100SCNCtrlWordWrite(0, (a - base_address) >> 1, d);			\
		return;															\
	}
	
#define TC0100SCN1CtrlWordWrite_Map(base_address)						\
	if (a >= base_address && a <= base_address + 0x0f) {				\
		TC0100SCNCtrlWordWrite(1, (a - base_address) >> 1, d);			\
		return;															\
	}
	
#define TC0100SCN2CtrlWordWrite_Map(base_address)						\
	if (a >= base_address && a <= base_address + 0x0f) {				\
		TC0100SCNCtrlWordWrite(2, (a - base_address) >> 1, d);			\
		return;															\
	}
	
#define TC0100SCN_CHECK_BG_LAYER_NEED_UPDATE_BYTE(chip_num)				\
	if (TC0100SCNDblWidth[chip_num]) {									\
		if (Offset >= 0x0000 && Offset < 0x8000) {						\
			TC0100SCNBgLayerUpdate[chip_num] = 1;						\
		}																\
	} else {															\
		if (Offset >= 0x0000 && Offset < 0x4000) {						\
			TC0100SCNBgLayerUpdate[chip_num] = 1;						\
		}																\
	}
	
#define TC0100SCN_CHECK_BG_LAYER_NEED_UPDATE_WORD(chip_num)				\
	if (TC0100SCNDblWidth[chip_num]) {									\
		if (Offset >= 0x0000/2 && Offset < 0x8000/2) {					\
			TC0100SCNBgLayerUpdate[chip_num] = 1;						\
		}																\
	} else {															\
		if (Offset >= 0x0000/2 && Offset < 0x4000/2) {					\
			TC0100SCNBgLayerUpdate[chip_num] = 1;						\
		}																\
	}
	
#define TC0100SCN_CHECK_FG_LAYER_NEED_UPDATE_BYTE(chip_num)				\
	if (TC0100SCNDblWidth[chip_num]) {									\
		if (Offset >= 0x8000 && Offset < 0x10000) {						\
			TC0100SCNFgLayerUpdate[chip_num] = 1;						\
		}																\
	} else {															\
		if (Offset >= 0x0000 && Offset < 0x8000) {						\
			TC0100SCNFgLayerUpdate[chip_num] = 1;						\
		}																\
	}
	
#define TC0100SCN_CHECK_FG_LAYER_NEED_UPDATE_WORD(chip_num)				\
	if (TC0100SCNDblWidth[chip_num]) {									\
		if (Offset >= 0x8000/2 && Offset < 0x10000/2) {					\
			TC0100SCNFgLayerUpdate[chip_num] = 1;						\
		}																\
	} else {															\
		if (Offset >= 0x8000/2 && Offset < 0xc000/2) {					\
			TC0100SCNFgLayerUpdate[chip_num] = 1;						\
		}																\
	}
	
#define TC0100SCN_CHECK_CHAR_LAYER_NEED_UPDATE_BYTE(chip_num)			\
	if (TC0100SCNDblWidth[chip_num]) {									\
		if (Offset >= 0x12000 && Offset < 0x14000) {					\
			TC0100SCNCharLayerUpdate[chip_num] = 1;						\
		}																\
		if (Offset >= 0x11000 && Offset < 0x12000) {					\
			TC0100SCNCharRamUpdate[chip_num] = 1;						\
		}																\
	} else {															\
		if (Offset >= 0x4000 && Offset < 0x6000) {						\
			TC0100SCNCharLayerUpdate[chip_num] = 1;						\
		}																\
		if (Offset >= 0x6000 && Offset < 0x7000) {						\
			TC0100SCNCharRamUpdate[chip_num] = 1;						\
		}																\
	}
	
#define TC0100SCN_CHECK_CHAR_LAYER_NEED_UPDATE_WORD(chip_num)			\
	if (TC0100SCNDblWidth[chip_num]) {									\
		if (Offset >= 0x12000/2 && Offset < 0x14000/2) {				\
			TC0100SCNCharLayerUpdate[chip_num] = 1;						\
		}																\
		if (Offset >= 0x11000/2 && Offset < 0x12000/2) {				\
			TC0100SCNCharRamUpdate[chip_num] = 1;						\
		}																\
	} else {															\
		if (Offset >= 0x4000/2 && Offset < 0x6000/2) {					\
			TC0100SCNCharLayerUpdate[chip_num] = 1;						\
		}																\
		if (Offset >= 0x6000/2 && Offset < 0x7000/2) {					\
			TC0100SCNCharRamUpdate[chip_num] = 1;						\
		}																\
	}

#define TC0100SCN0ByteWrite_Map(start, end)								\
	if (a >= start && a <= end) {										\
		INT32 Offset = (a - start) ^ 1;									\
		if (TC0100SCNRam[0][Offset] != d) {								\
			TC0100SCN_CHECK_BG_LAYER_NEED_UPDATE_BYTE(0)				\
			TC0100SCN_CHECK_FG_LAYER_NEED_UPDATE_BYTE(0)				\
			TC0100SCN_CHECK_CHAR_LAYER_NEED_UPDATE_BYTE(0)				\
		}																\
		TC0100SCNRam[0][Offset] = d;									\
		return;															\
	}

#define TC0100SCN0WordWrite_Map(start, end)								\
	if (a >= start && a <= end) {										\
		UINT16 *Ram = (UINT16*)TC0100SCNRam[0];							\
		INT32 Offset = (a - start) >> 1;								\
		if (Ram[Offset] != BURN_ENDIAN_SWAP_INT16(d)) {					\
			TC0100SCN_CHECK_BG_LAYER_NEED_UPDATE_WORD(0)				\
			TC0100SCN_CHECK_FG_LAYER_NEED_UPDATE_WORD(0)				\
			TC0100SCN_CHECK_CHAR_LAYER_NEED_UPDATE_WORD(0)				\
		}																\
		Ram[Offset] = BURN_ENDIAN_SWAP_INT16(d);						\
		return;															\
	}
	
#define TC0100SCN0LongWrite_Map(start, end)								\
	if (a >= start && a <= end) {										\
		UINT16 *Ram = (UINT16*)TC0100SCNRam[0];							\
		INT32 Offset = (a - start) >> 1;								\
		if (Ram[Offset] != BURN_ENDIAN_SWAP_INT16(d>>16)) {				\
			TC0100SCN_CHECK_BG_LAYER_NEED_UPDATE_WORD(0)				\
			TC0100SCN_CHECK_FG_LAYER_NEED_UPDATE_WORD(0)				\
			TC0100SCN_CHECK_CHAR_LAYER_NEED_UPDATE_WORD(0)				\
		}																\
		Ram[Offset] = BURN_ENDIAN_SWAP_INT16(d>>16);					\
        Offset |= 0x01;                                                 \
        if (Ram[Offset] != BURN_ENDIAN_SWAP_INT16((d&0xffff))) {		    \
			TC0100SCN_CHECK_BG_LAYER_NEED_UPDATE_WORD(0)				\
			TC0100SCN_CHECK_FG_LAYER_NEED_UPDATE_WORD(0)				\
			TC0100SCN_CHECK_CHAR_LAYER_NEED_UPDATE_WORD(0)				\
		}																\
		Ram[Offset] = BURN_ENDIAN_SWAP_INT16((d&0xffff));				    \
		return;															\
	}
	
#define TC0100SCN1ByteWrite_Map(start, end)								\
	if (a >= start && a <= end) {										\
		INT32 Offset = (a - start) ^ 1;									\
		if (TC0100SCNRam[1][Offset] != d) {								\
			TC0100SCN_CHECK_BG_LAYER_NEED_UPDATE_BYTE(1)				\
			TC0100SCN_CHECK_FG_LAYER_NEED_UPDATE_BYTE(1)				\
			TC0100SCN_CHECK_CHAR_LAYER_NEED_UPDATE_BYTE(1)				\
		}																\
		TC0100SCNRam[1][Offset] = d;									\
		return;															\
	}

#define TC0100SCN1WordWrite_Map(start, end)								\
	if (a >= start && a <= end) {										\
		UINT16 *Ram = (UINT16*)TC0100SCNRam[1];							\
		INT32 Offset = (a - start) >> 1;								\
		if (Ram[Offset] != BURN_ENDIAN_SWAP_INT16(d)) {					\
			TC0100SCN_CHECK_BG_LAYER_NEED_UPDATE_WORD(1)				\
			TC0100SCN_CHECK_FG_LAYER_NEED_UPDATE_WORD(1)				\
			TC0100SCN_CHECK_CHAR_LAYER_NEED_UPDATE_WORD(1)				\
		}																\
		Ram[Offset] = BURN_ENDIAN_SWAP_INT16(d);						\
		return;															\
	}
	
#define TC0100SCN2ByteWrite_Map(start, end)								\
	if (a >= start && a <= end) {										\
		INT32 Offset = (a - start) ^ 1;									\
		if (TC0100SCNRam[2][Offset] != d) {								\
			TC0100SCN_CHECK_BG_LAYER_NEED_UPDATE_BYTE(2)				\
			TC0100SCN_CHECK_FG_LAYER_NEED_UPDATE_BYTE(2)				\
			TC0100SCN_CHECK_CHAR_LAYER_NEED_UPDATE_BYTE(2)				\
		}																\
		TC0100SCNRam[2][Offset] = d;									\
		return;															\
	}

#define TC0100SCN2WordWrite_Map(start, end)								\
	if (a >= start && a <= end) {										\
		UINT16 *Ram = (UINT16*)TC0100SCNRam[2];							\
		INT32 Offset = (a - start) >> 1;								\
		if (Ram[Offset] != BURN_ENDIAN_SWAP_INT16(d)) {					\
			TC0100SCN_CHECK_BG_LAYER_NEED_UPDATE_WORD(2)				\
			TC0100SCN_CHECK_FG_LAYER_NEED_UPDATE_WORD(2)				\
			TC0100SCN_CHECK_CHAR_LAYER_NEED_UPDATE_WORD(2)				\
		}																\
		Ram[Offset] = BURN_ENDIAN_SWAP_INT16(d);						\
		return;															\
	}
	
#define TC0100SCNDualScreenByteWrite_Map(start, end)					\
	if (a >= start && a <= end) {										\
		INT32 Offset = (a - start) ^ 1;									\
		if (TC0100SCNRam[0][Offset] != d) {								\
			TC0100SCN_CHECK_BG_LAYER_NEED_UPDATE_BYTE(0)				\
			TC0100SCN_CHECK_FG_LAYER_NEED_UPDATE_BYTE(0)				\
			TC0100SCN_CHECK_CHAR_LAYER_NEED_UPDATE_BYTE(0)				\
		}																\
		if (TC0100SCNRam[1][Offset] != d) {								\
			TC0100SCN_CHECK_BG_LAYER_NEED_UPDATE_BYTE(1)				\
			TC0100SCN_CHECK_FG_LAYER_NEED_UPDATE_BYTE(1)				\
			TC0100SCN_CHECK_CHAR_LAYER_NEED_UPDATE_BYTE(1)				\
		}																\
		TC0100SCNRam[0][Offset] = BURN_ENDIAN_SWAP_INT16(d);			\
		TC0100SCNRam[1][Offset] = BURN_ENDIAN_SWAP_INT16(d);			\
		return;															\
	}
	
#define TC0100SCNDualScreenWordWrite_Map(start, end)					\
	if (a >= start && a <= end) {										\
		UINT16 *Ram0 = (UINT16*)TC0100SCNRam[0];						\
		UINT16 *Ram1 = (UINT16*)TC0100SCNRam[1];						\
		INT32 Offset = (a - start) >> 1;								\
		if (Ram0[Offset] != BURN_ENDIAN_SWAP_INT16(d)) {				\
			TC0100SCN_CHECK_BG_LAYER_NEED_UPDATE_WORD(0)				\
			TC0100SCN_CHECK_FG_LAYER_NEED_UPDATE_WORD(0)				\
			TC0100SCN_CHECK_CHAR_LAYER_NEED_UPDATE_WORD(0)				\
		}																\
		if (Ram1[Offset] != BURN_ENDIAN_SWAP_INT16(d)) {				\
			TC0100SCN_CHECK_BG_LAYER_NEED_UPDATE_WORD(1)				\
			TC0100SCN_CHECK_FG_LAYER_NEED_UPDATE_WORD(1)				\
			TC0100SCN_CHECK_CHAR_LAYER_NEED_UPDATE_WORD(1)				\
		}																\
		Ram0[Offset] = BURN_ENDIAN_SWAP_INT16(d);						\
		Ram1[Offset] = BURN_ENDIAN_SWAP_INT16(d);						\
		return;															\
	}

#define TC0100SCNTripleScreenByteWrite_Map(start, end)					\
	if (a >= start && a <= end) {										\
		INT32 Offset = (a - start) ^ 1;									\
		if (TC0100SCNRam[0][Offset] != d) {								\
			TC0100SCN_CHECK_BG_LAYER_NEED_UPDATE_BYTE(0)				\
			TC0100SCN_CHECK_FG_LAYER_NEED_UPDATE_BYTE(0)				\
			TC0100SCN_CHECK_CHAR_LAYER_NEED_UPDATE_BYTE(0)				\
		}																\
		if (TC0100SCNRam[1][Offset] != d) {								\
			TC0100SCN_CHECK_BG_LAYER_NEED_UPDATE_BYTE(1)				\
			TC0100SCN_CHECK_FG_LAYER_NEED_UPDATE_BYTE(1)				\
			TC0100SCN_CHECK_CHAR_LAYER_NEED_UPDATE_BYTE(1)				\
		}																\
		if (TC0100SCNRam[2][Offset] != d) {								\
			TC0100SCN_CHECK_BG_LAYER_NEED_UPDATE_BYTE(2)				\
			TC0100SCN_CHECK_FG_LAYER_NEED_UPDATE_BYTE(2)				\
			TC0100SCN_CHECK_CHAR_LAYER_NEED_UPDATE_BYTE(2)				\
		}																\
		TC0100SCNRam[0][Offset] = d;									\
		TC0100SCNRam[1][Offset] = d;									\
		TC0100SCNRam[2][Offset] = d;									\
		return;															\
	}
	
#define TC0100SCNTripleScreenWordWrite_Map(start, end)					\
	if (a >= start && a <= end) {										\
		UINT16 *Ram0 = (UINT16*)TC0100SCNRam[0];						\
		UINT16 *Ram1 = (UINT16*)TC0100SCNRam[1];						\
		UINT16 *Ram2 = (UINT16*)TC0100SCNRam[2];						\
		INT32 Offset = (a - start) >> 1;								\
		if (Ram0[Offset] != BURN_ENDIAN_SWAP_INT16(d)) {				\
			TC0100SCN_CHECK_BG_LAYER_NEED_UPDATE_WORD(0)				\
			TC0100SCN_CHECK_FG_LAYER_NEED_UPDATE_WORD(0)				\
			TC0100SCN_CHECK_CHAR_LAYER_NEED_UPDATE_WORD(0)				\
		}																\
		if (Ram1[Offset] != BURN_ENDIAN_SWAP_INT16(d)) {				\
			TC0100SCN_CHECK_BG_LAYER_NEED_UPDATE_WORD(1)				\
			TC0100SCN_CHECK_FG_LAYER_NEED_UPDATE_WORD(1)				\
			TC0100SCN_CHECK_CHAR_LAYER_NEED_UPDATE_WORD(1)				\
		}																\
		if (Ram2[Offset] != BURN_ENDIAN_SWAP_INT16(d)) {				\
			TC0100SCN_CHECK_BG_LAYER_NEED_UPDATE_WORD(2)				\
			TC0100SCN_CHECK_FG_LAYER_NEED_UPDATE_WORD(2)				\
			TC0100SCN_CHECK_CHAR_LAYER_NEED_UPDATE_WORD(2)				\
		}																\
		Ram0[Offset] = BURN_ENDIAN_SWAP_INT16(d);						\
		Ram1[Offset] = BURN_ENDIAN_SWAP_INT16(d);						\
		Ram2[Offset] = BURN_ENDIAN_SWAP_INT16(d);						\
		return;															\
	}

#define TC0220IOCHalfWordRead_Map(base_address)				\
	if (a >= base_address && a <= base_address + 0x0f) {		\
		return TC0220IOCHalfWordRead((a - base_address) >> 1);	\
	}
	
#define TC0220IOCHalfWordWrite_Map(base_address)			\
	if (a >= base_address && a <= base_address + 0x0f) {		\
		TC0220IOCHalfWordWrite((a - base_address) >> 1, d);	\
		return;							\
	}

#define TC0280GRDCtrlWordWrite_Map(base_address)			\
	if (a >= base_address && a <= base_address + 0x0f) {		\
		TC0280GRDCtrlWordWrite((a - base_address) >> 1, d);	\
		return;							\
	}

#define TC0360PRIHalfWordWrite_Map(base_address)			\
	if (a >= base_address && a <= base_address + 0x1f) {		\
		TC0360PRIHalfWordWrite((a - base_address) >> 1, d);	\
		return;							\
	}
	
#define TC0360PRIHalfWordSwapWrite_Map(base_address)			\
	if (a >= base_address && a <= base_address + 0x1f) {		\
		TC0360PRIHalfWordSwapWrite((a - base_address) >> 1, d);	\
		return;							\
	}

#define TC0430GRWCtrlWordWrite_Map(base_address)			\
	if (a >= base_address && a <= base_address + 0x0f) {		\
		TC0430GRWCtrlWordWrite((a - base_address) >> 1, d);	\
		return;							\
	}

#define TC0480SCPCtrlWordWrite_Map(base_address)			\
	if (a >= base_address && a <= base_address + 0x2f) {		\
		TC0480SCPCtrlWordWrite((a - base_address) >> 1, d);	\
		return;							\
	}

#define TC0510NIOHalfWordRead_Map(base_address)				\
	if (a >= base_address && a <= base_address + 0x0f) {		\
		return TC0510NIOHalfWordRead((a - base_address) >> 1);	\
	}

#define TC0510NIOHalfWordSwapRead_Map(base_address)				\
	if (a >= base_address && a <= base_address + 0x0f) {		\
		return TC0510NIOHalfWordSwapRead((a - base_address) >> 1);	\
	}

#define TC0510NIOHalfWordWrite_Map(base_address)			\
	if (a >= base_address && a <= base_address + 0x0f) {		\
		TC0510NIOHalfWordWrite((a - base_address) >> 1, d);	\
		return;							\
	}
	
#define TC0510NIOHalfWordSwapWrite_Map(base_address)			\
	if (a >= base_address && a <= base_address + 0x0f) {		\
		TC0510NIOHalfWordSwapWrite((a - base_address) >> 1, d);	\
		return;							\
	}

#define TC0180VCUHalfWordWrite_Map(base_address)					\
	if (a >= (base_address + 0x40000) && a <= (base_address+0x7ffff)) {		\
		TC0180VCUFbRAM[(a & 0x3ffff)^1] = d;					\
		TC0180VCUFramebufferWrite(a);						\
		return;									\
	}										\
											\
	if (a >= (base_address + 0x18000) && a <= (base_address+0x1801f)) {		\
		TC0180VCUWriteRegs(a, d);						\
		return;									\
	}

#define TC0180VCUWordWrite_Map(base_address)						\
	if (a >= (base_address + 0x40000) && a <= (base_address+0x7ffff)) {		\
		*((UINT16*)(TC0180VCUFbRAM + (a & 0x3fffe))) = d;		\
		TC0180VCUFramebufferWrite(a);						\
		return;									\
	}										\
											\
	if (a >= (base_address + 0x18000) && a <= (base_address+0x1801f)) {		\
		TC0180VCUWriteRegs(a, d >> 8);						\
		return;									\
	}

#define TC0180VCUHalfWordRead_Map(base_address)						\
	if (a >= (base_address + 0x40000) && a <= (base_address+0x7ffff)) {		\
		if (a & 1) return TC0180VCUFramebufferRead(a) >> 8;			\
		return TC0180VCUFramebufferRead(a);					\
	}										\
											\
	if (a >= (base_address + 0x18000) && a <= (base_address+0x1801f)) {		\
		return TC0180VCUReadRegs(a);						\
	}

// TC0180VCU doesn't seem to use word access at all
