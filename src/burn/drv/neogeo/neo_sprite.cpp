#include "neogeo.h"

UINT8* NeoZoomROM;

UINT8* NeoSpriteROM[MAX_SLOT] = { NULL, NULL, };

UINT32 nNeoTileMask[MAX_SLOT];
INT32 nNeoMaxTile[MAX_SLOT];

static UINT8* NeoSpriteROMActive;
static UINT32 nNeoTileMaskActive;
static INT32 nNeoMaxTileActive;

static UINT8* NeoTileAttrib[MAX_SLOT] = { NULL, };
static UINT8* NeoTileAttribActive;

INT32 nSliceStart, nSliceEnd, nSliceSize;
static INT32 nNeoEnforceSpriteLimit[MAX_SLOT] = { 0, };

static UINT32* pTileData;
static UINT32* pTilePalette;

static UINT16* pBank;

static INT32 nBankSize;
static INT32 nBankXPos, nBankYPos;
static INT32 nBankXZoom, nBankYZoom;

static INT32 nNeoSpriteFrame04, nNeoSpriteFrame08;

static INT32 nLastBPP = -1;

typedef void (*RenderBankFunction)();
static RenderBankFunction* RenderBank;

static const INT32 MAX_SPRITEBANK = 0x17d;
static const INT32 MAX_SPRITEBANK_LINE = 0x60;
static INT32 nMaxSpriteBank;

static UINT16 BankAttrib01, BankAttrib02, BankAttrib03;

static inline UINT32 alpha_blend(UINT32 d, UINT32 s, UINT32 p)
{
	INT32 a = 255 - p;

	return (((((s & 0xff00ff) * p) + ((d & 0xff00ff) * a)) & 0xff00ff00) +
		((((s & 0x00ff00) * p) + ((d & 0x00ff00) * a)) & 0x00ff0000)) >> 8;
}

void NeoSpriteCalcLimit()
{
	if (nNeoEnforceSpriteLimit[nNeoActiveSlot] == 0) {
		nMaxSpriteBank = MAX_SPRITEBANK; // no limit!
		return;
	}

	nMaxSpriteBank = 0;

	for (INT32 nYLine = 0; nYLine < 240; nYLine++) {
		INT32 nYCount = 0;
		for (INT32 nBank = 0; nBank < MAX_SPRITEBANK; nBank++) {
			BankAttrib02 = *((UINT16*)(NeoGraphicsRAM + 0x010400 + (nBank << 1)));

			if (~BankAttrib02 & 0x40) {
				nBankYPos = (0x0200 - (BankAttrib02 >> 7)) & 0x01FF;

				nBankSize  = BankAttrib02 & 0x3F;
			}

			if (nBankSize && (nBankSize >= 0x20 || ((nYLine - nBankYPos) & 0x1ff) < (nBankSize << 4))) {
				nYCount++;

				if (nBank >= nMaxSpriteBank) {
					nMaxSpriteBank = nBank + 1;
				}
				if (nYCount >= MAX_SPRITEBANK_LINE) {
					break;
				}
			}
		}
	}
}

// Include the tile rendering functions
#include "neo_sprite_func.h"

static UINT8 nSpriteDisableLut[0x200];

