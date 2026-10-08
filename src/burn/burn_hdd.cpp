// Burn - hard disk image module (CHD via libchdr, or raw dumps)

#include "burnint.h"
#include "burn_hdd.h"
#include "chd.h"
#include <map>
#include <vector>



#ifdef __LIBRETRO__
#include <streams/file_stream_transforms.h>
#define hddFseek	fseek
#define hddFtell	ftell
#else
#ifdef _MSC_VER
#define hddFseek	_fseeki64
#define hddFtell	_ftelli64
#else
#define hddFseek	fseeko
#define hddFtell	ftello
#endif
#endif



#ifdef _UNICODE
#define HDD_NARROW_FMT	_T("%hs")
#else
#define HDD_NARROW_FMT	_T("%s")
#endif

#define HDD_CACHE_HUNKS		8
#define HDD_DIFF_MAGIC		"FBNHDIFF"
#define HDD_DIFF_VERSION	1

struct BurnHDD {
	FILE* pFile;
	chd_file* pChd;					// NULL for raw dumps

	UINT32 nSectorSize;
	UINT32 nSectorCount;
	INT32 nCylinders, nHeads, nSectors;	// 0 when unknown

	// CHD hunk cache (direct mapped)
	UINT32 nHunkBytes;
	UINT32 nTotalHunks;
	UINT8* pCache;
	INT32 nCacheTag[HDD_CACHE_HUNKS];

	std::map<UINT32, std::vector<UINT8> > Diff;
	bool bDiffDirty;
	TCHAR szDiffPath[MAX_PATH];
};

static void Put32(UINT8* p, UINT32 v)
{
	p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}

static UINT32 Get32(const UINT8* p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | ((UINT32)p[3] << 24);
}

static FILE* OpenInDir(const TCHAR* szDir, const char* szName, TCHAR* szPathOut)
{
	if (szDir == NULL || szDir[0] == 0) return NULL;

	_stprintf(szPathOut, _T("%s%s/") HDD_NARROW_FMT, szAppHDDPath, szDir, szName);
	return _tfopen(szPathOut, _T("rb"));
}

static INT32 OpenChd(BurnHDD* pDisk)
{
	if (chd_open_file(pDisk->pFile, CHD_OPEN_READ, NULL, &pDisk->pChd) != CHDERR_NONE) {
		pDisk->pChd = NULL;
		return 1;
	}

	const chd_header* pHeader = chd_get_header(pDisk->pChd);
	char szMeta[256];
	UINT32 nMetaLen = 0;

	if (pHeader == NULL || pHeader->hunkbytes == 0 ||
		chd_get_metadata(pDisk->pChd, HARD_DISK_METADATA_TAG, 0, szMeta, sizeof(szMeta) - 1, &nMetaLen, NULL, NULL) != CHDERR_NONE) {
		bprintf(PRINT_ERROR, _T("HDD: the CHD is not a hard disk image\n"));
		return 1;
	}
	szMeta[nMetaLen < sizeof(szMeta) ? nMetaLen : sizeof(szMeta) - 1] = 0;

	INT32 nCyls = 0, nHeads = 0, nSecs = 0, nBps = 0;
	if (sscanf(szMeta, HARD_DISK_METADATA_FORMAT, &nCyls, &nHeads, &nSecs, &nBps) != 4 || nBps <= 0 || pHeader->hunkbytes % nBps) {
		bprintf(PRINT_ERROR, _T("HDD: bad CHD geometry\n"));
		return 1;
	}

	pDisk->nCylinders = nCyls;
	pDisk->nHeads = nHeads;
	pDisk->nSectors = nSecs;
	pDisk->nSectorSize = nBps;
	pDisk->nSectorCount = (UINT32)(pHeader->logicalbytes / nBps);
	pDisk->nHunkBytes = pHeader->hunkbytes;
	pDisk->nTotalHunks = pHeader->totalhunks;

	pDisk->pCache = (UINT8*)malloc((size_t)pDisk->nHunkBytes * HDD_CACHE_HUNKS);
	if (pDisk->pCache == NULL) return 1;
	for (INT32 i = 0; i < HDD_CACHE_HUNKS; i++) pDisk->nCacheTag[i] = -1;

	return 0;
}

static INT32 OpenRaw(BurnHDD* pDisk)
{
	hddFseek(pDisk->pFile, 0, SEEK_END);
	INT64 nSize = (INT64)hddFtell(pDisk->pFile);
	hddFseek(pDisk->pFile, 0, SEEK_SET);

	pDisk->nSectorSize = 512;
	pDisk->nSectorCount = (UINT32)(nSize / 512);
	return (pDisk->nSectorCount == 0);
}

