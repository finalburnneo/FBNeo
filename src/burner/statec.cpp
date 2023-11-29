// Driver State Compression module
#include "zlib.h"

#include "burnint.h"

static UINT8* Comp = nullptr; // Compressed data buffer
static INT32 nCompLen = 0;
static INT32 nCompFill = 0; // How much of the buffer has been filled so far

static z_stream Zstr; // Deflate stream

// -----------------------------------------------------------------------------
// Compression

static INT32 CompEnlarge(INT32 nAdd)
{
	void* NewMem = nullptr;

	// Need to make more room in the compressed buffer
	NewMem = realloc(Comp, nCompLen + nAdd);
	if (NewMem == nullptr)
	{
		return 1;
	}

	Comp = static_cast<UINT8*>(NewMem);
	memset(Comp + nCompLen, 0, nAdd);
	nCompLen += nAdd;

	return 0;
}

static INT32 CompGo(INT32 bFinish)
{
	INT32 nResult = 0;
	INT32 nAvailOut = 0;

	bool bRetry, bOverflow;

	do
	{
		bRetry = false;

		// Point to the remainder of out buffer
		Zstr.next_out = Comp + nCompFill;
		nAvailOut = nCompLen - nCompFill;
		if (nAvailOut < 0)
		{
			nAvailOut = 0;
		}
		Zstr.avail_out = nAvailOut;

		// Try to deflate into the buffer (there may not be enough room though)
		if (bFinish)
		{
			nResult = deflate(&Zstr, Z_FINISH); // deflate and finish
			if (nResult != Z_OK && nResult != Z_STREAM_END)
			{
				return 1;
			}
		}
		else
		{
			nResult = deflate(&Zstr, 0); // deflate
			if (nResult != Z_OK)
			{
				return 1;
			}
		}

		nCompFill = Zstr.next_out - Comp; // Update how much has been filled

		// Check for overflow
		bOverflow = bFinish ? (nResult == Z_OK) : (Zstr.avail_out <= 0);

		if (bOverflow)
		{
			if (CompEnlarge(4 * 1024))
			{
				return 1;
			}

			bRetry = true;
		}
	}
	while (bRetry);

	return 0;
}

static INT32 __cdecl StateCompressAcb(struct BurnArea* pba)
{
	// Set the data as the next available input
	Zstr.next_in = static_cast<UINT8*>(pba->Data);
	Zstr.avail_in = pba->nLen;

	CompGo(0); // Compress this Area

	Zstr.avail_in = 0;
	Zstr.next_in = nullptr;

	return 0;
}

// --------- Raw / Uncompressed state handling ----------
static INT32 nTotalLenUncomp = 0;
static UINT8* BufferUncomp = nullptr;
static UINT8* pBufferUncomp = nullptr;

static INT32 __cdecl UncompLenAcb(struct BurnArea* pba)
{
	nTotalLenUncomp += pba->nLen;

	return 0;
}

static INT32 __cdecl UncompSaveAcb(struct BurnArea* pba)
{
	memcpy(pBufferUncomp, pba->Data, pba->nLen);
	pBufferUncomp += pba->nLen;

	return 0;
}

static INT32 __cdecl UncompLoadAcb(struct BurnArea* pba)
{
	memcpy(pba->Data, pBufferUncomp, pba->nLen);
	pBufferUncomp += pba->nLen;

	return 0;
}