INT32 NeoRenderSprites()
{
	if (nLastBPP != nBurnBpp ) {
		nLastBPP = nBurnBpp;

		RenderBank = RenderBankNormal[nBurnBpp - 2];
	}

	if (!NeoSpriteROMActive || !(nBurnLayer & 1)) {
		return 0;
	}

//	UINT16 BankAttrib01, BankAttrib02, BankAttrib03;

	nNeoSpriteFrame04 = nNeoSpriteFrame & 3;
	nNeoSpriteFrame08 = nNeoSpriteFrame & 7;
	
	// ssrpg hack! - NeoCD/SDL
	INT32 nStart = 0;
	if (SekReadWord(0x108) == 0x0085) {
		UINT16 *vidram = (UINT16*)NeoGraphicsRAM;

	   	if ((vidram[0x8202] & 0x40) == 0 && (vidram[0x8203] & 0x40) != 0) {
			nStart = 3;

			while ((vidram[0x8200 + nStart] & 0x40) != 0) nStart++;

			if (nStart == 3) nStart = 0;
		}
	}

	if ((nSpriteEnable != 0xff) || (nBurnLayer & 0x04) == 0)
	{
		memset (nSpriteDisableLut, 0xff, sizeof(nSpriteDisableLut));

		INT32 nDisableSelect = 0;
		UINT16 *SizeAttribute = (UINT16*)(NeoGraphicsRAM + 0x010400);

		for (INT32 nBank = 0; nBank < nMaxSpriteBank; nBank++)
		{
			if (SizeAttribute[nBank] & 0x40) continue;

			if (SizeAttribute[nBank] & 0x3f)
			{
				INT32 nWidth = 1;

				for (INT32 nOffset = 1; nOffset < 0x20; nOffset++)
				{
					if (SizeAttribute[nBank+nOffset] & 0x40)
					{
						nWidth++;
					}
					else break;
				}

				if (nWidth >= 4)
				{
			//		bprintf (0, _T("Found: %d [%d]\n"), nBank, nDisableSelect);

					for (INT32 nOffset = 0; nOffset < nWidth; nOffset++) {
						nSpriteDisableLut[nBank + nOffset] = nDisableSelect;
					}

					nDisableSelect++;
				}
			}
		}
	}

	for (INT32 nBank = 0; nBank < nMaxSpriteBank; nBank++) {
		INT32 zBank = (nBank + nStart) % MAX_SPRITEBANK;
		BankAttrib01 = *((UINT16*)(NeoGraphicsRAM + 0x010000 + (zBank << 1)));
		BankAttrib02 = *((UINT16*)(NeoGraphicsRAM + 0x010400 + (zBank << 1)));
		BankAttrib03 = *((UINT16*)(NeoGraphicsRAM + 0x010800 + (zBank << 1)));

		pBank = (UINT16*)(NeoGraphicsRAM + (zBank << 7));

		if (BankAttrib02 & 0x40) {
			nBankXPos += nBankXZoom + 1;
		} else {
			nBankYPos = (0x0200 - (BankAttrib02 >> 7)) & 0x01FF;
			nBankXPos = (BankAttrib03 >> 7);
			if (nNeoScreenWidth == 304) {
				nBankXPos -= 8;
			}

			nBankYZoom = BankAttrib01 & 0xFF;
			nBankSize  = BankAttrib02 & 0x3F;

//			if (nBankSize > 0x10 && nSliceStart == 0x10) bprintf(PRINT_NORMAL, _T("bank: %04X, x: %04X, y: %04X, zoom: %02X, size: %02X.\n"), zBank, nBankXPos, nBankYPos, nBankYZoom, nBankSize);
		}

		if (nSpriteEnable != 0xff)
		{
			if (nSpriteDisableLut[nBank] < 8)
			{
				if ((nSpriteEnable & (1 << nSpriteDisableLut[nBank])) == 0)
				{
		//			bprintf (0, _T("Bank Type1 %d [%d] disabled\n"), nBank, nSpriteDisableLut[nBank]);
					continue;
				}
			}
		}

		if ((nBurnLayer & 0x04) == 0)
		{
			if (nSpriteDisableLut[nBank] == 0xff)
			{
		//		bprintf (0, _T("Bank Type2 %d [%d] disabled\n"), nBank, nSpriteDisableLut[nBank]);
				continue;
			}
		}

		if (nBankSize) {
			nBankXZoom = (BankAttrib01 >> 8) & 0x0F;
			if (nBankXPos >= 0x01E0) {
				nBankXPos -= 0x200;
			}

			if (nBankXPos >= 0 && nBankXPos < (nNeoScreenWidth - nBankXZoom - 1)) {
				RenderBank[nBankXZoom]();
			} else {
				if (nBankXPos >= -nBankXZoom && nBankXPos < nNeoScreenWidth) {
					RenderBank[nBankXZoom + 16]();
				}
			}
		}
	}

//	bprintf(PRINT_NORMAL, _T("\n"));

	return 0;
}

void NeoUpdateSprites(INT32 nOffset, INT32 nSize)
{
	if (!NeoSpriteROMActive) return;

	for (INT32 i = nOffset & ~127; i < nOffset + nSize; i += 128) {
		bool bTransparent = true;
		for (INT32 j = i; j < i + 128; j++) {
			if (NeoSpriteROMActive[j]) {
				bTransparent = false;
				break;
			}
		}
		if (bTransparent) {
			NeoTileAttribActive[i >> 7] = 1;
		} else {
			NeoTileAttribActive[i >> 7] = 0;
		}
	}
}

