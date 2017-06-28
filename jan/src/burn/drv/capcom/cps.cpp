#include "cps.h"
// CPS (general)

INT32 Cps = 0;							// 1 = CPS1, 2 = CPS2, 3 = CPS Changer
INT32 Cps1Qs = 0;
INT32 Cps1DisablePSnd = 0;				// Disables the Z80 as well
INT32 Cps2DisableQSnd = 0;				// Disables the Z80 as well

INT32 nCPS68KClockspeed = 0;
INT32 nCpsCycles = 0;						// 68K Cycles per frame
INT32	nCpsZ80Cycles;

UINT8 *CpsGfx =NULL; UINT32 nCpsGfxLen =0; // All the graphics
UINT8 *CpsRom =NULL; UINT32 nCpsRomLen =0; // Program Rom (as in rom)
UINT8 *CpsCode=NULL; UINT32 nCpsCodeLen=0; // Program Rom (decrypted)
UINT8 *CpsZRom=NULL; UINT32 nCpsZRomLen=0; // Z80 Roms
INT8 *CpsQSam=NULL; UINT32 nCpsQSamLen=0;	// QSound Sample Roms
UINT8 *CpsAd  =NULL; UINT32 nCpsAdLen  =0; // ADPCM Data
UINT8 *CpsStar=NULL;
UINT8 *CpsKey=NULL; UINT32 nCpsKeyLen = 0;
UINT32 nCpsGfxScroll[4]={0,0,0,0}; // Offset to Scroll tiles
UINT32 nCpsGfxMask=0;	  // Address mask

// Separate out the bits of a byte
inline static UINT32 Separate(UINT32 b)
{
	UINT32 a = b;									// 00000000 00000000 00000000 11111111
	a  =((a & 0x000000F0) << 12) | (a & 0x0000000F);	// 00000000 00001111 00000000 00001111
	a = ((a & 0x000C000C) <<  6) | (a & 0x00030003);	// 00000011 00000011 00000011 00000011
	a = ((a & 0x02020202) <<  3) | (a & 0x01010101);	// 00010001 00010001 00010001 00010001

	return a;
}

// Precalculated table of the Separate function
static UINT32 SepTable[256];

static INT32 SepTableCalc()
{
	static INT32 bDone = 0;
	if (bDone) {
		return 0;										// Already done it
	}

	for (INT32 i = 0; i < 256; i++) {
		SepTable[i] = Separate(255 - i);
	}

	bDone = 1;											// done it
	return 0;
}

// Allocate space and load up a rom
static INT32 LoadUp(UINT8** pRom, INT32* pnRomLen, INT32 nNum)
{
	UINT8 *Rom;
	struct BurnRomInfo ri;

	ri.nLen = 0;
	BurnDrvGetRomInfo(&ri, nNum);	// Find out how big the rom is
	if (ri.nLen <= 0) {
		return 1;
	}

	// Load the rom
	Rom = (UINT8*)BurnMalloc(ri.nLen);
	if (Rom == NULL) {
		return 1;
	}

	if (BurnLoadRom(Rom,nNum,1)) {
		BurnFree(Rom);
		return 1;
	}

	// Success
	*pRom = Rom; *pnRomLen = ri.nLen;
	return 0;
}

static INT32 LoadUpSplit(UINT8** pRom, INT32* pnRomLen, INT32 nNum, INT32 nNumRomsGroup)
{
	UINT8 *Rom;
	struct BurnRomInfo ri;
	UINT32 nRomSize[8], nTotalRomSize = 0;
	INT32 i;

	ri.nLen = 0;
	for (i = 0; i < nNumRomsGroup; i++) {
		BurnDrvGetRomInfo(&ri, nNum + i);
		nRomSize[i] = ri.nLen;
	}
	
	for (i = 0; i < nNumRomsGroup; i++) {
		nTotalRomSize += nRomSize[i];
	}
	if (!nTotalRomSize) return 1;

	Rom = (UINT8*)BurnMalloc(nTotalRomSize);
	if (Rom == NULL) return 1;
	
	INT32 Offset = 0;
	for (i = 0; i < nNumRomsGroup; i++) {
		if (i > 0) Offset += nRomSize[i - 1];
		if (BurnLoadRom(Rom + Offset, nNum + i, 1)) {
			BurnFree(Rom);
			return 1;
		}
	}

	*pRom = Rom;
	*pnRomLen = nTotalRomSize;
	
	return 0;
}

// ----------------------------CPS1--------------------------------
// Load 1 rom and interleave in the CPS style:
// rom  : aa bb
// --ba --ba --ba --ba --ba --ba --ba --ba 8 pixels (four bytes)
//                                                  (skip four bytes)

static INT32 CpsLoadOne(UINT8* Tile, INT32 nNum, INT32 nWord, INT32 nShift)
{
	UINT8 *Rom = NULL; INT32 nRomLen=0;
	UINT8 *pt = NULL, *pr = NULL;
	INT32 i;

	LoadUp(&Rom, &nRomLen, nNum);
	if (Rom == NULL) {
		return 1;
	}

	nRomLen &= ~1;								// make sure even

	for (i = 0, pt = Tile, pr = Rom; i < nRomLen; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}

	BurnFree(Rom);
	return 0;
}

static INT32 CpsLoadOnePang(UINT8 *Tile,INT32 nNum,INT32 nWord,INT32 nShift)
{
	INT32 i=0;
	UINT8 *Rom = NULL; INT32 nRomLen = 0;
	UINT8 *pt = NULL, *pr = NULL;

	LoadUp(&Rom, &nRomLen, nNum);
	if (Rom == NULL) {
		return 1;
	}

	nRomLen &= ~1; // make sure even

	for (i = 0x100000, pt = Tile, pr = Rom + 0x100000; i < nRomLen; pt += 8) {
		UINT32 Pix; // Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}

	BurnFree(Rom);
	return 0;
}

static INT32 CpsLoadOneHack160(UINT8 *Tile, INT32 nNum, INT32 nWord, INT32 nType)
{
	INT32 i = 0, j = 0;
	UINT8 *Rom1 = NULL, *Rom2 = NULL;
	INT32 nRomLen1 = 0, nRomLen2 = 0;
	UINT8 *pt = NULL, *pr = NULL;

	LoadUp(&Rom1, &nRomLen1, nNum);
	if (Rom1 == NULL) {
		return 1;
	}
	LoadUp(&Rom2, &nRomLen2, nNum + 1);
	if (Rom2 == NULL) {
		return 1;
	}
	
	INT32 TileOffset[4] = { 0x000000, 0x000004, 0x200000, 0x200004 };
	if (nType == 1) {
		TileOffset[1] = 0x200000;
		TileOffset[2] = 0x000004;
	}

	for (j = 0; j < 4; j++) {
		for (i = 0, pt = Tile + TileOffset[j], pr = Rom1 + (0x80000 * j); i < 0x80000; pt += 8) {
			UINT32 Pix;		// Eight pixels
			UINT8 b;
			b = *pr++; i++; Pix = SepTable[b];
			if (nWord) {
				b = *pr++; i++; Pix |= SepTable[b] << 1;
			}

			Pix <<= 0;
			*((UINT32 *)pt) |= Pix;
		}

		for (i = 0, pt = Tile + TileOffset[j], pr = Rom2 + (0x80000 * j); i < 0x80000; pt += 8) {
			UINT32 Pix;		// Eight pixels
			UINT8 b;
			b = *pr++; i++; Pix = SepTable[b];
			if (nWord) {
				b = *pr++; i++; Pix |= SepTable[b] << 1;
			}

			Pix <<= 2;
			*((UINT32 *)pt) |= Pix;
		}
	}
	
	BurnFree(Rom2);
	BurnFree(Rom1);
	return 0;
}

static INT32 CpsLoadOneBootleg(UINT8* Tile, INT32 nNum, INT32 nWord, INT32 nShift)
{
	UINT8 *Rom = NULL; INT32 nRomLen=0;
	UINT8 *pt = NULL, *pr = NULL;
	INT32 i;

	LoadUp(&Rom, &nRomLen, nNum);
	if (Rom == NULL) {
		return 1;
	}
	nRomLen &= ~1;								// make sure even

	for (i = 0, pt = Tile, pr = Rom; i < 0x40000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}

	for (i = 0, pt = Tile + 4, pr = Rom + 0x40000; i < 0x40000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}

	BurnFree(Rom);
	return 0;
}

static INT32 CpsLoadOneBootlegSmallSingle(UINT8* Tile, INT32 nNum, INT32 nWord, INT32 nShift)
{
	UINT8 *Rom = NULL; INT32 nRomLen=0;
	UINT8 *pt = NULL, *pr = NULL;
	INT32 i;

	LoadUp(&Rom, &nRomLen, nNum);
	if (Rom == NULL) {
		return 1;
	}
	nRomLen &= ~1;								// make sure even

	for (i = 0, pt = Tile, pr = Rom; i < 0x40000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}

	BurnFree(Rom);
	return 0;
}

static INT32 CpsLoadOneBootlegType2(UINT8* Tile, INT32 nNum, INT32 nWord, INT32 nShift)
{
	UINT8 *Rom = NULL; INT32 nRomLen=0;
	UINT8 *pt = NULL, *pr = NULL;
	INT32 i;

	LoadUp(&Rom, &nRomLen, nNum);
	if (Rom == NULL) {
		return 1;
	}
	nRomLen &= ~1;								// make sure even

	for (i = 0, pt = Tile, pr = Rom; i < 0x40000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}

	for (i = 0, pt = Tile + 4, pr = Rom + 0x40000; i < 0x40000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}
	
	for (i = 0, pt = Tile + 0x200000, pr = Rom + 0x80000; i < 0x40000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}

	for (i = 0, pt = Tile + 0x200004, pr = Rom + 0xc0000; i < 0x40000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}

	BurnFree(Rom);
	return 0;
}

static INT32 CpsLoadOneBootlegType2Small(UINT8* Tile, INT32 nNum, INT32 nWord, INT32 nShift)
{
	UINT8 *Rom = NULL; INT32 nRomLen=0;
	UINT8 *pt = NULL, *pr = NULL;
	INT32 i;

	LoadUp(&Rom, &nRomLen, nNum);
	if (Rom == NULL) {
		return 1;
	}
	nRomLen &= ~1;								// make sure even

	for (i = 0, pt = Tile, pr = Rom; i < 0x40000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}

	for (i = 0, pt = Tile + 4, pr = Rom + 0x40000; i < 0x40000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}

	BurnFree(Rom);
	return 0;
}

static INT32 CpsLoadOneBootlegType2SmallSingle(UINT8* Tile, INT32 nNum, INT32 nWord, INT32 nShift)
{
	UINT8 *Rom = NULL; INT32 nRomLen=0;
	UINT8 *pt = NULL, *pr = NULL;
	INT32 i;

	LoadUp(&Rom, &nRomLen, nNum);
	if (Rom == NULL) {
		return 1;
	}
	nRomLen &= ~1;								// make sure even

	for (i = 0, pt = Tile, pr = Rom; i < 0x40000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}

	BurnFree(Rom);
	return 0;
}

static INT32 CpsLoadOneBootlegType3(UINT8 *Tile, INT32 nNum, INT32 nWord, INT32 nShift)
{
	UINT8 *Rom = NULL; INT32 nRomLen=0;
	UINT8 *pt = NULL, *pr = NULL;
	INT32 i;

	LoadUp(&Rom, &nRomLen, nNum);
	if (Rom == NULL) {
		return 1;
	}
	nRomLen &= ~1;								// make sure even

	for (i = 0, pt = Tile, pr = Rom; i < nRomLen; pt += 4) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}

	BurnFree(Rom);
	return 0;
}

