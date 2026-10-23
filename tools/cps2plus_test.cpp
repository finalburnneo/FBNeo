#include "../src/burn/drv/capcom/cps.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <vector>
#include <string>
#include "../src/dep/libs/zlib/zlib.h"

extern BurnDriver BurnDrvCpsVsav2s05;

static void Check(bool bCondition, const char* szMessage)
{
	if (!bCondition) {
		fprintf(stderr, "FAIL: %s\n", szMessage);
		exit(1);
	}
}

static UINT32 HighCol(INT32 r, INT32 g, INT32 b, INT32)
{
	return (r << 16) | (g << 8) | b;
}

static std::vector<BurnRomInfo> Roms;
static INT32 nSampleBits = 25;
static UINT32 nSoundBytes = 0x80000;
static INT32 nFailedRom = -1;
static INT32 nRomLoadErrors;
static const char* szRomDirectory = NULL;

static INT32 SyntheticRomInfo(BurnRomInfo* pInfo, UINT32 i)
{
	if (i >= Roms.size()) return 1;
	if (pInfo) *pInfo = Roms[i];
	return 0;
}

static INT32 SyntheticInit()
{
	nRomLoadErrors = 0;
	return Cps2PlusInit(nSampleBits);
}

static void PutWord(UINT8* pMemory, UINT16 nWord)
{
	*(UINT16*)pMemory = BURN_ENDIAN_SWAP_INT16(nWord);
}

static INT32 ReadRom(FILE* pFile, UINT8* pDest, const BurnRomInfo& Info)
{
	UINT32 nRead = fread(pDest, 1, Info.nLen, pFile);
	if (nRead != Info.nLen || fgetc(pFile) != EOF || ferror(pFile)) return 1;
	return crc32(0, pDest, Info.nLen) != Info.nCrc;
}

static void TestRomFiles()
{
	const UINT8 Expected[] = { 'C', 'P', 'S', '2' };
	BurnRomInfo Info = { "fixture", 4, (UINT32)crc32(0, Expected, 4), 0 };
	for (INT32 i = 0; i < 4; i++) {
		UINT8 Data[] = { 'C', 'P', 'S', '2', 0 };
		if (i == 3) Data[0] ^= 1;
		UINT32 nLength = i == 1 ? 5 : i == 2 ? 3 : 4;
		FILE* pFile = tmpfile();
		Check(pFile != NULL, "temporary ROM fixture");
		Check(fwrite(Data, 1, nLength, pFile) == nLength, "write ROM fixture");
		rewind(pFile);
		UINT8 Loaded[4];
		Check((ReadRom(pFile, Loaded, Info) == 0) == (i == 0), "ROM reader rejects trailing, truncated and corrupt data");
		fclose(pFile);
	}
}

static INT32 LoadRom(UINT8* pDest, INT32* pWrote, INT32 i)
{
	BurnRomInfo Info;
	BurnDrvGetRomInfo(&Info, i);
	if (i == nFailedRom) { nRomLoadErrors++; return 1; }
	if (szRomDirectory) {
		char* szName;
		BurnDrvGetRomName(&szName, i, 0);
		std::string Path = std::string(szRomDirectory) + "/" + szName;
		FILE* pFile = fopen(Path.c_str(), "rb");
		if (!pFile) { fprintf(stderr, "Missing %s\n", Path.c_str()); nRomLoadErrors++; return 1; }
		INT32 nRet = ReadRom(pFile, pDest, Info);
		fclose(pFile);
		if (nRet) { nRomLoadErrors++; return 1; }
	} else {
		memset(pDest, 0, Info.nLen);
		if ((Info.nType & 15) == CPS2_ENCRYPTION_KEY) memset(pDest, 0xff, Info.nLen);
		if ((Info.nType & 15) == CPS2_PRG_68K && Info.nLen == 0x800000) {
			PutWord(pDest, 0x00ff); PutWord(pDest + 2, 0x8000);
			PutWord(pDest + 4, 0x00a0); PutWord(pDest + 6, 0x0000);
			// move.w #$1234,$ff0000; bra.s *
			UINT16 Program[] = { 0x33fc, 0x1234, 0x00ff, 0x0000, 0x60fe };
			for (UINT32 j = 0; j < sizeof(Program) / sizeof(Program[0]); j++) PutWord(pDest + 0x400000 + j * 2, Program[j]);
		}
		if ((Info.nType & 15) == CPS2_PRG_Z80) {
			memset(pDest, 0xf3, 0x8000);
			for (UINT32 j = 0; j < (Info.nLen - 0x8000) / 0x4000; j++) memset(pDest + 0x8000 + j * 0x4000, j, 0x4000);
		}
	}
	if (pWrote) *pWrote = Info.nLen;
	return 0;
}