// Compress a state using deflate
INT32 BurnStateCompress(UINT8** pDef, INT32* pnDefLen, INT32 bAll)
{
	if ((BurnDrvGetHardwareCode() & 0xffff0000) == HARDWARE_CAVE_CV1000)
	{
		// Systems with a huge amount of data can be defined here to
		// use this raw state handler.

		nTotalLenUncomp = 0;
		BurnAcb = UncompLenAcb; // Get length of state buffer

		if (bAll) BurnAreaScan(ACB_FULLSCAN | ACB_READ, nullptr); // scan all ram, read (from driver <- decompress)
		else BurnAreaScan(ACB_NVRAM | ACB_READ, nullptr); // scan nvram,   read (from driver <- decompress)

		BufferUncomp = static_cast<UINT8*>(malloc(nTotalLenUncomp));

		pBufferUncomp = BufferUncomp;
		BurnAcb = UncompSaveAcb;

		if (bAll) BurnAreaScan(ACB_FULLSCAN | ACB_READ, nullptr); // scan all ram, read (from driver <- decompress)
		else BurnAreaScan(ACB_NVRAM | ACB_READ, nullptr); // scan nvram,   read (from driver <- decompress)

		// Return the buffer
		if (pDef)
		{
			*pDef = BufferUncomp;
		}
		if (pnDefLen)
		{
			*pnDefLen = nTotalLenUncomp;
		}

		return 0;
	}

	// FBN-Standard / Compressed state handler
	void* NewMem = nullptr;

	memset(&Zstr, 0, sizeof(Zstr));

	Comp = nullptr;
	nCompLen = 0;
	nCompFill = 0; // Begin with a zero-length buffer
	if (CompEnlarge(8 * 1024))
	{
		return 1;
	}

	deflateInit(&Zstr, Z_DEFAULT_COMPRESSION);

	BurnAcb = StateCompressAcb; // callback our function with each area

	if (bAll) BurnAreaScan(ACB_FULLSCAN | ACB_READ, nullptr); // scan all ram, read (from driver <- decompress)
	else BurnAreaScan(ACB_NVRAM | ACB_READ, nullptr); // scan nvram,   read (from driver <- decompress)

	// Finish off
	CompGo(1);

	deflateEnd(&Zstr);

	// Size down
	NewMem = realloc(Comp, nCompFill);
	if (NewMem)
	{
		Comp = static_cast<UINT8*>(NewMem);
		nCompLen = nCompFill;
	}

	// Return the buffer
	if (pDef)
	{
		*pDef = Comp;
	}
	if (pnDefLen)
	{
		*pnDefLen = nCompFill;
	}

	return 0;
}

// -----------------------------------------------------------------------------
// Decompression

static INT32 __cdecl StateDecompressAcb(struct BurnArea* pba)
{
	Zstr.next_out = static_cast<UINT8*>(pba->Data);
	Zstr.avail_out = pba->nLen;

	inflate(&Zstr, Z_SYNC_FLUSH);

	Zstr.avail_out = 0;
	Zstr.next_out = nullptr;

	return 0;
}

INT32 BurnStateDecompress(UINT8* Def, INT32 nDefLen, INT32 bAll)
{
	if ((BurnDrvGetHardwareCode() & 0xffff0000) == HARDWARE_CAVE_CV1000)
	{
		// Systems with a huge amount of data can be defined here to
		// use this raw state handler.
		pBufferUncomp = Def;
		BurnAcb = UncompLoadAcb;

		if (bAll) BurnAreaScan(ACB_FULLSCAN | ACB_WRITE, nullptr); // scan all ram, write (to driver <- decompress)
		else BurnAreaScan(ACB_NVRAM | ACB_WRITE, nullptr); // scan nvram,   write (to driver <- decompress)

		return 0;
	}

	// FBN-Standard / Compressed state handler
	memset(&Zstr, 0, sizeof(Zstr));
	inflateInit(&Zstr);

	// Set all of the buffer as available input
	Zstr.next_in = Def;
	Zstr.avail_in = nDefLen;

	BurnAcb = StateDecompressAcb; // callback our function with each area

	if (bAll) BurnAreaScan(ACB_FULLSCAN | ACB_WRITE, nullptr); // scan all ram, write (to driver <- decompress)
	else BurnAreaScan(ACB_NVRAM | ACB_WRITE, nullptr); // scan nvram,   write (to driver <- decompress)

	inflateEnd(&Zstr);
	memset(&Zstr, 0, sizeof(Zstr));

	return 0;
}