static INT32 CpsLoadOneBootlegType3Swap(UINT8 *Tile, INT32 nNum, INT32 nWord, INT32 nShift)
{
	UINT8 *Rom = NULL; int nRomLen=0;
	UINT8 *pt = NULL, *pr = NULL;
	INT32 i;

	LoadUp(&Rom, &nRomLen, nNum);
	if (Rom == NULL) {
		return 1;
	}
	nRomLen &= ~1;								// make sure even

	for (i = 0, pt = Tile, pr = Rom; i < nRomLen; pt += 4) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
		
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= (nShift + 1);
		*((UINT32 *)pt) |= Pix;
	}

	BurnFree(Rom);
	return 0;
}

static INT32 CpsLoadOneSf2ebbl(UINT8* Tile, INT32 nNum, INT32 nWord, INT32 nShift)
{
	UINT8 *Rom = NULL; INT32 nRomLen=0;
	UINT8 *pt = NULL, *pr = NULL;
	INT32 i;

	LoadUp(&Rom, &nRomLen, nNum);
	if (Rom == NULL) {
		return 1;
	}
	nRomLen &= ~1;								// make sure even

	for (i = 0, pt = Tile, pr = Rom; i < 0x10000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}

	for (i = 0, pt = Tile + 4, pr = Rom + 0x10000; i < 0x10000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}

	BurnFree(Rom);
	return 0;
}

static INT32 CpsLoadOneSf2b(UINT8* Tile, INT32 nNum, INT32 nWord, INT32 nShift)
{
	UINT8 *Rom = NULL; INT32 nRomLen=0;
	UINT8 *pt = NULL, *pr = NULL;
	INT32 i;

	LoadUp(&Rom, &nRomLen, nNum);
	if (Rom == NULL) {
		return 1;
	}
	nRomLen &= ~1;								// make sure even

	for (i = 0, pt = Tile + 4, pr = Rom; i < 0x10000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}

	for (i = 0, pt = Tile, pr = Rom + 0x10000; i < 0x10000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}

	BurnFree(Rom);
	return 0;
}

static INT32 CpsLoadOneSf2koryu(UINT8* Tile, INT32 nNum, INT32 nWord, INT32 nShift)
{
	UINT8 *Rom = NULL; INT32 nRomLen=0;
	UINT8 *pt = NULL, *pr = NULL;
	INT32 i;

	LoadUp(&Rom, &nRomLen, nNum);
	if (Rom == NULL) {
		return 1;
	}

	nRomLen &= ~1;								// make sure even
	
	for (i = 0, pt = Tile, pr = Rom; i < nRomLen >> 1; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}
	
	Tile += 4;
	
	for (i = 0, pt = Tile, pr = Rom + (nRomLen >> 1); i < nRomLen >> 1; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}

	BurnFree(Rom);
	return 0;
}

static INT32 CpsLoadOneSf2stt(UINT8* Tile, INT32 nNum, INT32 nWord, INT32 nShift)
{
	UINT8 *Rom = NULL; INT32 nRomLen=0;
	UINT8 *pt = NULL, *pr = NULL;
	INT32 i;

	LoadUp(&Rom, &nRomLen, nNum);
	if (Rom == NULL) {
		return 1;
	}

	nRomLen &= ~1;								// make sure even

	for (i = 0, pt = Tile, pr = Rom + (nRomLen >> 1); i < nRomLen >> 1; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}
	
	Tile += 4;
	
	for (i = 0, pt = Tile, pr = Rom; i < nRomLen >> 1; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}

	BurnFree(Rom);
	return 0;
}

static INT32 CpsLoadOneWonder3b(UINT8* Tile, INT32 nNum, INT32 nWord, INT32 nShift)
{
	UINT8 *Rom = NULL; INT32 nRomLen=0;
	UINT8 *pt = NULL, *pr = NULL;
	INT32 i;

	LoadUp(&Rom, &nRomLen, nNum);
	if (Rom == NULL) {
		return 1;
	}

	nRomLen &= ~1;								// make sure even

	for (i = 0, pt = Tile, pr = Rom; i < (nRomLen >> 1); pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}
	
	for (i = 0, pt = Tile + 0x200000, pr = Rom + 0x40000; i < (nRomLen >> 1); pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		if (nWord) {
			b = *pr++; i++; Pix |= SepTable[b] << 1;
		}

		Pix <<= nShift;
		*((UINT32 *)pt) |= Pix;
	}

	BurnFree(Rom);
	return 0;
}

static INT32 CpsLoadPunisherbTiles(UINT8* Tile, INT32 nNum)
{
	UINT8 *Rom = (UINT8*)BurnMalloc(0x400000 * sizeof(UINT8));
	UINT8 *pt = NULL, *pr = NULL;
	INT32 i, j;

	if (Rom == NULL) {
		return 1;
	}
	
	if (BurnLoadRom(Rom + 0x000000, nNum + 0, 2)) {
		BurnFree(Rom);
		return 1;
	}
	
	if (BurnLoadRom(Rom + 0x000001, nNum + 1, 2)) {
		BurnFree(Rom);
		return 1;
	}
	
	if (BurnLoadRom(Rom + 0x100000, nNum + 2, 2)) {
		BurnFree(Rom);
		return 1;
	}
	
	if (BurnLoadRom(Rom + 0x100001, nNum + 3, 2)) {
		BurnFree(Rom);
		return 1;
	}
	
	if (BurnLoadRom(Rom + 0x200000, nNum + 4, 2)) {
		BurnFree(Rom);
		return 1;
	}
	
	if (BurnLoadRom(Rom + 0x200001, nNum + 5, 2)) {
		BurnFree(Rom);
		return 1;
	}
	
	if (BurnLoadRom(Rom + 0x300000, nNum + 6, 2)) {
		BurnFree(Rom);
		return 1;
	}
	
	if (BurnLoadRom(Rom + 0x300001, nNum + 7, 2)) {
		BurnFree(Rom);
		return 1;
	}
	
	INT32 TileOffset[4] = { 0x000000, 0x200000, 0x000004, 0x200004 };
	
	for (j = 0; j < 4; j++) {
		for (i = 0, pt = Tile + TileOffset[j], pr = Rom + (0x80000 * j); i < 0x80000; pt += 8) {
			UINT32 Pix;		// Eight pixels
			UINT8 b;
			b = *pr++; i++; Pix = SepTable[b];
			b = *pr++; i++; Pix |= SepTable[b] << 1;

			Pix <<= 0;
			*((UINT32 *)pt) |= Pix;
		}

		for (i = 0, pt = Tile + TileOffset[j], pr = Rom + 0x200000 +  (0x80000 * j); i < 0x80000; pt += 8) {
			UINT32 Pix;		// Eight pixels
			UINT8 b;
			b = *pr++; i++; Pix = SepTable[b];
			b = *pr++; i++; Pix |= SepTable[b] << 1;

			Pix <<= 2;
			*((UINT32 *)pt) |= Pix;
		}
	}
	
	BurnFree(Rom);
	return 0;
}