static void AddRom(UINT32 nLen, UINT32 nType)
{
	BurnRomInfo Info = { "fixture", nLen, 0, nType };
	Roms.push_back(Info);
}

static void Fixtures()
{
	Roms.clear();
	AddRom(0x800000, CPS2_PRG_68K | BRF_PRG | BRF_ESS);
	for (INT32 i = 0; i < 4; i++) AddRom(0x1000000, CPS2_GFX | BRF_GRA);
	AddRom(nSoundBytes, CPS2_PRG_Z80 | BRF_PRG | BRF_ESS);
	AddRom(0x2000000, CPS2_QSND | BRF_SND);
	AddRom(0x14, CPS2_ENCRYPTION_KEY);
}

static UINT32 SampleAudio(UINT16 nBank, bool bAdpcm)
{
	std::vector<INT16> Audio(nBurnSoundLen * 2);
	pBurnSoundOut = &Audio[0];
	QscReset();
	for (INT32 i = 0; i < 4; i++) { QscNewFrame(); QscUpdate(nBurnSoundLen); }
	QscNewFrame();
	if (bAdpcm) {
		QscWrite(0xca, 0); QscWrite(0xcb, 0x7fff); QscWrite(0xcc, nBank);
		QscWrite(0xcd, 0x3fff); QscWrite(0x90, 0x120); QscWrite(0xd6, 1);
	} else {
		QscWrite(0x78, nBank); QscWrite(0x01, 0); QscWrite(0x02, 0x1000);
		QscWrite(0x04, 0x100); QscWrite(0x05, 0x100); QscWrite(0x06, 0x3fff); QscWrite(0x80, 0x120);
	}
	for (INT32 i = 0; i < 4; i++) { QscNewFrame(); QscUpdate(nBurnSoundLen); }
	UINT32 nHash = 2166136261U;
	bool bNonzero = false;
	for (size_t i = 0; i < Audio.size(); i++) {
		nHash = (nHash ^ (UINT16)Audio[i]) * 16777619U;
		if (Audio[i]) bNonzero = true;
	}
	if (!bNonzero) fprintf(stderr, "silent bank %04x adpcm %d bits %d status %02x length %d\n", nBank, bAdpcm, nSampleBits, QscRead(), nBurnSoundLen);
	Check(bNonzero, "sample playback is audible");
	pBurnSoundOut = NULL;
	return nHash;
}

static void TestSamples()
{
	for (INT32 i = 0; i < 4; i++) memset(CpsQSam + i * 0x800000, 0x11 * (i + 4), 0x10000);
	for (INT32 nAdpcm = 0; nAdpcm < 2; nAdpcm++) {
		UINT32 nLow = SampleAudio(0x8000, nAdpcm != 0);
		UINT32 nBit23 = SampleAudio(0x8080, nAdpcm != 0);
		UINT32 nBit24 = SampleAudio(0x8100, nAdpcm != 0);
		Check((nBit23 == nLow) == (nSampleBits == 23), "sample decoder bit 23");
		Check((nBit24 == nLow) == (nSampleBits < 25), "sample decoder bit 24");
	}
}

static void TestSound();
static void TestPalette();

static void TestStockTransition()
{
	Roms.clear();
	AddRom(0x400000, CPS2_PRG_68K | BRF_PRG | BRF_ESS);
	for (INT32 i = 0; i < 4; i++) AddRom(0x800000, CPS2_GFX | BRF_GRA);
	AddRom(0x40000, CPS2_PRG_Z80 | BRF_PRG | BRF_ESS);
	AddRom(0x800000, CPS2_QSND | BRF_SND);
	AddRom(0x14, CPS2_ENCRYPTION_KEY);
	BurnDrvCpsVsav2s05.Init = Cps2Init;
	Check(BurnDrvInit() == 0, "stock board after enhanced/failing board");
	Check(!Cps2Plus && !Cps2PlusObjExt && !nCpsQSamMask, "stock geometry does not enable extensions");
	TestPalette();
	SekOpen(0);
	SekWriteWord(0x708000, 0x2345);
	Check(SekReadWord(0x70c000) == 0x2345, "stock object mirrors");
	SekClose();
	Check(BurnDrvExit() == 0, "stock board exit");
	INT32 AuxiliaryRoms[] = { 1, 5, 6 };
	for (UINT32 i = 0; i < sizeof(AuxiliaryRoms) / sizeof(AuxiliaryRoms[0]); i++) {
		nFailedRom = AuxiliaryRoms[i]; nRomLoadErrors = 0;
		Check(BurnDrvInit() == 0, "stock ROM loader retains its auxiliary error policy");
		Check(nRomLoadErrors == 1, "test loader observes a failed stock ROM");
		BurnDrvExit();
	}
	nFailedRom = -1;
	BurnDrvCpsVsav2s05.Init = SyntheticInit;
	Fixtures();
	Check(BurnDrvInit() == 0, "enhanced board after stock");
	TestSound();
	Check(BurnDrvExit() == 0, "enhanced transition exit");
}