static void LoadDiff(BurnHDD* pDisk)
{
	FILE* fp = _tfopen(pDisk->szDiffPath, _T("rb"));
	if (fp == NULL) return;

	UINT8 hdr[20];
	if (fread(hdr, sizeof(hdr), 1, fp) == 1 && memcmp(hdr, HDD_DIFF_MAGIC, 8) == 0 &&
		Get32(hdr + 8) == HDD_DIFF_VERSION && Get32(hdr + 12) == pDisk->nSectorSize) {
		UINT32 nEntries = Get32(hdr + 16);
		std::vector<UINT8> entry(4 + pDisk->nSectorSize);

		for (UINT32 i = 0; i < nEntries; i++) {
			if (fread(&entry[0], entry.size(), 1, fp) != 1) break;
			UINT32 nLba = Get32(&entry[0]);
			if (nLba >= pDisk->nSectorCount) continue;
			pDisk->Diff[nLba].assign(entry.begin() + 4, entry.end());
		}

		bprintf(PRINT_NORMAL, _T("HDD: %d modified sectors loaded from %s\n"), (INT32)pDisk->Diff.size(), pDisk->szDiffPath);
	} else {
		bprintf(PRINT_ERROR, _T("HDD: %s ignored (not made for this disk)\n"), pDisk->szDiffPath);
	}

	fclose(fp);
}

BurnHDD* BurnHDDOpen(INT32 nIndex)
{
	char* szName = NULL;
	if (BurnDrvGetHDDName(&szName, nIndex, 0) || szName == NULL) return NULL;

	TCHAR szPath[MAX_PATH];
	FILE* fp = OpenInDir(BurnDrvGetText(DRV_NAME), szName, szPath);
	if (fp == NULL) fp = OpenInDir(BurnDrvGetText(DRV_PARENT), szName, szPath);
	if (fp == NULL) {
		bprintf(PRINT_ERROR, _T("HDD: ") HDD_NARROW_FMT _T(" not found\n"), szName);
		return NULL;
	}

	BurnHDD* pDisk = new BurnHDD;
	pDisk->pFile = fp;
	pDisk->pChd = NULL;
	pDisk->nCylinders = pDisk->nHeads = pDisk->nSectors = 0;
	pDisk->nHunkBytes = pDisk->nTotalHunks = 0;
	pDisk->pCache = NULL;
	pDisk->bDiffDirty = false;

	// a CHD is recognised by its signature, whatever the file extension
	char sig[8] = { 0 };
	bool bChd = (fread(sig, 8, 1, fp) == 1 && memcmp(sig, "MComprHD", 8) == 0);
	hddFseek(fp, 0, SEEK_SET);

	if (bChd ? OpenChd(pDisk) : OpenRaw(pDisk)) {
		bprintf(PRINT_ERROR, _T("HDD: can't use %s\n"), szPath);
		if (pDisk->pChd) chd_close(pDisk->pChd);
		fclose(fp);
		free(pDisk->pCache);
		delete pDisk;
		return NULL;
	}

	if (nIndex == 0) {
		_stprintf(pDisk->szDiffPath, _T("%s%s.diff"), szAppEEPROMPath, BurnDrvGetText(DRV_NAME));
	} else {
		_stprintf(pDisk->szDiffPath, _T("%s%s_%d.diff"), szAppEEPROMPath, BurnDrvGetText(DRV_NAME), nIndex);
	}
	LoadDiff(pDisk);

	bprintf(PRINT_NORMAL, _T("HDD: %s, %s, %d sectors of %d bytes\n"), szPath, bChd ? _T("CHD") : _T("raw"),
		pDisk->nSectorCount, pDisk->nSectorSize);

	return pDisk;
}

INT32 BurnHDDFlush(BurnHDD* pDisk)
{
	if (pDisk == NULL) return 1;
	if (!pDisk->bDiffDirty) return 0;

	FILE* fp = _tfopen(pDisk->szDiffPath, _T("wb"));
	if (fp == NULL) {
		bprintf(PRINT_ERROR, _T("HDD: can't write %s\n"), pDisk->szDiffPath);
		return 1;
	}

	UINT8 hdr[20];
	memcpy(hdr, HDD_DIFF_MAGIC, 8);
	Put32(hdr + 8, HDD_DIFF_VERSION);
	Put32(hdr + 12, pDisk->nSectorSize);
	Put32(hdr + 16, (UINT32)pDisk->Diff.size());
	INT32 nRet = (fwrite(hdr, sizeof(hdr), 1, fp) != 1);

	UINT8 lba[4];
	for (std::map<UINT32, std::vector<UINT8> >::iterator it = pDisk->Diff.begin(); it != pDisk->Diff.end() && !nRet; ++it) {
		Put32(lba, it->first);
		nRet |= (fwrite(lba, 4, 1, fp) != 1);
		nRet |= (fwrite(&it->second[0], pDisk->nSectorSize, 1, fp) != 1);
	}

	fclose(fp);
	if (nRet == 0) pDisk->bDiffDirty = false;
	return nRet;
}