static INT32 CpsLoadSf2ceuab3Tiles(UINT8* Tile, INT32 nNum)
{
	UINT8 *Rom = (UINT8*)BurnMalloc(0x200000 * sizeof(UINT8));
	UINT8 *Temp = (UINT8*)BurnMalloc(0x200000 * sizeof(UINT8));
	UINT8 *pt = NULL, *pr = NULL;
	INT32 i;

	if (Rom == NULL) {
		return 1;
	}
	
	if (BurnLoadRom(Temp + 0x000000, nNum + 0, 1)) {
		BurnFree(Rom);
		return 1;
	}
	
	if (BurnLoadRom(Temp + 0x100000, nNum + 1, 1)) {
		BurnFree(Rom);
		return 1;
	}
	
	for (i = 0; i < 0x100000; i += 2) {
		Rom[i + 0x000000] = Temp[i + 0x000000];
		Rom[i + 0x000001] = Temp[i + 0x100000];
		Rom[i + 0x100000] = Temp[i + 0x000001];
		Rom[i + 0x100001] = Temp[i + 0x100001];
	}
	
	BurnFree(Temp);
	
	for (i = 0, pt = Tile, pr = Rom; i < 0x80000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		b = *pr++; i++; Pix |= SepTable[b] << 1;

		Pix <<= 0;
		*((UINT32 *)pt) |= Pix;
	}
	
	for (i = 0, pt = Tile + 4, pr = Rom + 0x80000; i < 0x80000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		b = *pr++; i++; Pix |= SepTable[b] << 1;

		Pix <<= 0;
		*((UINT32 *)pt) |= Pix;
	}
	
	for (i = 0, pt = Tile, pr = Rom + 0x100000; i < 0x80000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		b = *pr++; i++; Pix |= SepTable[b] << 1;

		Pix <<= 2;
		*((UINT32 *)pt) |= Pix;
	}
	
	for (i = 0, pt = Tile + 4, pr = Rom + 0x180000; i < 0x80000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		b = *pr++; i++; Pix |= SepTable[b] << 1;

		Pix <<= 2;
		*((UINT32 *)pt) |= Pix;
	}

	BurnFree(Rom);
	return 0;
}

static INT32 CpsLoadSf2ceeablTiles(UINT8* Tile, INT32 nNum)
{
	UINT8 *Rom = (UINT8*)BurnMalloc(0x200000 * sizeof(UINT8));
	UINT8 *Temp = (UINT8*)BurnMalloc(0x200000 * sizeof(UINT8));
	UINT8 *pt = NULL, *pr = NULL;
	INT32 i;

	if (Rom == NULL) {
		return 1;
	}
	
	if (BurnLoadRom(Temp + 0x000000, nNum + 0, 2)) {
		BurnFree(Rom);
		return 1;
	}
	
	if (BurnLoadRom(Temp + 0x000001, nNum + 1, 2)) {
		BurnFree(Rom);
		return 1;
	}
	
	if (BurnLoadRom(Temp + 0x100000, nNum + 2, 2)) {
		BurnFree(Rom);
		return 1;
	}
	
	if (BurnLoadRom(Temp + 0x100001, nNum + 3, 2)) {
		BurnFree(Rom);
		return 1;
	}
	
	for (i = 0; i < 0x100000; i += 2) {
		Rom[i + 0x000000] = Temp[i + 0x000000];
		Rom[i + 0x000001] = Temp[i + 0x100000];
		Rom[i + 0x100000] = Temp[i + 0x000001];
		Rom[i + 0x100001] = Temp[i + 0x100001];
	}
	
	BurnFree(Temp);
	
	for (i = 0, pt = Tile, pr = Rom; i < 0x80000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		b = *pr++; i++; Pix |= SepTable[b] << 1;

		Pix <<= 0;
		*((UINT32 *)pt) |= Pix;
	}
	
	for (i = 0, pt = Tile + 4, pr = Rom + 0x80000; i < 0x80000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		b = *pr++; i++; Pix |= SepTable[b] << 1;

		Pix <<= 0;
		*((UINT32 *)pt) |= Pix;
	}
	
	for (i = 0, pt = Tile, pr = Rom + 0x100000; i < 0x80000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		b = *pr++; i++; Pix |= SepTable[b] << 1;

		Pix <<= 2;
		*((UINT32 *)pt) |= Pix;
	}
	
	for (i = 0, pt = Tile + 4, pr = Rom + 0x180000; i < 0x80000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		b = *pr++; i++; Pix |= SepTable[b] << 1;

		Pix <<= 2;
		*((UINT32 *)pt) |= Pix;
	}

	BurnFree(Rom);
	return 0;
}

static INT32 CpsLoadSf2ceuab7Tiles(UINT8* Tile, INT32 nNum)
{
	UINT8 *Rom = (UINT8*)BurnMalloc(0x200000 * sizeof(UINT8));
	UINT8 *Temp = (UINT8*)BurnMalloc(0x200000 * sizeof(UINT8));
	UINT8 *pt = NULL, *pr = NULL;
	INT32 i;

	if (Rom == NULL) {
		return 1;
	}
	
	if (BurnLoadRom(Temp + 0x000000, nNum + 0, 2)) {
		BurnFree(Rom);
		return 1;
	}
	
	if (BurnLoadRom(Temp + 0x080000, nNum + 1, 2)) {
		BurnFree(Rom);
		return 1;
	}
	
	if (BurnLoadRom(Temp + 0x000001, nNum + 2, 2)) {
		BurnFree(Rom);
		return 1;
	}
	
	if (BurnLoadRom(Temp + 0x080001, nNum + 3, 2)) {
		BurnFree(Rom);
		return 1;
	}
	
	if (BurnLoadRom(Temp + 0x100000, nNum + 4, 2)) {
		BurnFree(Rom);
		return 1;
	}
	
	if (BurnLoadRom(Temp + 0x180000, nNum + 5, 2)) {
		BurnFree(Rom);
		return 1;
	}
	
	if (BurnLoadRom(Temp + 0x100001, nNum + 6, 2)) {
		BurnFree(Rom);
		return 1;
	}
	
	if (BurnLoadRom(Temp + 0x180001, nNum + 7, 2)) {
		BurnFree(Rom);
		return 1;
	}
	
	for (i = 0; i < 0x100000; i += 2) {
		Rom[i + 0x000000] = Temp[i + 0x000000];
		Rom[i + 0x000001] = Temp[i + 0x100000];
		Rom[i + 0x100000] = Temp[i + 0x000001];
		Rom[i + 0x100001] = Temp[i + 0x100001];
	}
	
	BurnFree(Temp);
	
	for (i = 0, pt = Tile, pr = Rom; i < 0x80000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		b = *pr++; i++; Pix |= SepTable[b] << 1;

		Pix <<= 0;
		*((UINT32 *)pt) |= Pix;
	}
	
	for (i = 0, pt = Tile + 4, pr = Rom + 0x80000; i < 0x80000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		b = *pr++; i++; Pix |= SepTable[b] << 1;

		Pix <<= 0;
		*((UINT32 *)pt) |= Pix;
	}
	
	for (i = 0, pt = Tile, pr = Rom + 0x100000; i < 0x80000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		b = *pr++; i++; Pix |= SepTable[b] << 1;

		Pix <<= 2;
		*((UINT32 *)pt) |= Pix;
	}
	
	for (i = 0, pt = Tile + 4, pr = Rom + 0x180000; i < 0x80000; pt += 8) {
		UINT32 Pix;						// Eight pixels
		UINT8 b;
		b = *pr++; i++; Pix = SepTable[b];
		b = *pr++; i++; Pix |= SepTable[b] << 1;

		Pix <<= 2;
		*((UINT32 *)pt) |= Pix;
	}

	BurnFree(Rom);
	return 0;
}

INT32 CpsLoadTiles(UINT8* Tile, INT32 nStart)
{
	// left  side of 16x16 tiles
	CpsLoadOne(Tile,     nStart    , 1, 0);
	CpsLoadOne(Tile,     nStart + 1, 1, 2);
	// right side of 16x16 tiles
	CpsLoadOne(Tile + 4, nStart + 2, 1, 0);
	CpsLoadOne(Tile + 4, nStart + 3, 1, 2);
	return 0;
}

INT32 CpsLoadTilesByte(UINT8* Tile, INT32 nStart)
{
	CpsLoadOne(Tile,     nStart + 0, 0, 0);
	CpsLoadOne(Tile,     nStart + 1, 0, 1);
	CpsLoadOne(Tile,     nStart + 2, 0, 2);
	CpsLoadOne(Tile,     nStart + 3, 0, 3);
	CpsLoadOne(Tile + 4, nStart + 4, 0, 0);
	CpsLoadOne(Tile + 4, nStart + 5, 0, 1);
	CpsLoadOne(Tile + 4, nStart + 6, 0, 2);
	CpsLoadOne(Tile + 4, nStart + 7, 0, 3);
	return 0;
}