static void TestMap()
{
	SekOpen(0);
	Check(SekReadWord(0xa00000) == 0x33fc && SekFetchWord(0xa00000) == 0x33fc, "extension read and fetch");
	Check(SekReadByte(0xa00000) == 0x33 && SekReadByte(0xa00001) == 0xfc, "extension byte order");
	SekWriteWord(0xa00000, 0);
	Check(SekReadWord(0xa00000) == 0x33fc, "extension is read-only");
	SekReset();
	SekRun(100);
	Check(SekReadWord(0xff0000) == 0x1234, "execute code from extension");
	SekWriteWord(0x660000, 0xabcd);
	Check(SekReadWord(0x660000) == 0xabcd, "original extra RAM is not ROM");
	for (INT32 nBank = 0; nBank < 2; nBank++) {
		CpsMapObjectBanks(nBank);
		for (INT32 nWindow = 0; nWindow < 8; nWindow++) {
			UINT32 a = 0x700000 + nWindow * 0x2000 + 0x18;
			UINT32 nPhysicalBank = nBank ^ (nWindow >> 2);
			SekWriteWord(a, 0x3456);
			Check(SekReadWord(a ^ 0x2000) == 0x3456, "A13 mirror");
			Check(Cps2PlusObjExt[nPhysicalBank * 0x400 + 3] == ((nWindow >> 1) & 1), "physical bank and A14 selector");
			SekWriteByte(a + 1, 0x78);
			Check(SekReadWord(a) == 0x3478, "low byte write");
			SekWriteByte(a, 0x12);
			Check(SekReadWord(a) == 0x1278, "high byte write");
			SekWriteWord(a ^ 0x4000, 0x5678);
			Check(Cps2PlusObjExt[nPhysicalBank * 0x400 + 3] == (((nWindow >> 1) & 1) ^ 1), "last entry write chooses graphics half");
		}
	}
	CpsMapObjectBanks(0);
	SekClose();
}

static void TestSound()
{
	ZetOpen(0);
	INT32 Banks[] = { 0, 15, 16, 29, 30, 31 };
	for (UINT32 i = 0; i < sizeof(Banks) / sizeof(Banks[0]); i++) {
		INT32 nBank = Banks[i];
		ZetWriteByte(0xd003, nBank);
		INT32 nSelected = nSoundBytes == 0x80000 ? (nBank < 30 ? nBank : 0) : nBank & 15;
		if (nSoundBytes == 0x40000 && nSelected >= 14) nSelected = -1;
		Check(ZetReadByte(0x8000) == (nSelected < 0 ? 0xf3 : nSelected), "Z80 bank data");
		if (nSoundBytes == 0x80000) Check((ZetReadByte(0xd007) & 0x7f) == (0x60 | nSelected), "extended bank status");
	}
	QsndZReset();
	Check(ZetReadByte(0x8000) == 0, "Z80 bank reset");
	ZetClose();
}

static void SaveState();
static void RestoreState();

static UINT32 PaletteIndexCol(INT32 r, INT32 g, INT32 b, INT32)
{
	return ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4);
}