void NeoSetSpriteSlot(INT32 nSlot)
{
	NeoTileAttribActive = NeoTileAttrib[nSlot];
	NeoSpriteROMActive  = NeoSpriteROM[nSlot];
	nNeoTileMaskActive  = nNeoTileMask[nSlot];
	nNeoMaxTileActive   = nNeoMaxTile[nSlot];
}

static void NeoBlendInit(INT32 nSlot)
{
	TCHAR filename[MAX_PATH];

	_stprintf(filename, _T("%s%s.bld"), szAppBlendPath, BurnDrvGetText(DRV_NAME));
	
	FILE *fa = _tfopen(filename, _T("rt"));

	if (fa == NULL) {
		_stprintf(filename, _T("%s%s.bld"), szAppBlendPath, BurnDrvGetText(DRV_PARENT));

		fa = _tfopen(filename, _T("rt"));

		if (fa == NULL) {
			return;
		}
	}

	bprintf (PRINT_IMPORTANT, _T("Using sprite blending (.bld) table!\n"));

	char szLine[64];

	INT32 table[4] = { 0, 0xff-0x3f, 0xff-0x7f, 0xff-0x7f }; // last one 7f?

	while (1)
	{
		if (fgets (szLine, 64, fa) == NULL) break;

		if (strncmp ("Game", szLine, 4) == 0) continue; 	// don't care
		if (strncmp ("Name", szLine, 4) == 0) continue; 	// don't care
		if (szLine[0] == ';') continue;				// comment (also don't care)

		INT32 type, single_entry = -1;
		UINT32 min,max,k;

		for (k = 0; k < strlen(szLine); k++) {
			if (szLine[k] == '-') { single_entry = k+1; break; }
		}

		if (single_entry < 0) {
			sscanf(szLine,"%x %d",&max,&type);
			min = max;
		} else {
			sscanf(szLine,"%x",&min);
			sscanf(szLine+single_entry,"%x %d",&max,&type);
		}

		for (k = min; k <= max; k++) {
			if (k < nNeoTileMask[nSlot] + 1 && NeoTileAttrib[nSlot][k] != 1) 	// ?
				NeoTileAttrib[nSlot][k] = table[type&3];
		}
	}

	fclose (fa);
}

INT32 NeoInitSprites(INT32 nSlot)
{
	// Create a table that indicates if a tile is transparent
	NeoTileAttrib[nSlot] = (UINT8*)BurnMalloc(nNeoTileMask[nSlot] + 1);

	for (INT32 i = 0; i < nNeoMaxTile[nSlot]; i++) {
		bool bTransparent = true;
		for (INT32 j = i << 7; j < (i + 1) << 7; j++) {
			if (NeoSpriteROM[nSlot][j]) {
				bTransparent = false;
				break;
			}
		}
		if (bTransparent) {
			NeoTileAttrib[nSlot][i] = 1;
		} else {
			NeoTileAttrib[nSlot][i] = 0;
		}
	}
	for (UINT32 i = nNeoMaxTile[nSlot]; i < nNeoTileMask[nSlot] + 1; i++) {
		NeoTileAttrib[nSlot][i] = 1;
	}

	if (bBurnUseBlend) NeoBlendInit(nSlot);

	NeoTileAttribActive = NeoTileAttrib[nSlot];
	NeoSpriteROMActive  = NeoSpriteROM[nSlot];
	nNeoTileMaskActive  = nNeoTileMask[nSlot];
	nNeoMaxTileActive   = nNeoMaxTile[nSlot];

	nNeoEnforceSpriteLimit[nSlot] = 0; // off by default

	if (!strcmp(BurnDrvGetTextA(DRV_NAME), "bstars") || !strcmp(BurnDrvGetTextA(DRV_NAME), "bstarsh")) {
		nNeoEnforceSpriteLimit[nSlot] = 1; // bstars needs this for proper homerun cutscene
	}

	return 0;
}

void NeoExitSprites(INT32 nSlot)
{
	BurnFree(NeoTileAttrib[nSlot]);
	NeoTileAttribActive = NULL;
}