INT32 CpsLoadTilesForgottn(INT32 nStart)
{
	CpsLoadOne(CpsGfx + 0 + 0x000000, nStart +  0, 0, 0);
	CpsLoadOne(CpsGfx + 0 + 0x000000, nStart +  1, 0, 1);
	CpsLoadOne(CpsGfx + 0 + 0x000000, nStart +  2, 1, 2);
	CpsLoadOne(CpsGfx + 4 + 0x000000, nStart +  3, 0, 0);
	CpsLoadOne(CpsGfx + 4 + 0x000000, nStart +  4, 0, 1);
	CpsLoadOne(CpsGfx + 4 + 0x000000, nStart +  5, 0, 2);
	CpsLoadOne(CpsGfx + 4 + 0x000000, nStart +  6, 0, 3);
	CpsLoadOne(CpsGfx + 0 + 0x100000, nStart +  7, 0, 0);
	CpsLoadOne(CpsGfx + 0 + 0x100000, nStart +  8, 0, 1);
	CpsLoadOne(CpsGfx + 4 + 0x100000, nStart +  9, 0, 0);
	CpsLoadOne(CpsGfx + 4 + 0x100000, nStart + 10, 0, 1);
	CpsLoadOne(CpsGfx + 4 + 0x100000, nStart + 11, 0, 2);
	CpsLoadOne(CpsGfx + 4 + 0x100000, nStart + 12, 0, 3);
	CpsLoadOne(CpsGfx + 0 + 0x200000, nStart + 13, 1, 0);
	CpsLoadOne(CpsGfx + 0 + 0x200000, nStart + 14, 0, 2);
	CpsLoadOne(CpsGfx + 0 + 0x200000, nStart + 15, 0, 3);
	CpsLoadOne(CpsGfx + 4 + 0x200000, nStart + 16, 1, 0);
	CpsLoadOne(CpsGfx + 4 + 0x200000, nStart + 17, 0, 2);
	CpsLoadOne(CpsGfx + 4 + 0x200000, nStart + 18, 0, 3);
	CpsLoadOne(CpsGfx + 0 + 0x300000, nStart + 19, 0, 2);
	CpsLoadOne(CpsGfx + 0 + 0x300000, nStart + 20, 0, 3);
	CpsLoadOne(CpsGfx + 4 + 0x300000, nStart + 21, 0, 2);
	CpsLoadOne(CpsGfx + 4 + 0x300000, nStart + 22, 0, 3);
	
	return 0;
}

INT32 CpsLoadTilesForgottna(INT32 nStart)
{
	CpsLoadOne(CpsGfx + 0 + 0x000000, nStart +  0, 0, 0);
	CpsLoadOne(CpsGfx + 0 + 0x000000, nStart +  1, 0, 1);
	CpsLoadOne(CpsGfx + 0 + 0x000000, nStart +  2, 1, 2);
	CpsLoadOne(CpsGfx + 4 + 0x000000, nStart +  3, 1, 0);
	CpsLoadOne(CpsGfx + 4 + 0x000000, nStart +  4, 0, 2);
	CpsLoadOne(CpsGfx + 4 + 0x000000, nStart +  5, 0, 3);
	CpsLoadOne(CpsGfx + 0 + 0x100000, nStart +  6, 0, 0);
	CpsLoadOne(CpsGfx + 0 + 0x100000, nStart +  7, 0, 1);
	CpsLoadOne(CpsGfx + 4 + 0x100000, nStart +  8, 0, 2);
	CpsLoadOne(CpsGfx + 4 + 0x100000, nStart +  9, 0, 3);
	CpsLoadOne(CpsGfx + 0 + 0x200000, nStart + 10, 1, 0);
	CpsLoadOne(CpsGfx + 0 + 0x200000, nStart + 11, 0, 2);
	CpsLoadOne(CpsGfx + 0 + 0x200000, nStart + 12, 0, 3);
	CpsLoadOne(CpsGfx + 4 + 0x200000, nStart + 13, 1, 0);
	CpsLoadOne(CpsGfx + 4 + 0x200000, nStart + 14, 0, 2);
	CpsLoadOne(CpsGfx + 4 + 0x200000, nStart + 15, 0, 3);
	CpsLoadOne(CpsGfx + 0 + 0x300000, nStart + 16, 0, 2);
	CpsLoadOne(CpsGfx + 0 + 0x300000, nStart + 17, 0, 3);
	CpsLoadOne(CpsGfx + 4 + 0x300000, nStart + 18, 0, 2);
	CpsLoadOne(CpsGfx + 4 + 0x300000, nStart + 19, 0, 3);
	
	return 0;
}

INT32 CpsLoadTilesForgottnu(INT32 nStart)
{
	CpsLoadOne(CpsGfx + 0 + 0x000000, nStart +  0, 0, 0);
	CpsLoadOne(CpsGfx + 0 + 0x000000, nStart +  1, 0, 1);
	CpsLoadOne(CpsGfx + 0 + 0x000000, nStart +  2, 1, 2);
	CpsLoadOne(CpsGfx + 4 + 0x000000, nStart +  3, 1, 0);
	CpsLoadOne(CpsGfx + 4 + 0x000000, nStart +  4, 0, 2);
	CpsLoadOne(CpsGfx + 4 + 0x000000, nStart +  5, 0, 3);
	CpsLoadOne(CpsGfx + 0 + 0x100000, nStart +  6, 0, 0);
	CpsLoadOne(CpsGfx + 0 + 0x100000, nStart +  7, 0, 1);
	CpsLoadOne(CpsGfx + 4 + 0x100000, nStart +  8, 0, 2);
	CpsLoadOne(CpsGfx + 4 + 0x100000, nStart +  9, 0, 3);
	CpsLoadOne(CpsGfx + 0 + 0x200000, nStart + 10, 1, 0);
	CpsLoadOne(CpsGfx + 0 + 0x200000, nStart + 11, 0, 2);
	CpsLoadOne(CpsGfx + 0 + 0x200000, nStart + 12, 0, 3);
	CpsLoadOne(CpsGfx + 4 + 0x200000, nStart + 13, 1, 0);
	CpsLoadOne(CpsGfx + 4 + 0x200000, nStart + 14, 1, 2);
	CpsLoadOne(CpsGfx + 0 + 0x300000, nStart + 15, 0, 2);
	CpsLoadOne(CpsGfx + 0 + 0x300000, nStart + 16, 0, 3);
	
	return 0;
}

INT32 CpsLoadTilesPang(UINT8* Tile, INT32 nStart)
{
	CpsLoadOne(    Tile,     nStart,     1, 0);
	CpsLoadOne(    Tile,     nStart + 1, 1, 2);
	CpsLoadOnePang(Tile + 4, nStart,     1, 0);
	CpsLoadOnePang(Tile + 4, nStart + 1, 1, 2);

	return 0;
}

static INT32 CpsLoadTilesBootleg(UINT8 *Tile, INT32 nStart)
{
	CpsLoadOneBootleg(Tile, nStart + 0, 0, 0);
	CpsLoadOneBootleg(Tile, nStart + 1, 0, 1);
	CpsLoadOneBootleg(Tile, nStart + 2, 0, 2);
	CpsLoadOneBootleg(Tile, nStart + 3, 0, 3);
	
	return 0;
}

INT32 CpsLoadTilesSf2ebbl(UINT8 *Tile, INT32 nStart)
{
	CpsLoadOneSf2ebbl(Tile, nStart + 0, 0, 0);
	CpsLoadOneSf2ebbl(Tile, nStart + 1, 0, 2);
	CpsLoadOneSf2ebbl(Tile, nStart + 2, 0, 1);
	CpsLoadOneSf2ebbl(Tile, nStart + 3, 0, 3);
	
	return 0;
}

INT32 CpsLoadTilesSf2b(UINT8 *Tile, INT32 nStart)
{
	CpsLoadOneSf2b(Tile, nStart + 0, 0, 0);
	CpsLoadOneSf2b(Tile, nStart + 1, 0, 2);
	CpsLoadOneSf2b(Tile, nStart + 2, 0, 1);
	CpsLoadOneSf2b(Tile, nStart + 3, 0, 3);
	
	return 0;
}

INT32 CpsLoadTilesSf2koryuExtra(UINT8* Tile, INT32 nStart)
{
	CpsLoadOneSf2koryu(Tile, nStart + 0, 1, 0);
	CpsLoadOneSf2koryu(Tile, nStart + 1, 1, 2);
	
	return 0;
}

static INT32 CpsLoadTilesBootlegType3(UINT8 *Tile, INT32 nStart)
{
	CpsLoadOneBootlegType3(Tile, nStart +  0, 0, 0);
	CpsLoadOneBootlegType3(Tile, nStart +  1, 0, 1);
	CpsLoadOneBootlegType3(Tile, nStart +  2, 0, 2);
	CpsLoadOneBootlegType3(Tile, nStart +  3, 0, 3);
	
	return 0;
}

INT32 CpsLoadTilesHack160(INT32 nStart)
{
	CpsLoadOneHack160(CpsGfx, nStart, 1, 0);
	
	return 0;
}

INT32 CpsLoadTilesHack160Alt(INT32 nStart)
{
	CpsLoadOneHack160(CpsGfx, nStart, 1, 1);
	
	return 0;
}

INT32 CpsLoadTilesSf2koryu(INT32 nStart)
{
	CpsLoadOneSf2koryu(CpsGfx + 0x000000, nStart + 0, 1, 0);
	CpsLoadOneSf2koryu(CpsGfx + 0x000000, nStart + 1, 1, 2);
	CpsLoadOneSf2koryu(CpsGfx + 0x200000, nStart + 2, 1, 0);
	CpsLoadOneSf2koryu(CpsGfx + 0x200000, nStart + 3, 1, 2);
	CpsLoadOneSf2koryu(CpsGfx + 0x400000, nStart + 4, 1, 0);
	CpsLoadOneSf2koryu(CpsGfx + 0x400000, nStart + 5, 1, 2);

	return 0;
}