static UINT32 HighCol565(INT32 r, INT32 g, INT32 b, INT32)
{
	return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

static std::vector<UINT32> Pixels(384 * 224);

static void RenderObjects()
{
	memset(&Pixels[0], 0, Pixels.size() * sizeof(Pixels[0]));
	pBurnDraw = (UINT8*)&Pixels[0];
	CpsObjDrawInit();
	Cps2ObjDraw(0, 7);
}

static void TestGraphics()
{
	CtvReady();
	memset(CpsRam708, 0, 0x10000);
	memset(CpsGfx, 0xff, nCpsGfxLen);
	memset(CpsSaveFrg[0], 0, 0x10);
	BurnHighCol = PaletteIndexCol;
	std::vector<UINT8> Palette(0x1800);
	for (INT32 i = 0; i < 0xc00; i++) PutWord(&Palette[(i ^ 15) * 2], 0xf000 | (i + 1));
	UINT8 nOldCtrl = CpsReg[nCpsPalCtrlReg];
	CpsReg[nCpsPalCtrlReg] = 0x3f;
	CpsPalUpdate(&Palette[0]);
	CpsReg[nCpsPalCtrlReg] = nOldCtrl;
	nSpriteEnable = 0xff;
	memset(CpsGfx, 0x11, 128);
	memset(CpsGfx + 0x2000000, 0x22, 128);
	SekOpen(0);
	CpsMapObjectBanks(0);
	// Entry zero is blank. The first compacted entry must retain entry one's selector.
	UINT32 a = 0x70c008;
	SekWriteWord(a, 64); SekWriteWord(a + 2, 48);
	SekWriteWord(a + 4, 0); SekWriteWord(a + 6, 0);
	SekWriteWord(a + 10, 0x8000);
	CpsObjGet(); CpsObjGet();
	SekWriteWord(0x70800e, 0); // recycle the live slot after it was latched
	SekClose();
	RenderObjects();
	Check(Pixels[48 * 384 + 64] == 3, "latched high sprite survives compaction and live writes");
	SaveState();
	CpsObjReset();
	RestoreState();
	RenderObjects();
	Check(Pixels[48 * 384 + 64] == 3, "restore buffered sprite selector");

	for (INT32 nPhysical = 0; nPhysical < 2; nPhysical++) {
		for (INT32 nBank = 0; nBank < 8; nBank++) {
			UINT32 nTile = nBank * 0x10000 + 0x100;
			memset(CpsGfx + nTile * 128, (nBank + 1) * 0x11, 128);
			SekOpen(0); CpsMapObjectBanks(nPhysical ^ 1);
			UINT32 w = 0x708000 | ((nBank & 4) ? 0x4000 : 0);
			SekWriteWord(w, 80); SekWriteWord(w + 2, 48 | ((nBank & 3) << 13));
			SekWriteWord(w + 4, 0x100); SekWriteWord(w + 6, 0);
			SekWriteWord(w + 10, 0x8000);
			CpsObjGet(); CpsObjGet();
			SekClose(); RenderObjects();
			Check(Pixels[48 * 384 + 80] == (UINT32)(nBank + 2), "eight object graphics banks");
		}
	}
	SekOpen(0); CpsMapObjectBanks(0);
	memset(CpsRam708, 0, 0x10000); SekWriteWord(a + 10, 0x8000); SekClose();
	for (INT32 nFlip = 0; nFlip < 4; nFlip++) {
		memset(CpsGfx + 0x27ff800, 0x11, 16 * 128);
		memset(CpsGfx + 0x2000000, 0x22, 16 * 128);
		SekOpen(0);
		SekWriteWord(a, 64); SekWriteWord(a + 2, 48);
		SekWriteWord(a + 4, 0xfff0); SekWriteWord(a + 6, 0x1100 | (nFlip << 5));
		CpsObjGet(); CpsObjGet();
		SekClose();
		RenderObjects();
		INT32 nTop = (nFlip & 2) ? 3 : 2;
		Check(Pixels[48 * 384 + 64] == (UINT32)nTop, "sprite row overflow preserves bank wires and flip");
		Check(Pixels[64 * 384 + 64] == (UINT32)(5 - nTop), "sprite row wraps to 16 bits");
	}

	std::vector<UINT8> Tiles(0x4000);
	nStartline = 0; nEndline = 224;
	for (INT32 nLayer = 1; nLayer <= 3; nLayer++) {
		for (INT32 nEdge = 0; nEdge < 2; nEdge++) {
			UINT16 nCode = nEdge ? 0xffff : 0;
			INT32 nShift = nLayer == 1 ? 6 : nLayer == 2 ? 7 : 9;
			UINT32 nLow = (0x800000 + (UINT32(nCode) << nShift)) & 0x1ffffff;
			memset(CpsGfx + nLow, 0x11, 512);
			memset(CpsGfx + nLow + 0x2000000, 0x22, 512);
			for (INT32 nHigh = 0; nHigh < 2; nHigh++) {
				for (INT32 i = 0; i < 0x4000; i += 4) {
					PutWord(&Tiles[i], nCode); PutWord(&Tiles[i + 2], nHigh ? 0x200 : 0);
				}
				memset(&Pixels[0], 0, Pixels.size() * sizeof(Pixels[0]));
				if (nLayer == 1) Cps2Scr1Draw(&Tiles[0], 0, 0);
				if (nLayer == 3) Cps2Scr3Draw(&Tiles[0], 0, 0);
				if (nLayer == 2) {
					CpsrBase = &Tiles[0]; nCpsrScrX = nCpsrScrY = 0; CpsrRows = NULL;
					Cps2rPrepare(); Cps2rRender();
					std::vector<UINT16> Rows(0x400);
					for (INT32 i = 0; i < 0x400; i++) Rows[i] = BURN_ENDIAN_SWAP_INT16(i & 1);
					CpsrRows = &Rows[0]; Cps2rPrepare(); Cps2rRender(); CpsrRows = NULL;
				}
				Check(Pixels[32 * 384 + 32] == (UINT32)(nLayer * 0x200 + (nHigh ? 3 : 2)), "scroll selector and native address wrap");
			}
		}
	}
	pBurnDraw = NULL;
	BurnHighCol = HighCol;
}

static std::vector<UINT8> State;
static size_t nStateOffset;
static size_t nMachineSize;
static bool bRestore;

static INT32 Scan(BurnArea* pArea)
{
	if (!strcmp(pArea->szName, "Latched palette")) nMachineSize = nStateOffset;
	if (bRestore) {
		Check(nStateOffset + pArea->nLen <= State.size(), "state extent");
		memcpy(pArea->Data, &State[nStateOffset], pArea->nLen);
	} else {
		UINT8* pData = (UINT8*)pArea->Data;
		State.insert(State.end(), pData, pData + pArea->nLen);
	}
	nStateOffset += pArea->nLen;
	return 0;
}

static void SaveState()
{
	State.clear(); nStateOffset = 0; bRestore = false;
	BurnAcb = Scan;
	Check(CpsAreaScan(ACB_VOLATILE | ACB_READ, NULL) == 0, "save state");
}

static void RestoreState()
{
	nStateOffset = 0; bRestore = true;
	Check(CpsAreaScan(ACB_VOLATILE | ACB_WRITE, NULL) == 0, "restore state");
	Check(nStateOffset == State.size(), "state layout");
}

static void TestPalette()
{
	std::vector<UINT8> Palette(0x1800);
	UINT8 nOldCtrl = CpsReg[nCpsPalCtrlReg];
	for (INT32 nPage = 0; nPage < 6; nPage++) {
		for (INT32 i = 0; i < 0x200; i++) PutWord(&Palette[(nPage * 0x200 + i) * 2], 0xf000 | ((nPage + 1) << 8));
	}
	for (INT32 nCtrl = 0; nCtrl < 64; nCtrl++) {
		for (INT32 i = 0; i < 0xc00; i++) CpsPal[i] = 0x12345678;
		CpsReg[nCpsPalCtrlReg] = nCtrl;
		CpsPalUpdate(&Palette[0]);
		INT32 nFirst = 0;
		while (nFirst < 6 && !(nCtrl & (1 << nFirst))) nFirst++;
		for (INT32 nPage = 0; nPage < 6; nPage++) {
			UINT32 nExpected = (nCtrl & (1 << nPage)) ? HighCol((nPage - nFirst + 1) * 17, 0, 0, 0) : 0x12345678;
			for (INT32 i = 0; i < 0x200; i++) Check(CpsPal[nPage * 0x200 + i] == nExpected, "palette DMA routing and disabled pages");
		}
	}
	if (Cps2Plus) {
		CpsReg[nCpsPalCtrlReg] = 0;
		SaveState();
		nBurnBpp = 2; BurnHighCol = HighCol565;
		RestoreState();
		for (INT32 nPage = 0; nPage < 6; nPage++) Check(CpsPal[nPage * 0x200] == HighCol565((nPage + 1) * 17, 0, 0, 0), "palette state follows the frontend pixel format");
		nBurnBpp = 4; BurnHighCol = HighCol;
		RestoreState();
		for (INT32 nPage = 0; nPage < 6; nPage++) Check(CpsPal[nPage * 0x200] == HighCol((nPage + 1) * 17, 0, 0, 0), "palette state returns to RGB32");
	} else {
		INT32 nOldXmv = CpsXmvSixEnabled;
		CpsXmvSixEnabled = 1;
		State.clear(); nStateOffset = 0; bRestore = false; BurnAcb = Scan;
		CpsPalVisualScan(ACB_DRIVER_DATA | ACB_READ);
		Check(State.size() == 0xc00 * sizeof(UINT32) && !memcmp(&State[0], CpsPal, State.size()), "legacy palette state keeps its converted colours and layout");
		memset(CpsPal, 0, State.size());
		nStateOffset = 0; bRestore = true;
		CpsPalVisualScan(ACB_DRIVER_DATA | ACB_WRITE);
		Check(!memcmp(&State[0], CpsPal, State.size()), "legacy palette state restores its original bytes");
		CpsXmvSixEnabled = nOldXmv;
	}
	CpsReg[nCpsPalCtrlReg] = nOldCtrl;
}

static void TestState()
{
	SekOpen(0); CpsMapObjectBanks(1); SekWriteWord(0x704020, 0xaabb); SekClose();
	ZetOpen(0); ZetWriteByte(0xd003, 16); ZetClose();
	SaveState();
	SekOpen(0); CpsMapObjectBanks(0); SekWriteWord(0x700020, 0); SekClose();
	ZetOpen(0); ZetWriteByte(0xd003, 0); ZetClose();
	RestoreState();
	SekOpen(0);
	Check(SekReadWord(0x704020) == 0xaabb && Cps2PlusObjExt[0x404] == 1, "restore physical object banks");
	SekWriteWord(0x700020, 0xccdd);
	Check(Cps2PlusObjExt[0x404] == 0, "restore keeps alias handlers installed");
	SekClose();
	ZetOpen(0); Check(ZetReadByte(0x8000) == 16, "restore Z80 mapping"); ZetClose();
	Check(Cps2Frame() == 0, "advance restored machine");
	Check(CpsPalConsumeRestoredCache() == 0, "restored palette expires when emulation advances");
}

static UINT32 Hash(const void* pMemory, size_t nLength)
{
	const UINT8* pData = (const UINT8*)pMemory;
	UINT32 nHash = 2166136261U;
	for (size_t i = 0; i < nLength; i++) nHash = (nHash ^ pData[i]) * 16777619U;
	return nHash;
}

static void TestReleased(const char* szDirectory)
{
	szRomDirectory = szDirectory;
	nRomLoadErrors = 0;
	INT32 nRet = BurnDrvInit();
	if (nRet || nRomLoadErrors) BurnDrvExit();
	Check(nRet == 0 && nRomLoadErrors == 0, "released Arrange init and ROM sizes/CRCs");
	std::vector<INT16> Audio(nBurnSoundLen * 2);
	pBurnSoundOut = &Audio[0];
	pBurnDraw = (UINT8*)&Pixels[0];
	for (INT32 i = 0; i < 5400; i++) {
		for (UINT32 j = 0; ; j++) {
			BurnInputInfo Input;
			if (BurnDrvGetInputInfo(&Input, j)) break;
			if (Input.nType != BIT_DIGITAL || !Input.szInfo) continue;
			*Input.pVal = ((!strcmp(Input.szInfo, "p1 coin") && ((i >= 3050 && i <= 3070) || (i >= 3090 && i <= 3110))) ||
				(!strcmp(Input.szInfo, "p1 start") && i >= 3170 && i <= 3190) ||
				(!strcmp(Input.szInfo, "p1 fire 1") && ((i >= 3560 && i <= 3580) || (i >= 3860 && i <= 3880)))) ? 1 : 0;
		}
		Check(BurnDrvFrame() == 0, "released Arrange frame");
		if (i == 299 || i == 1799 || i == 3299 || i == 3599 || i == 4499 || i == 5399) {
			printf("Arrange frame %d: video %08x, audio %08x, RAM %08x\n", i + 1,
				Hash(&Pixels[0], Pixels.size() * 4), Hash(&Audio[0], Audio.size() * 2), Hash(CpsRamFF, 0x10000));
			char szCapture[64];
			snprintf(szCapture, sizeof(szCapture), "vsav2s05-%d.ppm", i + 1);
			FILE* pFile = fopen(szCapture, "wb");
			Check(pFile != NULL, "capture file");
			fprintf(pFile, "P6\n384 224\n255\n");
			for (size_t j = 0; j < Pixels.size(); j++) {
				UINT8 RGB[] = { (UINT8)(Pixels[j] >> 16), (UINT8)(Pixels[j] >> 8), (UINT8)Pixels[j] };
				fwrite(RGB, 1, 3, pFile);
			}
			fclose(pFile);
		}
	}
	SaveState();
	std::vector<UINT8> Initial = State;
	std::vector<UINT8> Final;
	UINT32 nVideo = 0, nAudio = 0, nMachine = 0, nNextVideo = 0, nNextAudio = 0;
	for (INT32 nPass = 0; nPass < 3; nPass++) {
		if (nPass) { State = Initial; RestoreState(); }
		pBurnDraw = nPass == 2 ? NULL : (UINT8*)&Pixels[0];
		for (INT32 i = 0; i < 30; i++) Check(BurnDrvFrame() == 0, "replayed Arrange frame");
		SaveState();
		if (nPass == 0) {
			Final = State; nMachine = Hash(&State[0], nMachineSize); nVideo = Hash(&Pixels[0], Pixels.size() * 4); nAudio = Hash(&Audio[0], Audio.size() * 2);
			Check(BurnDrvFrame() == 0, "reference visible frame");
			nNextVideo = Hash(&Pixels[0], Pixels.size() * 4);
			nNextAudio = Hash(&Audio[0], Audio.size() * 2);
		} else {
			Check(nMachine == Hash(&State[0], nMachineSize), "machine replay including hidden frames");
			Check(nAudio == Hash(&Audio[0], Audio.size() * 2), "audio replay including hidden frames");
			if (nPass == 1) {
				Check(nVideo == Hash(&Pixels[0], Pixels.size() * 4), "video replay");
				Check(State == Final, "machine state replay");
			} else {
				pBurnDraw = (UINT8*)&Pixels[0];
				Check(BurnDrvFrame() == 0, "visible frame after hidden replay");
				Check(nNextVideo == Hash(&Pixels[0], Pixels.size() * 4), "video after hidden replay");
				Check(nNextAudio == Hash(&Audio[0], Audio.size() * 2), "audio after hidden replay");
			}
		}
	}
	Check(BurnDrvExit() == 0, "released Arrange exit");
	pBurnDraw = NULL; pBurnSoundOut = NULL;
	printf("Released Arrange boot and replay tests passed\n");
}

static INT32 nDriverExits;

static INT32 CountDriverExit()
{
	nDriverExits++;
	return 0;
}

int main(int argc, char** argv)
{
	// EEPROMInit/Exit use frontend paths even in a headless process.
	struct stat Info;
	Check(stat("cps2plus_test_nvram", &Info) != 0 && errno == ENOENT, "EEPROM test path must be absent");
	TestRomFiles();
	BurnLibInit();
	for (nBurnDrvActive = 0; nBurnDrvActive < nBurnDrvCount; nBurnDrvActive++) {
		if (!strcmp(BurnDrvGetTextA(DRV_NAME), "vsav2s05")) break;
	}
	Check(nBurnDrvActive < nBurnDrvCount, "released Arrange driver is registered");
	BurnHighCol = HighCol;
	BurnExtLoadRom = LoadRom;
	nBurnSoundRate = 48000;
	nBurnSoundLen = 800;
	nBurnBpp = 4;
	nBurnPitch = 384 * 4;
	bBurnUseBlend = false;
	if (argc == 3 && !strcmp(argv[1], "--rom-dir")) {
		TestReleased(argv[2]);
		BurnLibExit();
		return 0;
	}
	Check(argc == 1, "usage: cps2plus_test [--rom-dir extracted-ROM-directory]");
	BurnDriver Original = BurnDrvCpsVsav2s05;
	// Substitute generated ROM metadata only in this test process.
	BurnDrvCpsVsav2s05.GetRomInfo = SyntheticRomInfo;
	BurnDrvCpsVsav2s05.Init = SyntheticInit;
	Fixtures();
	_tcscpy(szAppBlendPath, _T("./cps2plus_test-"));
	FILE* pBlend = fopen("cps2plus_test-vsav2s05.bld", "wb");
	Check(pBlend != NULL, "synthetic blend table");
	fputs("40000 0\n7ffff 1\n", pBlend); fclose(pBlend);
	bBurnUseBlend = true;
	Check(BurnDrvInit() == 0, "synthetic board init");
	remove("cps2plus_test-vsav2s05.bld");
	TestMap(); TestSound(); TestPalette(); TestState(); TestGraphics(); TestSamples();
	Check(BurnDrvExit() == 0, "synthetic board exit");
	Check(!Cps2Plus && !Cps2PlusObjExt && !nCpsQSamMask, "profile cleared on exit");
	for (nSampleBits = 23; nSampleBits <= 25; nSampleBits++) {
		for (nSoundBytes = 0x40000; nSoundBytes <= 0x80000; nSoundBytes *= 2) {
			Fixtures();
			Check(BurnDrvInit() == 0, "sample/sound board profile");
			TestSound(); TestSamples();
			CpsReset = 1; Check(BurnDrvFrame() == 0, "board reset"); CpsReset = 0;
			ZetOpen(0); Check(ZetReadByte(0x8000) == 0, "user reset bank"); ZetClose();
			Check(BurnDrvExit() == 0, "profile exit");
		}
	}
	nSampleBits = 24; nSoundBytes = 0x40000;
	Fixtures();
	for (UINT32 i = 0; i < Roms.size(); i++) {
		UINT32 nSize = Roms[i].nLen;
		Roms[i].nLen = nSize - 1;
		Check(BurnDrvInit() != 0, "invalid region geometry rejected");
		Check(!Cps2Plus && !nCpsQSamMask, "invalid geometry clears profile");
		BurnDrvExit();
		Roms[i].nLen = nSize;
	}
	// Region totals alone cannot validate the loader's four consecutive graphics lanes.
	BurnRomInfo Temp = Roms[2];
	Roms[2] = Roms[5]; Roms[5] = Temp;
	Check(BurnDrvInit() != 0, "interleaved graphics group rejected");
	BurnDrvExit();
	// The common loader accepts descending groups; an ascending pair would
	// reserve all groups at the largest size instead of the declared total.
	Roms.clear();
	AddRom(0x800000, CPS2_PRG_68K | BRF_PRG | BRF_ESS);
	for (INT32 i = 0; i < 4; i++) AddRom(0x400000, CPS2_GFX | BRF_GRA);
	for (INT32 i = 0; i < 4; i++) AddRom(0xc00000, CPS2_GFX | BRF_GRA);
	AddRom(nSoundBytes, CPS2_PRG_Z80 | BRF_PRG | BRF_ESS);
	AddRom(0x2000000, CPS2_QSND | BRF_SND); AddRom(0x14, CPS2_ENCRYPTION_KEY);
	Check(BurnDrvInit() != 0, "ascending graphics group sizes rejected");
	BurnDrvExit();
	for (INT32 i = 1; i <= 4; i++) Roms[i].nLen = 0xc00000;
	for (INT32 i = 5; i <= 8; i++) Roms[i].nLen = 0x400000;
	Check(BurnDrvInit() == 0 && nCpsGfxLen == 0x4000000, "descending graphics groups use the declared size");
	BurnDrvExit();
	for (INT32 i = 1; i <= 4; i++) Roms[i].nLen = 0x800000;
	for (INT32 i = 5; i <= 8; i++) Roms[i].nLen = 0x200000;
	Check(BurnDrvInit() == 0 && nCpsGfxLen == 0x4000000, "partially populated graphics retain the board address space");
	for (INT32 i = 0x2800000; i < 0x4000000; i++) Check(CpsGfx[i] == 0, "unpopulated graphics sockets are cleared");
	BurnDrvExit();
	Fixtures();
	TestStockTransition();
	for (nFailedRom = 0; nFailedRom < (INT32)Roms.size(); nFailedRom++) {
		nDriverExits = 0;
		CpsRunExitCallbackFunction = CountDriverExit;
		INT32 nType = Roms[nFailedRom].nType & 15;
		INT32 nRet = BurnDrvInit();
		Check((nRet != 0) == (nType == CPS2_PRG_68K || nType == CPS2_ENCRYPTION_KEY), "board uses the native CPS2 ROM error policy");
		Check(nRomLoadErrors == 1, "test loader observes every failed ROM");
		Check(Cps == 2 && nDriverExits == 0, "failed init waits for the frontend to exit the driver");
		Check(BurnDrvExit() == 0 && nDriverExits == 1, "frontend exits a failed driver once");
		Check(!Cps2Plus && !nCpsQSamMask, "driver exit clears a failed board profile");
	}
	nFailedRom = -1;
	Check(BurnDrvInit() == 0, "valid init after failures");
	Check(BurnDrvExit() == 0, "final profile exit");
	BurnDrvCpsVsav2s05 = Original;
	BurnLibExit();
	printf("CPS2+ synthetic platform tests passed\n");
	return 0;
}
