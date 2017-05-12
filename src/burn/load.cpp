// Burn - Rom Loading module
#include "burnint.h"

// Load a rom and separate out the bytes by nGap
// Dest is the memory block to insert the rom into
INT32 BurnLoadRomExt(UINT8 *Dest, INT32 i, INT32 nGap, INT32 nFlags)
{
	INT32 nRet = 0, nLen = 0;
	if (BurnExtLoadRom == NULL) return 1; // Load function was not defined by the application

	// Find the length of the rom (as given by the current driver)
	{
		struct BurnRomInfo ri;
		ri.nType=0;
		ri.nLen=0;
		BurnDrvGetRomInfo(&ri,i);
		if (ri.nType==0) return 0; // Empty rom slot - don't load anything and return success
		nLen=ri.nLen;
	}

	char* RomName = ""; //added by emufan
	BurnDrvGetRomName(&RomName, i, 0);

	if (nLen <= 0) return 1;

	if ((nGap>1) || (nFlags & LD_NIBBLES) || (nFlags & LD_XOR))
	{
		INT32 nLoadLen=0;
		UINT8 *Load=(UINT8 *)BurnMalloc(nLen);
		if (Load==NULL) return 1;
		memset(Load,0,nLen);

		// Load in the file
		nRet=BurnExtLoadRom(Load,&nLoadLen,i);
		if (bDoIpsPatch) IpsApplyPatches(Load, RomName);
		if (nRet!=0) { if (Load) { BurnFree(Load); Load = NULL; } return 1; }

		if (nLoadLen<0) nLoadLen=0;
		if (nLoadLen>nLen) nLoadLen=nLen;

		INT32 nGroup = (LD_GROUP(nFlags) > 0) ? LD_GROUP(nFlags) : 1;
		INT32 nInvert = (nFlags & LD_INVERT) ? 0xff : 0;
		INT32 nByteswap = (nFlags & LD_BYTESWAP) ? 1 : 0;
		INT32 nReverse = (nGroup > 1) ? (nFlags & LD_REVERSE) : 0;
		INT32 nXor = (nFlags & LD_XOR) ? 1 : 0;
		INT32 nNibbles = (nFlags & LD_NIBBLES) ? 1 : 0;
		UINT8 *Src = Load;

		if (nNibbles) { nGroup = 1; nGap = 2; }

		for (INT32 n = 0, z = 0; n < nLoadLen; n += nGroup, z += nGap) {
			if (nNibbles) {
				Dest[z+0] = (Src[n^nByteswap] ^ nInvert) & 0xf;
				Dest[z+1] = (Src[n^nByteswap] ^ nInvert) >> 4;
			} else {
				if (nReverse) {
					for (INT32 j = 0; j < nGroup; j++) {
						INT32 nXorData = nInvert;
						if (nXor) nXorData ^= Dest[z + j];
						Dest[z + j] = Src[(n + ((nGroup - 1) - j)) ^ nByteswap] ^ nXorData;
					}
				} else {
					for (INT32 j = 0; j < nGroup; j++) {
						INT32 nXorData = nInvert;
						if (nXor) nXorData ^= Dest[z + j];
						Dest[z + j] = Src[(n + j) ^ nByteswap] ^ nXorData;
					}
				}
			}
		}

		if (Load) {
			BurnFree(Load);
			Load = NULL;
		}
	}
	else
	{
 		// If no XOR, and gap of 1, just copy straight in
		nRet=BurnExtLoadRom(Dest,NULL,i);
		if (bDoIpsPatch) IpsApplyPatches(Dest, RomName);
		if (nRet!=0) return 1;

		if (nFlags & LD_INVERT) {
			for (INT32 n = 0; n < nLen; n++) {
				Dest[n] ^= 0xff;
			}
		}

		if (nFlags & LD_BYTESWAP) {
			BurnByteswap(Dest, nLen);
		}
	}

	return 0;
}

INT32 BurnLoadRom(UINT8 *Dest, INT32 i, INT32 nGap)
{
	return BurnLoadRomExt(Dest,i,nGap,0);
}

INT32 BurnXorRom(UINT8 *Dest, INT32 i, INT32 nGap)
{
	return BurnLoadRomExt(Dest,i,nGap,LD_XOR);
}

// Separate out a bitfield into Bit number 'nField' of each nibble in pDest
// (end result: each dword in memory carries the 8 pixels of a tile line).
INT32 BurnLoadBitField(UINT8 *pDest, UINT8 *pSrc, INT32 nField, INT32 nSrcLen)
{
	INT32 nPix=0;

	for (nPix=0; nPix<(nSrcLen<<3); nPix++)
	{
		INT32 nBit;
		// Get the bitplane pixel value (on or off)
		nBit=(*pSrc)>>(7-(nPix&7)); nBit&=1;
		nBit<<=nField; // Move to correct bit for this field

		// use low nibble for each even pixel
		if ((nPix&1)==1) nBit<<=4; // use high nibble for each odd pixel

		*pDest|=nBit; // OR into destination
		if ((nPix&1)==1) pDest++;
		if ((nPix&7)==7) pSrc++;
  	}

	return 0;
}