INT32 CpsLoadTilesSf2stt(INT32 nStart)
{
	CpsLoadOneSf2stt(CpsGfx + 0x000000, nStart + 0, 1, 0);
	CpsLoadOneSf2stt(CpsGfx + 0x000000, nStart + 1, 1, 2);
	CpsLoadOneSf2stt(CpsGfx + 0x200000, nStart + 2, 1, 0);
	CpsLoadOneSf2stt(CpsGfx + 0x200000, nStart + 3, 1, 2);
	CpsLoadOneSf2stt(CpsGfx + 0x400000, nStart + 4, 1, 0);
	CpsLoadOneSf2stt(CpsGfx + 0x400000, nStart + 5, 1, 2);

	return 0;
}

INT32 CpsLoadTilesSf2mdt(INT32 nStart)
{
	CpsLoadTilesBootlegType3(CpsGfx + 0x000000, nStart + 0);
	CpsLoadTilesBootlegType3(CpsGfx + 0x200000, nStart + 4);
	CpsLoadTilesBootlegType3(CpsGfx + 0x400000, nStart + 8);
	
	return 0;
}

INT32 CpsLoadTilesSf2mdta(INT32 nStart)
{
	CpsLoadOneBootlegType3Swap(CpsGfx + 0x000000, nStart +  0, 0, 0);
	CpsLoadOneBootlegType3Swap(CpsGfx + 0x000000, nStart +  1, 0, 2);
	CpsLoadOneBootlegType3Swap(CpsGfx + 0x200000, nStart +  2, 0, 0);
	CpsLoadOneBootlegType3Swap(CpsGfx + 0x200000, nStart +  3, 0, 2);
	CpsLoadOneBootlegType3Swap(CpsGfx + 0x400000, nStart +  4, 0, 0);
	CpsLoadOneBootlegType3Swap(CpsGfx + 0x400000, nStart +  5, 0, 2);
	
	return 0;
}

INT32 CpsLoadTilesSf2ceuab3(INT32 nStart)
{
	CpsLoadOneBootlegType2(CpsGfx + 0x000000, nStart + 0, 0, 0);
	CpsLoadOneBootlegType2(CpsGfx + 0x000000, nStart + 1, 0, 1);
	CpsLoadOneBootlegType2(CpsGfx + 0x000000, nStart + 2, 0, 2);
	CpsLoadOneBootlegType2(CpsGfx + 0x000000, nStart + 3, 0, 3);
	// The last two roms are a complete pain, handled by this custom function
	CpsLoadSf2ceuab3Tiles(CpsGfx + 0x400000, nStart + 4);
	
	return 0;
}

INT32 CpsLoadTilesSf2ceeabl(INT32 nStart)
{
	CpsLoadOneBootlegType2Small(CpsGfx + 0x000000, nStart + 0, 0, 0);
	CpsLoadOneBootlegType2Small(CpsGfx + 0x200000, nStart + 1, 0, 0);
	CpsLoadOneBootlegType2Small(CpsGfx + 0x000000, nStart + 2, 0, 1);
	CpsLoadOneBootlegType2Small(CpsGfx + 0x200000, nStart + 3, 0, 1);
	CpsLoadOneBootlegType2Small(CpsGfx + 0x000000, nStart + 4, 0, 2);
	CpsLoadOneBootlegType2Small(CpsGfx + 0x200000, nStart + 5, 0, 2);
	CpsLoadOneBootlegType2Small(CpsGfx + 0x000000, nStart + 6, 0, 3);
	CpsLoadOneBootlegType2Small(CpsGfx + 0x200000, nStart + 7, 0, 3);
	// The last four roms are a complete pain, handled by this custom function
	CpsLoadSf2ceeablTiles(CpsGfx + 0x400000, nStart + 8);
	
	return 0;
}

INT32 CpsLoadTilesSf2ceuab7(INT32 nStart)
{
	CpsLoadOneBootlegType2SmallSingle(CpsGfx + 0x000000, nStart +  0, 0, 0);
	CpsLoadOneBootlegType2SmallSingle(CpsGfx + 0x000004, nStart +  1, 0, 0);
	CpsLoadOneBootlegType2SmallSingle(CpsGfx + 0x200000, nStart +  2, 0, 0);
	CpsLoadOneBootlegType2SmallSingle(CpsGfx + 0x200004, nStart +  3, 0, 0);
	CpsLoadOneBootlegType2SmallSingle(CpsGfx + 0x000000, nStart +  4, 0, 1);
	CpsLoadOneBootlegType2SmallSingle(CpsGfx + 0x000004, nStart +  5, 0, 1);
	CpsLoadOneBootlegType2SmallSingle(CpsGfx + 0x200000, nStart +  6, 0, 1);
	CpsLoadOneBootlegType2SmallSingle(CpsGfx + 0x200004, nStart +  7, 0, 1);
	CpsLoadOneBootlegType2SmallSingle(CpsGfx + 0x000000, nStart +  8, 0, 2);
	CpsLoadOneBootlegType2SmallSingle(CpsGfx + 0x000004, nStart +  9, 0, 2);
	CpsLoadOneBootlegType2SmallSingle(CpsGfx + 0x200000, nStart + 10, 0, 2);
	CpsLoadOneBootlegType2SmallSingle(CpsGfx + 0x200004, nStart + 11, 0, 2);
	CpsLoadOneBootlegType2SmallSingle(CpsGfx + 0x000000, nStart + 12, 0, 3);
	CpsLoadOneBootlegType2SmallSingle(CpsGfx + 0x000004, nStart + 13, 0, 3);
	CpsLoadOneBootlegType2SmallSingle(CpsGfx + 0x200000, nStart + 14, 0, 3);
	CpsLoadOneBootlegType2SmallSingle(CpsGfx + 0x200004, nStart + 15, 0, 3);
	
	// The last eight roms are a complete pain, handled by this custom function
	CpsLoadSf2ceuab7Tiles(CpsGfx + 0x400000, nStart + 16);
	
	return 0;
}

INT32 CpsLoadTilesSf2ebbl3(INT32 nStart)
{
	CpsLoadOne(CpsGfx + 0x000000, nStart     , 1, 0);
	CpsLoadOne(CpsGfx + 0x000000, nStart +  1, 1, 2);
	CpsLoadOne(CpsGfx + 0x000004, nStart +  2, 0, 0);
	CpsLoadOne(CpsGfx + 0x000004, nStart +  3, 0, 1);
	CpsLoadOne(CpsGfx + 0x000004, nStart +  4, 0, 2);
	CpsLoadOne(CpsGfx + 0x000004, nStart +  5, 0, 3);	
	CpsLoadOne(CpsGfx + 0x200000, nStart +  6, 0, 0);
	CpsLoadOne(CpsGfx + 0x200000, nStart +  7, 0, 1);
	CpsLoadOne(CpsGfx + 0x200000, nStart +  8, 1, 2);
	CpsLoadOne(CpsGfx + 0x200004, nStart +  9, 1, 0);
	CpsLoadOne(CpsGfx + 0x200004, nStart + 10, 0, 2);
	CpsLoadOne(CpsGfx + 0x200004, nStart + 11, 0, 3);	
	CpsLoadOne(CpsGfx + 0x400000, nStart + 12, 1, 0);
	CpsLoadOne(CpsGfx + 0x400000, nStart + 13, 1, 2);
	CpsLoadOne(CpsGfx + 0x400004, nStart + 14, 1, 0);
	CpsLoadOne(CpsGfx + 0x400004, nStart + 15, 1, 2);
	
	return 0;
}

INT32 CpsLoadTilesFcrash(INT32 nStart)
{
	CpsLoadTilesBootlegType3(CpsGfx + 0x000000, nStart +  0);
	CpsLoadTilesBootlegType3(CpsGfx + 0x080000, nStart +  4);
	CpsLoadTilesBootlegType3(CpsGfx + 0x100000, nStart +  8);
	CpsLoadTilesBootlegType3(CpsGfx + 0x180000, nStart + 12);
	
	return 0;
}

INT32 CpsLoadTilesCawingbl(INT32 nStart)
{
	CpsLoadTilesBootlegType3(CpsGfx + 0x000000, nStart + 0);
	
	return 0;
}

INT32 CpsLoadTilesCaptcommb(INT32 nStart)
{
	CpsLoadOneBootlegType2(CpsGfx + 0x000000, nStart + 0, 0, 0);
	CpsLoadOneBootlegType2(CpsGfx + 0x000000, nStart + 1, 0, 1);
	CpsLoadOneBootlegType2(CpsGfx + 0x000000, nStart + 2, 0, 2);
	CpsLoadOneBootlegType2(CpsGfx + 0x000000, nStart + 3, 0, 3);
	
	return 0;
}

INT32 CpsLoadTilesDinopic(INT32 nStart)
{
	CpsLoadTilesBootleg(CpsGfx + 0x000000, nStart + 0);
	CpsLoadTilesBootleg(CpsGfx + 0x200000, nStart + 4);
	
	return 0;
}