void BurnHDDClose(BurnHDD* pDisk)
{
	if (pDisk == NULL) return;

	BurnHDDFlush(pDisk);

	if (pDisk->pChd) chd_close(pDisk->pChd);
	if (pDisk->pFile) fclose(pDisk->pFile);		// libchdr does not close a FILE* it was given
	free(pDisk->pCache);
	delete pDisk;
}

UINT32 BurnHDDGetSectorSize(BurnHDD* pDisk)
{
	return pDisk ? pDisk->nSectorSize : 0;
}

UINT32 BurnHDDGetSectorCount(BurnHDD* pDisk)
{
	return pDisk ? pDisk->nSectorCount : 0;
}

INT32 BurnHDDGetGeometry(BurnHDD* pDisk, INT32* pnCylinders, INT32* pnHeads, INT32* pnSectors)
{
	if (pDisk == NULL || pDisk->nCylinders == 0) return 1;

	if (pnCylinders) *pnCylinders = pDisk->nCylinders;
	if (pnHeads) *pnHeads = pDisk->nHeads;
	if (pnSectors) *pnSectors = pDisk->nSectors;
	return 0;
}

static INT32 ReadImageSector(BurnHDD* pDisk, UINT32 nLba, UINT8* pDest)
{
	if (pDisk->pChd == NULL) {
		if (hddFseek(pDisk->pFile, (INT64)nLba * pDisk->nSectorSize, SEEK_SET) != 0) return 1;
		return (fread(pDest, pDisk->nSectorSize, 1, pDisk->pFile) != 1);
	}

	UINT64 nOffset = (UINT64)nLba * pDisk->nSectorSize;
	UINT32 nHunk = (UINT32)(nOffset / pDisk->nHunkBytes);
	if (nHunk >= pDisk->nTotalHunks) return 1;

	INT32 nSlot = nHunk % HDD_CACHE_HUNKS;
	UINT8* pHunk = pDisk->pCache + (size_t)nSlot * pDisk->nHunkBytes;

	if (pDisk->nCacheTag[nSlot] != (INT32)nHunk) {
		if (chd_read(pDisk->pChd, nHunk, pHunk) != CHDERR_NONE) {
			pDisk->nCacheTag[nSlot] = -1;
			return 1;
		}
		pDisk->nCacheTag[nSlot] = nHunk;
	}

	memcpy(pDest, pHunk + (nOffset % pDisk->nHunkBytes), pDisk->nSectorSize);
	return 0;
}

INT32 BurnHDDRead(BurnHDD* pDisk, UINT32 nLba, UINT32 nCount, void* pDest)
{
	if (pDisk == NULL || pDest == NULL) return 1;

	UINT8* pDst = (UINT8*)pDest;
	INT32 nRet = 0;

	for (UINT32 i = 0; i < nCount; i++, nLba++, pDst += pDisk->nSectorSize) {
		std::map<UINT32, std::vector<UINT8> >::iterator it = pDisk->Diff.find(nLba);
		if (it != pDisk->Diff.end()) {
			memcpy(pDst, &it->second[0], pDisk->nSectorSize);
			continue;
		}

		if (nLba >= pDisk->nSectorCount || ReadImageSector(pDisk, nLba, pDst)) {
			memset(pDst, 0, pDisk->nSectorSize);
			nRet = 1;
		}
	}

	return nRet;
}

INT32 BurnHDDWrite(BurnHDD* pDisk, UINT32 nLba, UINT32 nCount, const void* pSrc)
{
	if (pDisk == NULL || pSrc == NULL) return 1;

	const UINT8* pS = (const UINT8*)pSrc;
	INT32 nRet = 0;

	for (UINT32 i = 0; i < nCount; i++, nLba++, pS += pDisk->nSectorSize) {
		if (nLba >= pDisk->nSectorCount) {
			nRet = 1;
			continue;
		}
		pDisk->Diff[nLba].assign(pS, pS + pDisk->nSectorSize);
		pDisk->bDiffDirty = true;
	}

	return nRet;
}

void BurnHDDDiscardWrites(BurnHDD* pDisk)
{
	if (pDisk == NULL) return;

	pDisk->Diff.clear();
	pDisk->bDiffDirty = true;	// an empty diff file replaces the old one
	BurnHDDFlush(pDisk);
}