INT32 CpsLoadTilesDinopic4(INT32 nStart)
{
	CpsLoadOneBootlegSmallSingle(CpsGfx + 0x000000, nStart +  0, 0, 0);
	CpsLoadOneBootlegSmallSingle(CpsGfx + 0x000004, nStart +  1, 0, 0);
	CpsLoadOneBootlegSmallSingle(CpsGfx + 0x000000, nStart +  2, 0, 1);
	CpsLoadOneBootlegSmallSingle(CpsGfx + 0x000004, nStart +  3, 0, 1);
	CpsLoadOneBootlegSmallSingle(CpsGfx + 0x000000, nStart +  4, 0, 2);
	CpsLoadOneBootlegSmallSingle(CpsGfx + 0x000004, nStart +  5, 0, 2);
	CpsLoadOneBootlegSmallSingle(CpsGfx + 0x000000, nStart +  6, 0, 3);
	CpsLoadOneBootlegSmallSingle(CpsGfx + 0x000004, nStart +  7, 0, 3);
	CpsLoadOneBootlegSmallSingle(CpsGfx + 0x200000, nStart +  8, 0, 0);
	CpsLoadOneBootlegSmallSingle(CpsGfx + 0x200004, nStart +  9, 0, 0);
	CpsLoadOneBootlegSmallSingle(CpsGfx + 0x200000, nStart + 10, 0, 1);
	CpsLoadOneBootlegSmallSingle(CpsGfx + 0x200004, nStart + 11, 0, 1);
	CpsLoadOneBootlegSmallSingle(CpsGfx + 0x200000, nStart + 12, 0, 2);
	CpsLoadOneBootlegSmallSingle(CpsGfx + 0x200004, nStart + 13, 0, 2);
	CpsLoadOneBootlegSmallSingle(CpsGfx + 0x200000, nStart + 14, 0, 3);
	CpsLoadOneBootlegSmallSingle(CpsGfx + 0x200004, nStart + 15, 0, 3);
	
	return 0;
}

INT32 CpsLoadTilesSlampic(INT32 nStart)
{
	CpsLoadTilesBootleg(CpsGfx + 0x000000, nStart + 0);
	CpsLoadTilesBootleg(CpsGfx + 0x200000, nStart + 4);
	CpsLoadTilesBootleg(CpsGfx + 0x400000, nStart + 8);
	
	return 0;
}

INT32 CpsLoadTilesKodb(INT32 nStart)
{
	CpsLoadTilesByte(CpsGfx, nStart);

	return 0;
}

INT32 CpsLoadTilesWonder3b(INT32 nStart)
{
	CpsLoadOneWonder3b(CpsGfx + 0x000000, nStart + 0, 0, 0);
	CpsLoadOneWonder3b(CpsGfx + 0x000000, nStart + 1, 0, 2);
	CpsLoadOneWonder3b(CpsGfx + 0x000004, nStart + 2, 0, 0);
	CpsLoadOneWonder3b(CpsGfx + 0x000004, nStart + 3, 0, 2);
	CpsLoadOneWonder3b(CpsGfx + 0x000000, nStart + 4, 0, 1);
	CpsLoadOneWonder3b(CpsGfx + 0x000000, nStart + 5, 0, 3);
	CpsLoadOneWonder3b(CpsGfx + 0x000004, nStart + 6, 0, 1);
	CpsLoadOneWonder3b(CpsGfx + 0x000004, nStart + 7, 0, 3);
	
	return 0;
}

INT32 CpsLoadTilesPang3r1a(INT32 nStart)
{
	CpsLoadOne(CpsGfx + 0x000000, nStart + 0, 1, 0);
	CpsLoadOne(CpsGfx + 0x200000, nStart + 1, 1, 0);
	CpsLoadOne(CpsGfx + 0x000004, nStart + 2, 1, 0);
	CpsLoadOne(CpsGfx + 0x200004, nStart + 3, 1, 0);
	CpsLoadOne(CpsGfx + 0x000000, nStart + 4, 1, 2);
	CpsLoadOne(CpsGfx + 0x200000, nStart + 5, 1, 2);
	CpsLoadOne(CpsGfx + 0x000004, nStart + 6, 1, 2);
	CpsLoadOne(CpsGfx + 0x200004, nStart + 7, 1, 2);
	
	return 0;
}

INT32 CpsLoadTilesPunisherb(INT32 nStart)
{
	CpsLoadPunisherbTiles(CpsGfx, nStart);
	
	return 0;
}

INT32 CpsLoadStars(UINT8* pStar, INT32 nStart)
{
	UINT8* pTemp[2] = { NULL, NULL};
	INT32 nLen;

	for (INT32 i = 0; i < 2; i++) {
		if (LoadUp(&pTemp[i], &nLen, nStart + (i << 1))) {
			BurnFree(pTemp[0]);
			BurnFree(pTemp[1]);
		}
	}

	for (INT32 i = 0; i < 0x1000; i++) {
		pStar[i] = pTemp[0][i << 1];
		pStar[0x01000 + i] = pTemp[1][i << 1];
	}

	BurnFree(pTemp[0]);
	BurnFree(pTemp[1]);
	
	return 0;
}

INT32 CpsLoadStarsByte(UINT8* pStar, INT32 nStart)
{
	UINT8* pTemp[2] = { NULL, NULL};
	INT32 nLen;

	for (INT32 i = 0; i < 2; i++) {
		if (LoadUp(&pTemp[i], &nLen, nStart + (i * 4))) {
			BurnFree(pTemp[0]);
			BurnFree(pTemp[1]);
		}
	}

	for (INT32 i = 0; i < 0x1000; i++) {
		pStar[i] = pTemp[0][i];
		pStar[0x01000 + i] = pTemp[1][i];
	}

	BurnFree(pTemp[0]);
	BurnFree(pTemp[1]);	

	return 0;
}

INT32 CpsLoadStarsForgottnAlt(UINT8* pStar, INT32 nStart)
{
	UINT8* pTemp[2] = { NULL, NULL};
	INT32 nLen;

	for (INT32 i = 0; i < 2; i++) {
		if (LoadUp(&pTemp[i], &nLen, nStart + (i * 3))) {
			BurnFree(pTemp[0]);
			BurnFree(pTemp[1]);
		}
	}

	for (INT32 i = 0; i < 0x1000; i++) {
		pStar[i] = pTemp[0][i << 1];
		pStar[0x01000 + i] = pTemp[1][i << 1];
	}

	BurnFree(pTemp[0]);
	BurnFree(pTemp[1]);
	
	return 0;
}

// ----------------------------CPS2--------------------------------
// Load 1 rom and interleve in the CPS2 style:
// rom  : aa bb -- -- (4 bytes)
// --ba --ba --ba --ba --ba --ba --ba --ba 8 pixels (four bytes)
//                                                  (skip four bytes)

// memory 000000-100000 are in even word fields of first 080000 section
// memory 100000-200000 are in  odd word fields of first 080000 section
// i = ABCD nnnn nnnn nnnn nnnn n000
// s = 00AB Cnnn nnnn nnnn nnnn nnD0

inline static void Cps2Load100000(UINT8* Tile, UINT8* Sect, INT32 nShift)
{
	UINT8 *pt, *pEnd, *ps;
	pt = Tile; pEnd = Tile + 0x100000; ps = Sect;

	do {
		UINT32 Pix;				// Eight pixels
		Pix  = SepTable[ps[0]];
		Pix |= SepTable[ps[1]] << 1;
		Pix <<= nShift;
		*((UINT32*)pt) |= Pix;

		pt += 8; ps += 4;
	}
	while (pt < pEnd);
}

static INT32 Cps2LoadOne(UINT8* Tile, INT32 nNum, INT32 nWord, INT32 nShift)
{
	UINT8 *Rom = NULL; INT32 nRomLen = 0;
	UINT8 *pt, *pr;

	LoadUp(&Rom, &nRomLen, nNum);
	if (Rom == NULL) {
		return 1;
	}

	if (nWord == 0) {
		UINT8*Rom2 = NULL; INT32 nRomLen2 = 0;
		UINT8*Rom3 = Rom;

		LoadUp(&Rom2, &nRomLen2, nNum + 1);
		if (Rom2 == NULL) {
			return 1;
		}

		nRomLen <<= 1;
		Rom = (UINT8*)BurnMalloc(nRomLen);
		if (Rom == NULL) {
			BurnFree(Rom2);
			BurnFree(Rom3);
			return 1;
		}

		for (INT32 i = 0; i < nRomLen2; i++) {
			Rom[(i << 1) + 0] = Rom3[i];
			Rom[(i << 1) + 1] = Rom2[i];
		}

		BurnFree(Rom2);
		BurnFree(Rom3);
	}

	// Go through each section
	pt = Tile; pr = Rom;
	for (INT32 b = 0; b < nRomLen >> 19; b++) {
		Cps2Load100000(pt, pr,     nShift); pt += 0x100000;
		Cps2Load100000(pt, pr + 2, nShift); pt += 0x100000;
		pr += 0x80000;
	}

	BurnFree(Rom);

	return 0;
}

static INT32 Cps2LoadSplit(UINT8* Tile, INT32 nNum, INT32 nShift, INT32 nNumRomsGroup)
{
	UINT8 *Rom = NULL; INT32 nRomLen = 0;
	UINT8 *pt, *pr;

	LoadUpSplit(&Rom, &nRomLen, nNum, nNumRomsGroup);
	if (Rom == NULL) {
		return 1;
	}
	
	// Go through each section
	pt = Tile; pr = Rom;
	for (INT32 b = 0; b < nRomLen >> 19; b++) {
		Cps2Load100000(pt, pr,     nShift); pt += 0x100000;
		Cps2Load100000(pt, pr + 2, nShift); pt += 0x100000;
		pr += 0x80000;
	}

	BurnFree(Rom);

	return 0;
}

INT32 Cps2LoadTiles(UINT8* Tile, INT32 nStart)
{
	// left  side of 16x16 tiles
	Cps2LoadOne(Tile,     nStart,     1, 0);
	Cps2LoadOne(Tile,     nStart + 1, 1, 2);
	// right side of 16x16 tiles
	Cps2LoadOne(Tile + 4, nStart + 2, 1, 0);
	Cps2LoadOne(Tile + 4, nStart + 3, 1, 2);

	return 0;
}

INT32 Cps2LoadTilesSplit4(UINT8* Tile, INT32 nStart)
{
	// left  side of 16x16 tiles
	Cps2LoadSplit(Tile,     nStart +  0, 0, 4);
	Cps2LoadSplit(Tile,     nStart +  4, 2, 4);
	// right side of 16x16 tiles
	Cps2LoadSplit(Tile + 4, nStart +  8, 0, 4);
	Cps2LoadSplit(Tile + 4, nStart + 12, 2, 4);

	return 0;
}

INT32 Cps2LoadTilesSplit8(UINT8* Tile, INT32 nStart)
{
	// left  side of 16x16 tiles
	Cps2LoadSplit(Tile,     nStart +  0, 0, 8);
	Cps2LoadSplit(Tile,     nStart +  8, 2, 8);
	// right side of 16x16 tiles
	Cps2LoadSplit(Tile + 4, nStart + 16, 0, 8);
	Cps2LoadSplit(Tile + 4, nStart + 24, 2, 8);

	return 0;
}

INT32 Cps2LoadTilesSIM(UINT8* Tile, INT32 nStart)
{
	Cps2LoadOne(Tile,     nStart,     0, 0);
	Cps2LoadOne(Tile,     nStart + 2, 0, 2);
	Cps2LoadOne(Tile + 4, nStart + 4, 0, 0);
	Cps2LoadOne(Tile + 4, nStart + 6, 0, 2);

	return 0;
}

INT32 Cps2LoadTiles19xxj(UINT8* Tile, INT32 nStart)
{
	// left
	Cps2LoadOne(Tile + 0x000000, nStart +  0, 1, 0);
	Cps2LoadOne(Tile + 0x000000, nStart +  1, 1, 2);
	
	// right
	Cps2LoadOne(Tile + 0x000004, nStart +  2, 1, 0);
	Cps2LoadOne(Tile + 0x000004, nStart +  3, 1, 2);
	
	// left
	Cps2LoadOne(Tile + 0x800000, nStart +  4, 1, 0);
	Cps2LoadOne(Tile + 0xa00000, nStart +  5, 1, 0);
	Cps2LoadOne(Tile + 0xc00000, nStart +  6, 1, 0);
	Cps2LoadOne(Tile + 0xe00000, nStart +  7, 1, 0);
	Cps2LoadOne(Tile + 0x800000, nStart +  8, 1, 2);
	Cps2LoadOne(Tile + 0xa00000, nStart +  9, 1, 2);
	Cps2LoadOne(Tile + 0xc00000, nStart + 10, 1, 2);
	Cps2LoadOne(Tile + 0xe00000, nStart + 11, 1, 2);
	
	// right
	Cps2LoadOne(Tile + 0x800004, nStart + 12, 1, 0);
	Cps2LoadOne(Tile + 0xa00004, nStart + 13, 1, 0);
	Cps2LoadOne(Tile + 0xc00004, nStart + 14, 1, 0);
	Cps2LoadOne(Tile + 0xe00004, nStart + 15, 1, 0);
	Cps2LoadOne(Tile + 0x800004, nStart + 16, 1, 2);
	Cps2LoadOne(Tile + 0xa00004, nStart + 17, 1, 2);
	Cps2LoadOne(Tile + 0xc00004, nStart + 18, 1, 2);
	Cps2LoadOne(Tile + 0xe00004, nStart + 19, 1, 2);
	
	return 0;
}

INT32 Cps2LoadTilesGigaman2(UINT8 *Tile, UINT8 *pSrc)
{
	UINT8 *pt = Tile;
	UINT8 *pr = pSrc;
	for (INT32 b = 0; b < 0x200000 >> 19; b++) {
		Cps2Load100000(pt, pr,     0); pt += 0x100000;
		Cps2Load100000(pt, pr + 2, 0); pt += 0x100000;
		pr += 0x80000;
	}
	
	pt = Tile;
	pr = pSrc + 0x200000;
	for (INT32 b = 0; b < 0x200000 >> 19; b++) {
		Cps2Load100000(pt, pr,     2); pt += 0x100000;
		Cps2Load100000(pt, pr + 2, 2); pt += 0x100000;
		pr += 0x80000;
	}
	
	pt = Tile + 4;
	pr = pSrc + 0x400000;
	for (INT32 b = 0; b < 0x200000 >> 19; b++) {
		Cps2Load100000(pt, pr,     0); pt += 0x100000;
		Cps2Load100000(pt, pr + 2, 0); pt += 0x100000;
		pr += 0x80000;
	}
	
	pt = Tile + 4;
	pr = pSrc + 0x600000;
	for (INT32 b = 0; b < 0x200000 >> 19; b++) {
		Cps2Load100000(pt, pr,     2); pt += 0x100000;
		Cps2Load100000(pt, pr + 2, 2); pt += 0x100000;
		pr += 0x80000;
	}

	return 0;
}

// ----------------------------------------------------------------

// The file extension indicates the data contained in a file.
// it consists of 2 numbers optionally followed by a single letter.
// The letter indicates the version. The meaning for the nubmers
// is as follows:
// 01 - 02 : Z80 program
// 03 - 10 : 68K program (filenames ending with x contain the XOR table)
// 11 - 12 : QSound sample data
// 13 - nn : Graphics data

static UINT32 nGfxMaxSize;

static INT32 CpsGetROMs(bool bLoad)
{
	struct BurnRomInfo ri;

	UINT8* CpsCodeLoad = CpsCode;
	UINT8* CpsRomLoad = CpsRom;
	UINT8* CpsGfxLoad = CpsGfx;
	UINT8* CpsZRomLoad = CpsZRom;
	UINT8* CpsQSamLoad = (UINT8*)CpsQSam;
	UINT8* CpsKeyLoad = CpsKey;

	INT32 nGfxNum = 0;

	if (bLoad) {
		if (!CpsCodeLoad || !CpsRomLoad || !CpsGfxLoad || !CpsZRomLoad || !CpsQSamLoad) {
			return 1;
		}
	} else {
		nCpsCodeLen = nCpsRomLen = nCpsGfxLen = nCpsZRomLen = nCpsQSamLen = 0;

		nGfxMaxSize = 0;
		if (BurnDrvGetHardwareCode() & HARDWARE_CAPCOM_CPS2_SIMM) {
			nGfxMaxSize = ~0U;
		}
	}

	INT32 i = 0;
	do {
		ri.nLen = 0;
		ri.nType = 0;
		BurnDrvGetRomInfo(&ri, i);
		
		if ((ri.nType & 0x0f) == CPS2_PRG_68K) {
			if (bLoad) {
				if (BurnLoadRom(CpsRomLoad, i, 1)) return 1;
				CpsRomLoad += ri.nLen;
			} else {
				nCpsRomLen += ri.nLen;
			}
			i++;
		}
		
		if ((ri.nType & 0x0f) == CPS2_PRG_68K_SIMM) {
			if (bLoad) {
				if (BurnLoadRom(CpsRomLoad + 0x000001, i + 0, 2)) return 1;
				if (BurnLoadRom(CpsRomLoad + 0x000000, i + 1, 2)) return 1;
				CpsRomLoad += ri.nLen * 2;
				i += 2;
			} else {
				nCpsRomLen += ri.nLen;
				i++;
			}
		}
		
		if ((ri.nType & 0x0f) == CPS2_PRG_68K_XOR_TABLE) {
			if (bLoad) {
				if (BurnLoadRom(CpsCodeLoad, i, 1)) return 1;
				CpsCodeLoad += ri.nLen;
			} else {
				nCpsCodeLen += ri.nLen;
			}
			i++;
		}
		
		if ((ri.nType & 0x0f) == CPS2_GFX) {
			if (bLoad) {
				Cps2LoadTiles(CpsGfxLoad, i);
				CpsGfxLoad += (nGfxMaxSize == ~0U ? ri.nLen : nGfxMaxSize) * 4;
				i += 4;
			} else {
				if (ri.nLen > nGfxMaxSize) {
					nGfxMaxSize = ri.nLen;
				}
				if (ri.nLen < nGfxMaxSize) {
					nGfxMaxSize = ~0U;
				}
				nCpsGfxLen += ri.nLen;
				nGfxNum++;
				i++;
			}
		}
		
		if ((ri.nType & 0x0f) == CPS2_GFX_SIMM) {
			if (bLoad) {
				Cps2LoadTilesSIM(CpsGfxLoad, i);
				CpsGfxLoad += ri.nLen * 8;
				i += 8;
			} else {
				nCpsGfxLen += ri.nLen;
				i++;
			}
		}
		
		if ((ri.nType & 0x0f) == CPS2_GFX_SPLIT4) {
			if (bLoad) {
				Cps2LoadTilesSplit4(CpsGfxLoad, i);
				CpsGfxLoad += (nGfxMaxSize == ~0U ? ri.nLen : nGfxMaxSize) * 16;
				i += 16;
			} else {
				if (ri.nLen > nGfxMaxSize) {
					nGfxMaxSize = ri.nLen;
				}
				if (ri.nLen < nGfxMaxSize) {
					nGfxMaxSize = ~0U;
				}
				nCpsGfxLen += ri.nLen;
				nGfxNum++;
				i++;
			}
		}
		
		if ((ri.nType & 0x0f) == CPS2_GFX_SPLIT8) {
			if (bLoad) {
				Cps2LoadTilesSplit8(CpsGfxLoad, i);
				CpsGfxLoad += (nGfxMaxSize == ~0U ? ri.nLen : nGfxMaxSize) * 32;
				i += 32;
			} else {
				if (ri.nLen > nGfxMaxSize) {
					nGfxMaxSize = ri.nLen;
				}
				if (ri.nLen < nGfxMaxSize) {
					nGfxMaxSize = ~0U;
				}
				nCpsGfxLen += ri.nLen;
				nGfxNum++;
				i++;
			}
		}
		
		if ((ri.nType & 0x0f) == CPS2_GFX_19XXJ) {
			if (bLoad) {
				Cps2LoadTiles19xxj(CpsGfxLoad, i);
				CpsGfxLoad += (nGfxMaxSize == ~0U ? ri.nLen : nGfxMaxSize) * 20;
				i += 20;
			} else {
				nGfxMaxSize = 0xcd000;
				nCpsGfxLen += ri.nLen;
				nGfxNum++;
				i++;
			}
		}
				
		if ((ri.nType & 0x0f) == CPS2_PRG_Z80) {
			if (bLoad) {
				BurnLoadRom(CpsZRomLoad, i, 1);
				CpsZRomLoad += ri.nLen;
			} else {
				nCpsZRomLen += ri.nLen;
			}
			i++;
		}
		
		if ((ri.nType & 0x0f) == CPS2_QSND) {
			if (bLoad) {
				BurnLoadRom(CpsQSamLoad, i, 1);
				BurnByteswap(CpsQSamLoad, ri.nLen);
				CpsQSamLoad += ri.nLen;
			} else {
				nCpsQSamLen += ri.nLen;
			}
			i++;
		}
		
		if ((ri.nType & 0x0f) == CPS2_QSND_SIMM) {
			if (bLoad) {
				BurnLoadRom(CpsQSamLoad, i, 1);
				BurnByteswap(CpsQSamLoad, ri.nLen);
				CpsQSamLoad += ri.nLen;
			} else {
				nCpsQSamLen += ri.nLen;
			}
			i++;
		}
		
		if ((ri.nType & 0x0f) == CPS2_QSND_SIMM_BYTESWAP) {
			if (bLoad) {
				BurnLoadRom(CpsQSamLoad + 1, i + 0, 2);
				BurnLoadRom(CpsQSamLoad + 0, i + 1, 2);
				i += 2;
			} else {
				nCpsQSamLen += ri.nLen;
				i++;
			}
		}
		
		if ((ri.nType & 0x0f) == CPS2_ENCRYPTION_KEY) {
			if (bLoad) {
				if (BurnLoadRom(CpsKeyLoad, i, 1)) return 1;
				CpsKeyLoad += ri.nLen;
			} else {
				nCpsKeyLen += ri.nLen;
			}
			i++;
		}
	} while (ri.nLen);

	if (bLoad) {
#if 0
		for (UINT32 i = 0; i < nCpsCodeLen / 4; i++) {
			((UINT32*)CpsCode)[i] ^= ((UINT32*)CpsRom)[i];
		}
#endif
		cps2_decrypt_game_data();
		
//		if (!nCpsCodeLen) return 1;
	} else {

		if (nGfxMaxSize != ~0U) {
			nCpsGfxLen = nGfxNum * nGfxMaxSize;
		}
		
#if 1 && defined FBA_DEBUG
		if (!nCpsCodeLen) {
			bprintf(PRINT_IMPORTANT, _T("  - 68K ROM size:\t0x%08X (Decrypted with key)\n"), nCpsRomLen);
		} else {
			bprintf(PRINT_IMPORTANT, _T("  - 68K ROM size:\t0x%08X (XOR table size: 0x%08X)\n"), nCpsRomLen, nCpsCodeLen);
		}
		bprintf(PRINT_IMPORTANT, _T("  - Z80 ROM size:\t0x%08X\n"), nCpsZRomLen);
		bprintf(PRINT_IMPORTANT, _T("  - Graphics data:\t0x%08X\n"), nCpsGfxLen);
		bprintf(PRINT_IMPORTANT, _T("  - QSound data:\t0x%08X\n"), nCpsQSamLen);
		bprintf(PRINT_IMPORTANT, _T("  - Key data:\t\t0x%08X\n"), nCpsKeyLen);
#endif

		if (/*!nCpsCodeLen ||*/ !nCpsRomLen || !nCpsGfxLen || !nCpsZRomLen || ! nCpsQSamLen) {
			return 1;
		}
	}

	return 0;
}

// ----------------------------------------------------------------

INT32 CpsInit()
{
	INT32 nMemLen, i;
	
	if (Cps == 1) {
		BurnSetRefreshRate(59.61);
	} else {
		if (Cps == 2) {
			BurnSetRefreshRate(59.629403);
		}
	}

	if (!nCPS68KClockspeed) {
		if (!(Cps & 1)) {
			nCPS68KClockspeed = 11800000;
		} else {
			nCPS68KClockspeed = 10000000;
		}
	}
	nCPS68KClockspeed = nCPS68KClockspeed * 100 / nBurnFPS;

	nMemLen = nCpsGfxLen + nCpsRomLen + nCpsCodeLen + nCpsZRomLen + nCpsQSamLen + nCpsAdLen + nCpsKeyLen;

	if (Cps1Qs == 1) {
		nMemLen += nCpsZRomLen * 2;
	}

	// Allocate Gfx, Rom and Z80 Roms
	CpsGfx = (UINT8*)BurnMalloc(nMemLen);
	if (CpsGfx == NULL) {
		return 1;
	}
	memset(CpsGfx, 0, nMemLen);

	CpsRom  = CpsGfx + nCpsGfxLen;
	CpsCode = CpsRom + nCpsRomLen;
	if (Cps1Qs == 1) {
		CpsEncZRom = CpsCode + nCpsCodeLen;
		CpsZRom = CpsEncZRom + nCpsZRomLen * 2;
	} else {
		CpsZRom = CpsCode + nCpsCodeLen;
	}
	CpsQSam =(INT8*)(CpsZRom + nCpsZRomLen);
	CpsAd   =(UINT8*)(CpsQSam + nCpsQSamLen);
	CpsKey  =(UINT8*)(CpsAd + nCpsAdLen);

	// Create Gfx addr mask
	for (i = 0; i < 31; i++) {
		if ((1 << i) >= (INT32)nCpsGfxLen) {
			break;
		}
	}
	nCpsGfxMask = (1 << i) - 1;

	// Offset to Scroll tiles
	if (!(Cps & 1)) {
		nCpsGfxScroll[1] = nCpsGfxScroll[2] = nCpsGfxScroll[3] = 0x800000;
	} else {
		nCpsGfxScroll[1] = nCpsGfxScroll[2] = nCpsGfxScroll[3] = 0;
	}

#if 0
	if (nCpsZRomLen>=5) {
		// 77->cfff and rst 00 in case driver doesn't load
		CpsZRom[0] = 0x3E; CpsZRom[1] = 0x77;
		CpsZRom[2] = 0x32; CpsZRom[3] = 0xFF; CpsZRom[4] = 0xCF;
		CpsZRom[5] = 0xc7;
	}
#endif

	SepTableCalc();									  // Precalc the separate table

	CpsReset = 0; Cpi01A = Cpi01C = Cpi01E = 0;		  // blank other inputs

	// Use this as default - all CPS-2 games use it
	SetCpsBId(CPS_B_21_DEF, 0);

	return 0;
}

INT32 Cps2Init()
{
	Cps = 2;

	if (CpsGetROMs(false)) {
		return 1;
	}

	CpsInit();

	if (CpsGetROMs(true)) {
		return 1;
	}

	return CpsRunInit();
}

INT32 CpsExit()
{
	if (!(Cps & 1)) {
		CpsRunExit();
	}

	CpsLayEn[1] = CpsLayEn[2] = CpsLayEn[3] = CpsLayEn[4] = CpsLayEn[5] = 0;
	nCpsLcReg = 0;
	nCpsGfxScroll[1] = nCpsGfxScroll[2] = nCpsGfxScroll[3] = 0;
	nCpsGfxMask = 0;
	
	Scroll1TileMask = 0;
	Scroll2TileMask = 0;
	Scroll3TileMask = 0;

	nCpsCodeLen = nCpsRomLen = nCpsGfxLen = nCpsZRomLen = nCpsQSamLen = nCpsAdLen = nCpsKeyLen = 0;
	CpsRom = CpsZRom = CpsAd = CpsStar = NULL;
	CpsQSam = NULL;
	CpsKey = NULL;

	// All Memory is allocated to this (this is the only one we can free)
	BurnFree(CpsGfx);
	
	BurnFree(CpsCode);
	
	bCpsUpdatePalEveryFrame = 0;

	nCPS68KClockspeed = 0;
	Cps = 0;
	nCpsNumScanlines = 259;

	return 0;
}
