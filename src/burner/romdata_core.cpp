// =============================================================================
//  FBNeo  -  RomData core (platform-independent)
// =============================================================================

#include "burnint.h"
#include "romdata_core.h"
#include "drv/capcom/cps.h"
#include "drv/galaxian/gal.h"
#include "drv/megadrive/megadrive.h"
#include "drv/sega/sys16.h"
#include "drv/taito/taito.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <sys/stat.h>
#if defined(BUILD_WIN32) || defined(_WIN32)
#if defined(_UNICODE) && !defined(UNICODE)
#define UNICODE
#endif
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#ifdef _MSC_VER
#include "dirent.h"
#else
#include <dirent.h>
#endif

// The CRT has no _t-prefixed opendir/readdir/closedir, so map them here.
#ifdef _UNICODE
#define RD_DIR		_WDIR
#define RD_dirent	_wdirent
#define rd_opendir	_wopendir
#define rd_readdir	_wreaddir
#define rd_closedir	_wclosedir
#else
#define RD_DIR		DIR
#define RD_dirent	dirent
#define rd_opendir	opendir
#define rd_readdir	readdir
#define rd_closedir	closedir
#endif
#define rd_name(de)	((de)->d_name)			// native width: narrowing mangles non-ASCII names

#define SHORT_MAX	128
#define DATE_MAX	32

#define RD_BOARDROM_BASE	0x80			// STDROMPICKEXT reserves 0x80+ for the board/BIOS set

#define RD_CACHE_SCHEMA_VERSION			1
#define RD_CACHE_IDENTIFY_VERSION		1

#define RD_CACHE_MAX_ENTRIES			100000
#define RD_CACHE_MAX_ROMS				4096
#define RD_CACHE_MAX_PAYLOAD			(64 * 1024 * 1024)

static const UINT8 RDCacheMagic[8]   = { 'F', 'B', 'N', 'R', 'D', 'A', 'T', 'A' };
static const UINT8 RDCacheTrailer[8] = { 'R', 'D', 'A', 'T', 'A', 'E', 'N', 'D' };

// Lightweight RomData driver record.
// NOTE: the first member is declared without the 'struct' keyword - gamelist.pl
// scans sources for "struct BurnDriver <name>" and would emit this member as a
// driver (undefined symbol at link time).
struct RomDataDrv {
	BurnDriver	drv;						// embedded engine driver (appended to pDriverEx)
	struct BurnRomInfo* pRomInfo;			// the ONE ROM table (final form, owns szName)
	UINT32 nRomInfoCount;
	char*  pszShortName;
	char*  pszDrvName;
	char*  pszDate;
	char*  pszFullNameA;
	wchar_t* pszFullNameW;
	char*  pszExtName;
	struct BurnDriver* pBaseDriver;			// serves the board/BIOS ROMs the .dat does not carry
};

static struct RomDataDrv** pRDDrv = NULL;	// array of RomData records
static UINT32 nRDDrvCount    = 0;
static UINT32 nRDDrvCapacity = 0;

static inline char* rd_strdup(const char* s)
{
	if (!s)
		return NULL;

	size_t n = strlen(s) + 1;
	char* p = (char*)malloc(n);
	if (p)
		memcpy(p, s, n);

	return p;
}

static inline bool rd_is_empty(const TCHAR* s)
{
	return (!s || *s == _T('\0'));
}

static inline bool rd_is_emptyA(const char* s)
{
	return (!s || *s == '\0');
}

static bool rd_multiply_size(UINT32 nCount, size_t nItemSize, size_t* pSize)
{
	if (!pSize || (nItemSize && nCount > (size_t)-1 / nItemSize))
		return false;

	*pSize = (size_t)nCount * nItemSize;
	return true;
}

static INT32 rd_reserve_records(UINT32 nRequired)
{
	if (nRequired <= nRDDrvCapacity)
		return 0;

	UINT32 nCapacity = nRDDrvCapacity ? nRDDrvCapacity : 32;
	while (nCapacity < nRequired) {
		if (nCapacity > ~0U / 2) {
			nCapacity = nRequired;
			break;
		}
		nCapacity *= 2;
	}

	size_t nSize;
	if (!rd_multiply_size(nCapacity, sizeof(struct RomDataDrv*), &nSize))
		return 1;

	struct RomDataDrv** pRecords = (struct RomDataDrv**)realloc(pRDDrv, nSize);
	if (!pRecords)
		return 1;

	memset(pRecords + nRDDrvCapacity, 0, (size_t)(nCapacity - nRDDrvCapacity) * sizeof(struct RomDataDrv*));
	pRDDrv = pRecords;
	nRDDrvCapacity = nCapacity;
	return 0;
}

// Only .dat files are ours (case-insensitive).
static inline bool rd_is_dat(const TCHAR* szPath)
{
	const TCHAR* dot = szPath ? _tcsrchr(szPath, _T('.')) : NULL;
	return dot != NULL && _tcsicmp(dot, _T(".dat")) == 0;
}

//  Encoding detection + normalisation to UTF-8/char
enum RDEncoding {
	RD_ENC_ANSI = 0,	// unknown 8-bit / ASCII-compatible; passed through verbatim
	RD_ENC_UTF8,		// UTF-8 without BOM (ASCII is a subset)
	RD_ENC_UTF8_BOM,
	RD_ENC_UTF16_LE,
	RD_ENC_UTF16_BE,
	RD_ENC_UTF16_LE_NOBOM,
};

static bool rd_is_valid_utf8(const UINT8* buf, size_t len)
{
	size_t p = 0;
	while (p < len) {
		UINT8 c = buf[p];
		if (c == 0x00)
			return false;								// embedded NUL -> not UTF-8
		if (c < 0x80) {
			p++;
			continue;
		}												// ASCII

		INT32 nExtra;
		if (c >= 0xc2 && c <= 0xdf)
			nExtra = 1;									// 2-byte
		else if (c >= 0xe0 && c <= 0xef)
			nExtra = 2;									// 3-byte
		else if (c >= 0xf0 && c <= 0xf4)
			nExtra = 3;									// 4-byte
		else
			return false;								// invalid lead byte -> not UTF-8

		// Each of the nExtra continuation bytes must exist and be 10xxxxxx.
		for (INT32 i = 1; i <= nExtra; i++) {
			if (p + (size_t)i >= len)
				return false;							// truncated sequence at EOF
			if ((buf[p + i] & 0xc0) != 0x80)
				return false;
		}
		p += nExtra + 1;
	}
	return true;
}

static UINT8* rd_read_all(FILE* fp, size_t* pLen)
{
	if (!fp || !pLen)
		return NULL;

	*pLen = 0;
#if defined(BUILD_WIN32) || defined(_WIN32)
	if (_fseeki64(fp, 0, SEEK_END) != 0)
		return NULL;

	INT64 nSize = _ftelli64(fp);
	if (nSize < 0 || (UINT64)nSize > (UINT64)SIZE_MAX - 1 || _fseeki64(fp, 0, SEEK_SET) != 0)
		return NULL;
#else
	if (fseeko(fp, 0, SEEK_END) != 0)
		return NULL;

	INT64 nSize = (INT64)ftello(fp);
	if (nSize < 0 || (UINT64)nSize > (UINT64)SIZE_MAX - 1 || fseeko(fp, 0, SEEK_SET) != 0)
		return NULL;
#endif
	UINT8* buf = (UINT8*)malloc((size_t)nSize + 1);
	if (!buf)
		return NULL;

	size_t got = fread(buf, 1, (size_t)nSize, fp);
	if (got != (size_t)nSize || ferror(fp)) {			// short read -> reject, don't parse a truncated dat
		free_s((void**)&buf);
		return NULL;
	}
	buf[got] = 0;
	*pLen = got;
	return buf;
}

// UTF-16LE without BOM, per Notepad++ Utf8_16_Read::determineEncoding().
// N++ deliberately dropped BE-without-BOM detection ("very weak"), so neither do we.
static bool rd_is_utf16le_nobom(const UINT8* buf, size_t len)
{
	if (!buf || len < 2 || (len & 1) || buf[0] == 0 || buf[1] != 0)
		return false;

#if defined(BUILD_WIN32) || defined(_WIN32)
	INT32 nFlags = IS_TEXT_UNICODE_STATISTICS;
	return IsTextUnicode(buf, (INT32)(len > INT_MAX ? INT_MAX : len), &nFlags) != 0;
#else
	size_t units = len / 2;
	size_t zeroHigh = 0;
	for (size_t i = 0; i < units; i++) {
		if (buf[i * 2 + 1] == 0)
			zeroHigh++;
	}
	return zeroHigh * 4 >= units * 3;
#endif
}

static RDEncoding rd_detect_encoding(const UINT8* buf, size_t len)
{
	// BOM first, in Notepad++ determineEncoding() order.
	if (len >= 2 && buf[0] == 0xfe && buf[1] == 0xff)
		return RD_ENC_UTF16_BE;
	if (len >= 2 && buf[0] == 0xff && buf[1] == 0xfe)
		return RD_ENC_UTF16_LE;
	if (len >= 3 && buf[0] == 0xef && buf[1] == 0xbb && buf[2] == 0xbf)
		return RD_ENC_UTF8_BOM;
	if (rd_is_utf16le_nobom(buf, len))
		return RD_ENC_UTF16_LE_NOBOM;

	// No BOM: UTF-8 if the byte sequence validates, else ANSI.
	return rd_is_valid_utf8(buf, len) ? RD_ENC_UTF8 : RD_ENC_ANSI;
}

// ANSI (system code page, e.g. GBK) -> malloc'd UTF-8.  Win32 only.
#if defined(BUILD_WIN32) || defined(_WIN32)
static char* rd_ansi_to_utf8(const char* s, size_t len)
{
	if (!s)
		return NULL;

	INT32 wn = MultiByteToWideChar(CP_ACP, 0, s, (INT32)len, NULL, 0);
	if (wn <= 0)
		return NULL;

	wchar_t* w = (wchar_t*)malloc((size_t)(wn + 1) * sizeof(wchar_t));
	if (!w)
		return NULL;

	MultiByteToWideChar(CP_ACP, 0, s, (INT32)len, w, wn);
	w[wn] = 0;
	INT32 un = WideCharToMultiByte(CP_UTF8, 0, w, wn, NULL, 0, NULL, NULL);
	if (un <= 0) {
		free_s((void**)&w);
		return NULL;
	}
	char* u = (char*)malloc((size_t)un + 1);
	if (!u) {
		free_s((void**)&w);
		return NULL;
	}
	WideCharToMultiByte(CP_UTF8, 0, w, wn, u, un, NULL, NULL);
	u[un] = 0;
	free_s((void**)&w);
	return u;
}
#endif

static char* rd_load_text(const TCHAR* szPath)
{
	FILE* fp = _tfopen(szPath, _T("rb"));
	if (!fp)
		return NULL;

	size_t len = 0;
	UINT8* raw = rd_read_all(fp, &len);
	fclose(fp);
	if (!raw)
		return NULL;

	RDEncoding enc = rd_detect_encoding(raw, len);

	if (enc == RD_ENC_UTF8) {
		return (char*)raw;
	}
	if (enc == RD_ENC_ANSI) {
#if defined(BUILD_WIN32) || defined(_WIN32)
		char* u = rd_ansi_to_utf8((const char*)raw, len);	// GBK/... -> UTF-8
		if (u) {
			free_s((void**)&raw);
			return u;
		}
#endif
		return (char*)raw;
	}
	if (enc == RD_ENC_UTF8_BOM) {
		memmove(raw, raw + 3, len - 3 + 1);					// drop BOM
		return (char*)raw;
	}

	// UTF-16 -> UTF-8.  Without a BOM there is nothing to skip (N++ m_nSkip == 0).
	size_t start = (enc == RD_ENC_UTF16_LE_NOBOM) ? 0 : 2;
	if (len < start || ((len - start) & 1)) {
		free_s((void**)&raw);
		return NULL;
	}
	size_t units = (len - start) / 2;
	if (units > (SIZE_MAX - 1) / 3) {
		free_s((void**)&raw);
		return NULL;
	}

	// worst case 3 bytes per unit + NUL (a surrogate pair is 2 units -> 4 bytes)
	char* out = (char*)malloc(units * 3 + 1);
	if (!out) {
		free_s((void**)&raw);
		return NULL;
	}
	size_t o = 0;
	for (size_t i = 0; i < units; i++) {
		UINT8 b0 = raw[start + i * 2 + 0];
		UINT8 b1 = raw[start + i * 2 + 1];
		UINT32 cp = (enc == RD_ENC_UTF16_BE) ? (UINT32)(b1 | (b0 << 8)) : (UINT32)(b0 | (b1 << 8));
		if (cp >= 0xd800 && cp < 0xdc00) {					// high surrogate: needs its low half
			if (++i >= units) {
				free_s((void**)&out);
				free_s((void**)&raw);
				return NULL;
			}
			b0 = raw[start + i * 2 + 0];
			b1 = raw[start + i * 2 + 1];
			UINT32 low = (enc == RD_ENC_UTF16_BE) ? (UINT32)(b1 | (b0 << 8)) : (UINT32)(b0 | (b1 << 8));
			if (low < 0xdc00 || low >= 0xe000) {
				free_s((void**)&out);
				free_s((void**)&raw);
				return NULL;
			}
			cp = 0x10000 + ((cp & 0x3ff) << 10) + (low & 0x3ff);
		} else if (cp >= 0xdc00 && cp < 0xe000) {			// lone low surrogate
			free_s((void**)&out);
			free_s((void**)&raw);
			return NULL;
		}
		if (cp < 0x80) {
			out[o++] = (char)cp;
		} else if (cp < 0x800) {
			out[o++] = (char)(0xc0 | ( cp >>  6));
			out[o++] = (char)(0x80 | ( cp        & 0x3f));
		} else if (cp < 0x10000) {
			out[o++] = (char)(0xe0 | ( cp >> 12));
			out[o++] = (char)(0x80 | ((cp >>  6) & 0x3f));
			out[o++] = (char)(0x80 | ( cp        & 0x3f));
		} else {
			out[o++] = (char)(0xf0 | ( cp >> 18));
			out[o++] = (char)(0x80 | ((cp >> 12) & 0x3f));
			out[o++] = (char)(0x80 | ((cp >>  6) & 0x3f));
			out[o++] = (char)(0x80 | ( cp        & 0x3f));
		}
	}
	out[o] = 0;
	free_s((void**)&raw);
	return out;
}

static const char* RD_DELIMS = " \t\r\n,%:|{}";

static char* rd_qtoken(char* s, char** ppSaved)
{
	char* p = s ? s : *ppSaved;
	if (!p)
		return NULL;

	p += strspn(p, RD_DELIMS);						// skip leading delimiters
	if (*p == '\0') {
		*ppSaved = NULL;
		return NULL;
	}

	char* token = p;
	if (*p == '"') {								// quoted field
		token = ++p;
		char* q = strchr(p, '"');
		if (q) {
			*q = '\0';
			p = q + 1;
		}
		else {
			p = strchr(p, '\0');
		}
	} else {
		p = strpbrk(p, RD_DELIMS);
	}

	if (p && *p != '\0') {
		*p = '\0';
		*ppSaved = p + 1;
	}
	else {
		*ppSaved = NULL;
	}
	return token;
}

// Parse a hex string to UINT32 with strict validation.
static bool rd_hex(const char* s, UINT32* out)
{
	if (rd_is_emptyA(s) || !out || s[0] == '-')
		return false;

	errno = 0;
	char* end = NULL;
	unsigned long v = strtoul(s, &end, 16);
	if (s == end || *end != '\0' || errno == ERANGE)
		return false;
#if ULONG_MAX > 0xffffffffUL
	// strtoul is wider than UINT32 on LP64, so reject values that would truncate.
	if (v > 0xffffffffUL)
		return false;
#endif

	*out = (UINT32)v;
	return true;
}

//  Pure parse layer:  .dat text  ->  header fields + BurnRomInfo[]
struct RomDataParsed {
	char   szShortName[SHORT_MAX];
	char   szDrvName[SHORT_MAX];
	char   szDate[DATE_MAX];
	char   szExtraRom[SHORT_MAX];
	char   szFullName[MAX_PATH];
	struct BurnRomInfo* pRomInfo;					// malloc'd table (owns each szName)
	UINT32 nRomInfoCount;
	UINT32 nRomInfoCapacity;
	INT32  nBaseIdx;								// DrvName resolved once, for '*' lines
	UINT64 nDatSize;
	UINT64 nDatWriteTime;
	UINT64 nBaseFingerprint;
	const TCHAR* szPath;							// diagnostics only
	INT32  nLine;									// diagnostics only
};

// Say so when the .dat asked for something we cannot honour, instead of dropping it silently.
static void rd_warn(const struct RomDataParsed* pp, const char* pszWhat, const char* pszDetail)
{
	bprintf(PRINT_ERROR, _T("RomData: %s(%d): %hs '%hs'\n"), pp->szPath ? pp->szPath : _T("?"), pp->nLine, pszWhat, pszDetail ? pszDetail : "");
}

static void rd_parsed_free(struct RomDataParsed* pp)
{
	if (!pp)
		return;

	if (pp->pRomInfo) {
		for (UINT32 i = 0; i < pp->nRomInfoCount; i++)
			free_s((void**)&pp->pRomInfo[i].szName);
		free_s((void**)&pp->pRomInfo);
	}
	pp->nRomInfoCount    = 0;
	pp->nRomInfoCapacity = 0;
}

static INT32 rd_reserve_roms(struct RomDataParsed* pp, UINT32 nRequired)
{
	if (!pp)
		return -3;
	if (nRequired <= pp->nRomInfoCapacity)
		return 0;

	UINT32 nCapacity = pp->nRomInfoCapacity ? pp->nRomInfoCapacity : 16;
	while (nCapacity < nRequired) {
		if (nCapacity > ~0U / 2) {
			nCapacity = nRequired;
			break;
		}
		nCapacity *= 2;
	}

	size_t nSize;
	if (!rd_multiply_size(nCapacity, sizeof(struct BurnRomInfo), &nSize))
		return -3;

	struct BurnRomInfo* pRomInfo = (struct BurnRomInfo*)realloc(pp->pRomInfo, nSize);
	if (!pRomInfo)
		return -3;

	memset(pRomInfo + pp->nRomInfoCapacity, 0, (size_t)(nCapacity - pp->nRomInfoCapacity) * sizeof(struct BurnRomInfo));
	pp->pRomInfo         = pRomInfo;
	pp->nRomInfoCapacity = nCapacity;
	return 0;
}

struct RDCacheBuffer {
	UINT8* pData;
	UINT32 nLength;
	UINT32 nCapacity;
};

struct RDCacheReader {
	const UINT8* pData;
	UINT32 nLength;
	UINT32 nOffset;
};

struct RDCacheEntry {
	TCHAR szPath[MAX_PATH];
	UINT64 nSize;
	UINT64 nWriteTime;
	UINT64 nBaseFingerprint;
	struct RomDataParsed Parsed;
};

struct RDCache {
	struct RDCacheEntry* pEntries;
	UINT32 nCount;
	UINT32 nCapacity;
};

static void rd_cache_entry_free(struct RDCacheEntry* pEntry)
{
	if (!pEntry)
		return;

	rd_parsed_free(&pEntry->Parsed);
	memset(pEntry, 0, sizeof(*pEntry));
}

static void rd_cache_free(struct RDCache* pCache)
{
	if (!pCache)
		return;

	for (UINT32 i = 0; i < pCache->nCount; i++) rd_cache_entry_free(&pCache->pEntries[i]);
	free_s((void**)&pCache->pEntries);
	pCache->nCount    = 0;
	pCache->nCapacity = 0;
}

static INT32 rd_cache_reserve_entries(struct RDCache* pCache, UINT32 nRequired)
{
	if (!pCache || nRequired > RD_CACHE_MAX_ENTRIES)
		return 0;
	if (nRequired <= pCache->nCapacity)
		return 1;

	UINT32 nCapacity = pCache->nCapacity ? pCache->nCapacity : 32;
	while (nCapacity < nRequired) {
		if (nCapacity > RD_CACHE_MAX_ENTRIES / 2) {
			nCapacity = RD_CACHE_MAX_ENTRIES;
			break;
		}
		nCapacity *= 2;
	}

	size_t nSize;
	if (!rd_multiply_size(nCapacity, sizeof(struct RDCacheEntry), &nSize))
		return 0;

	struct RDCacheEntry* pEntries = (struct RDCacheEntry*)realloc(pCache->pEntries, nSize);
	if (!pEntries)
		return 0;

	memset(pEntries + pCache->nCapacity, 0, (size_t)(nCapacity - pCache->nCapacity) * sizeof(struct RDCacheEntry));
	pCache->pEntries = pEntries;
	pCache->nCapacity = nCapacity;
	return 1;
}

static INT32 rd_cache_add_move(struct RDCache* pCache, struct RDCacheEntry* pEntry)
{
	if (!pCache || !pEntry || !rd_cache_reserve_entries(pCache, pCache->nCount + 1))
		return 0;

	pCache->pEntries[pCache->nCount++] = *pEntry;
	memset(pEntry, 0, sizeof(*pEntry));
	return 1;
}

static INT32 rd_parsed_copy(struct RomDataParsed* pDest, const struct RomDataParsed* pSource)
{
	if (!pDest || !pSource)
		return 0;

	memset(pDest, 0, sizeof(*pDest));
	memcpy(pDest->szShortName, pSource->szShortName, sizeof(pDest->szShortName));
	memcpy(pDest->szDrvName,   pSource->szDrvName,   sizeof(pDest->szDrvName));
	memcpy(pDest->szDate,      pSource->szDate,      sizeof(pDest->szDate));
	memcpy(pDest->szExtraRom,  pSource->szExtraRom,  sizeof(pDest->szExtraRom));
	memcpy(pDest->szFullName,  pSource->szFullName,  sizeof(pDest->szFullName));
	pDest->nBaseIdx          = pSource->nBaseIdx;
	pDest->nDatSize          = pSource->nDatSize;
	pDest->nDatWriteTime     = pSource->nDatWriteTime;
	pDest->nBaseFingerprint  = pSource->nBaseFingerprint;
	pDest->szPath            = pSource->szPath;

	if (rd_reserve_roms(pDest, pSource->nRomInfoCount))
		return 0;

	for (UINT32 i = 0; i < pSource->nRomInfoCount; i++) {
		pDest->pRomInfo[i] = pSource->pRomInfo[i];
		pDest->pRomInfo[i].szName = rd_strdup(pSource->pRomInfo[i].szName);
		if (!pDest->pRomInfo[i].szName) {
			rd_parsed_free(pDest);
			return 0;
		}
		pDest->nRomInfoCount++;
	}
	return 1;
}

static struct RDCacheEntry* rd_cache_find(struct RDCache* pCache, const TCHAR* szPath)
{
	if (!pCache || !szPath)
		return NULL;

	for (UINT32 i = 0; i < pCache->nCount; i++) {
		if (_tcsicmp(pCache->pEntries[i].szPath, szPath) == 0)
			return &pCache->pEntries[i];
	}
	return NULL;
}

static INT32 rd_get_stamp(const TCHAR* szPath, UINT64* pSize, UINT64* pWriteTime)
{
	if (!szPath || !pSize || !pWriteTime)
		return 0;

#if defined(BUILD_WIN32) || defined(_WIN32)
	WIN32_FILE_ATTRIBUTE_DATA Data;
	if (!GetFileAttributesEx(szPath, GetFileExInfoStandard, &Data) || (Data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		return 0;

	*pSize = ((UINT64)Data.nFileSizeHigh << 32) | Data.nFileSizeLow;
	*pWriteTime = ((UINT64)Data.ftLastWriteTime.dwHighDateTime << 32) | Data.ftLastWriteTime.dwLowDateTime;
#else
	struct stat st;
	if (stat(szPath, &st) != 0 || !S_ISREG(st.st_mode))
		return 0;

	*pSize = (UINT64)st.st_size;
	*pWriteTime = (UINT64)st.st_mtime;
#endif
	return 1;
}

static UINT64 rd_hash_bytes(UINT64 nHash, const void* pData, size_t nLength)
{
	const UINT8* p = (const UINT8*)pData;
	for (size_t i = 0; i < nLength; i++) {
		nHash ^= p[i];
		nHash *= 0x100000001b3ULL;
	}
	return nHash;
}

static UINT64 rd_base_fingerprint(INT32 nBaseIdx, UINT32 nCount)
{
	if (nBaseIdx < 0 || (UINT32)nBaseIdx >= nBurnDrvCount)
		return 0;

	const UINT32 nOldDrvSel = nBurnDrvActive;
	nBurnDrvActive = (UINT32)nBaseIdx;
	UINT64 nHash = 0xcbf29ce484222325ULL;
	char* szBaseName = BurnDrvGetTextA(DRV_NAME);
	if (szBaseName)
		nHash = rd_hash_bytes(nHash, szBaseName, strlen(szBaseName) + 1);

	for (UINT32 i = 0; i < nCount; i++) {
		struct BurnRomInfo ri;
		char* szName = NULL;
		memset(&ri, 0, sizeof(ri));
		INT32 nInfo = BurnDrvGetRomInfo(&ri, i);
		INT32 nName = BurnDrvGetRomName(&szName, i, 0);
		nHash = rd_hash_bytes(nHash, &nInfo, sizeof(nInfo));
		nHash = rd_hash_bytes(nHash, &nName, sizeof(nName));
		if (!nInfo) {
			nHash = rd_hash_bytes(nHash, &ri.nLen,  sizeof(ri.nLen));
			nHash = rd_hash_bytes(nHash, &ri.nCrc,  sizeof(ri.nCrc));
			nHash = rd_hash_bytes(nHash, &ri.nType, sizeof(ri.nType));
		}
		if (!nName && szName)
			nHash = rd_hash_bytes(nHash, szName, strlen(szName) + 1);
	}
	nBurnDrvActive = nOldDrvSel;
	return nHash;
}

static char* rd_tchar_to_utf8(const TCHAR* szText)
{
	if (!szText)
		return rd_strdup("");

#ifdef _UNICODE
	INT32 nLength = WideCharToMultiByte(CP_UTF8, 0, szText, -1, NULL, 0, NULL, NULL);
	if (nLength <= 0)
		return NULL;

	char* szUtf8 = (char*)malloc((size_t)nLength);
	if (!szUtf8)
		return NULL;

	if (!WideCharToMultiByte(CP_UTF8, 0, szText, -1, szUtf8, nLength, NULL, NULL)) {
		free_s((void**)&szUtf8);
		return NULL;
	}
	return szUtf8;
#else
#if defined(BUILD_WIN32) || defined(_WIN32)
	INT32 nWide = MultiByteToWideChar(CP_ACP, 0, szText, -1, NULL, 0);
	if (nWide <= 0)
		return NULL;

	wchar_t* szWide = (wchar_t*)malloc((size_t)nWide * sizeof(wchar_t));
	if (!szWide)
		return NULL;

	MultiByteToWideChar(CP_ACP, 0, szText, -1, szWide, nWide);
	INT32 nLength = WideCharToMultiByte(CP_UTF8, 0, szWide, -1, NULL, 0, NULL, NULL);
	char* szUtf8 = nLength > 0 ? (char*)malloc((size_t)nLength) : NULL;
	if (szUtf8)
		WideCharToMultiByte(CP_UTF8, 0, szWide, -1, szUtf8, nLength, NULL, NULL);

	free_s((void**)&szWide);
	return szUtf8;
#else
	return rd_strdup(szText);
#endif
#endif
}

static TCHAR* rd_tchar_from_utf8(const char* szUtf8)
{
	if (!szUtf8)
		return NULL;

#ifdef _UNICODE
	INT32 nLength = MultiByteToWideChar(CP_UTF8, 0, szUtf8, -1, NULL, 0);
	if (nLength <= 0)
		return NULL;

	wchar_t* szText = (wchar_t*)malloc((size_t)nLength * sizeof(wchar_t));
	if (!szText)
		return NULL;

	if (!MultiByteToWideChar(CP_UTF8, 0, szUtf8, -1, szText, nLength)) {
		free_s((void**)&szText);
		return NULL;
	}
	return szText;
#else
#if defined(BUILD_WIN32) || defined(_WIN32)
	INT32 nWide = MultiByteToWideChar(CP_UTF8, 0, szUtf8, -1, NULL, 0);
	if (nWide <= 0)
		return NULL;

	wchar_t* szWide = (wchar_t*)malloc((size_t)nWide * sizeof(wchar_t));
	if (!szWide)
		return NULL;

	MultiByteToWideChar(CP_UTF8, 0, szUtf8, -1, szWide, nWide);
	INT32 nLength = WideCharToMultiByte(CP_ACP, 0, szWide, -1, NULL, 0, NULL, NULL);
	char* szText = nLength > 0 ? (char*)malloc((size_t)nLength) : NULL;
	if (szText)
		WideCharToMultiByte(CP_ACP, 0, szWide, -1, szText, nLength, NULL, NULL);

	free_s((void**)&szWide);
	return szText;
#else
	return (TCHAR*)rd_strdup(szUtf8);
#endif
#endif
}

static INT32 rd_cache_buffer_reserve(struct RDCacheBuffer* pBuffer, UINT32 nAdditional)
{
	if (!pBuffer || nAdditional > RD_CACHE_MAX_PAYLOAD - pBuffer->nLength)
		return 0;

	UINT32 nRequired = pBuffer->nLength + nAdditional;
	if (nRequired <= pBuffer->nCapacity)
		return 1;

	UINT32 nCapacity = pBuffer->nCapacity ? pBuffer->nCapacity : 4096;
	while (nCapacity < nRequired) {
		if (nCapacity > RD_CACHE_MAX_PAYLOAD / 2) {
			nCapacity = RD_CACHE_MAX_PAYLOAD;
			break;
		}
		nCapacity *= 2;
	}
	UINT8* pData = (UINT8*)realloc(pBuffer->pData, nCapacity);
	if (!pData)
		return 0;

	pBuffer->pData     = pData;
	pBuffer->nCapacity = nCapacity;
	return 1;
}

static INT32 rd_cache_buffer_bytes(struct RDCacheBuffer* pBuffer, const void* pData, UINT32 nLength)
{
	if (!rd_cache_buffer_reserve(pBuffer, nLength))
		return 0;

	if (nLength)
		memcpy(pBuffer->pData + pBuffer->nLength, pData, nLength);
	pBuffer->nLength += nLength;
	return 1;
}

static INT32 rd_cache_buffer_u32(struct RDCacheBuffer* pBuffer, UINT32 nValue)
{
	UINT8 Data[4] = { (UINT8)nValue, (UINT8)(nValue >> 8), (UINT8)(nValue >> 16), (UINT8)(nValue >> 24) };
	return rd_cache_buffer_bytes(pBuffer, Data, sizeof(Data));
}

static INT32 rd_cache_buffer_u64(struct RDCacheBuffer* pBuffer, UINT64 nValue)
{
	UINT8 Data[8];
	for (INT32 i = 0; i < 8; i++)
		Data[i] = (UINT8)(nValue >> (i * 8));

	return rd_cache_buffer_bytes(pBuffer, Data, sizeof(Data));
}

static INT32 rd_cache_buffer_stringA(struct RDCacheBuffer* pBuffer, const char* szText)
{
	size_t nLength = strlen(szText ? szText : "");
	return nLength <= 0x7fffffff && rd_cache_buffer_u32(pBuffer, (UINT32)nLength) && rd_cache_buffer_bytes(pBuffer, szText ? szText : "", (UINT32)nLength);
}

static INT32 rd_cache_buffer_stringT(struct RDCacheBuffer* pBuffer, const TCHAR* szText)
{
	char* szUtf8 = rd_tchar_to_utf8(szText);
	if (!szUtf8)
		return 0;

	INT32 nResult = rd_cache_buffer_stringA(pBuffer, szUtf8);
	free_s((void**)&szUtf8);
	return nResult;
}

static INT32 rd_cache_reader_bytes(struct RDCacheReader* pReader, void* pDest, UINT32 nLength)
{
	if (!pReader || pReader->nOffset > pReader->nLength || nLength > pReader->nLength - pReader->nOffset)
		return 0;

	if (pDest && nLength)
		memcpy(pDest, pReader->pData + pReader->nOffset, nLength);

	pReader->nOffset += nLength;
	return 1;
}

static INT32 rd_cache_reader_u32(struct RDCacheReader* pReader, UINT32* pValue)
{
	UINT8 Data[4];
	if (!pValue || !rd_cache_reader_bytes(pReader, Data, sizeof(Data)))
		return 0;

	*pValue = (UINT32)Data[0] | ((UINT32)Data[1] << 8) | ((UINT32)Data[2] << 16) | ((UINT32)Data[3] << 24);
	return 1;
}

static INT32 rd_cache_reader_u64(struct RDCacheReader* pReader, UINT64* pValue)
{
	UINT8 Data[8];
	if (!pValue || !rd_cache_reader_bytes(pReader, Data, sizeof(Data)))
		return 0;

	*pValue = 0;
	for (INT32 i = 0; i < 8; i++)
		*pValue |= (UINT64)Data[i] << (i * 8);

	return 1;
}

static INT32 rd_cache_reader_stringA(struct RDCacheReader* pReader, char* szDest, UINT32 nDestCount)
{
	UINT32 nLength;
	if (!szDest || !nDestCount || !rd_cache_reader_u32(pReader, &nLength) || nLength >= nDestCount || pReader->nOffset > pReader->nLength || nLength > pReader->nLength - pReader->nOffset)
		return 0;
	if (!rd_cache_reader_bytes(pReader, szDest, nLength))
		return 0;

	szDest[nLength] = '\0';
	return 1;
}

static INT32 rd_cache_reader_stringT(struct RDCacheReader* pReader, TCHAR* szDest, UINT32 nDestCount)
{
	char szUtf8[MAX_PATH * 4];
	if (!rd_cache_reader_stringA(pReader, szUtf8, sizeof(szUtf8)))
		return 0;

	TCHAR* szText = rd_tchar_from_utf8(szUtf8);
	if (!szText)
		return 0;

	INT32 nResult = _tcslen(szText) < nDestCount;
	if (nResult) {
		_tcsncpy(szDest, szText, nDestCount - 1);
		szDest[nDestCount - 1] = _T('\0');
	}
	free_s((void**)&szText);
	return nResult;
}

static UINT32 rd_cache_crc32(const UINT8* pData, UINT32 nLength)
{
	UINT32 nCrc = 0xffffffff;
	for (UINT32 i = 0; i < nLength; i++) {
		nCrc ^= pData[i];
		for (INT32 bit = 0; bit < 8; bit++)
			nCrc = (nCrc >> 1) ^ (0xedb88320 & (0 - (nCrc & 1)));
	}
	return nCrc ^ 0xffffffff;
}

static INT32 rd_cache_serialize_entry(struct RDCacheBuffer* pBuffer, const struct RDCacheEntry* pEntry)
{
	const struct RomDataParsed* pp = &pEntry->Parsed;
	if (!rd_cache_buffer_stringT(pBuffer, pEntry->szPath)       ||
		!rd_cache_buffer_u64(pBuffer, pEntry->nSize)            ||
		!rd_cache_buffer_u64(pBuffer, pEntry->nWriteTime)       ||
		!rd_cache_buffer_u64(pBuffer, pEntry->nBaseFingerprint) ||
		!rd_cache_buffer_stringA(pBuffer, pp->szShortName)      ||
		!rd_cache_buffer_stringA(pBuffer, pp->szDrvName)        ||
		!rd_cache_buffer_stringA(pBuffer, pp->szDate)           ||
		!rd_cache_buffer_stringA(pBuffer, pp->szExtraRom)       ||
		!rd_cache_buffer_stringA(pBuffer, pp->szFullName)       ||
		!rd_cache_buffer_u32(pBuffer, pp->nRomInfoCount))
		return 0;

	for (UINT32 i = 0; i < pp->nRomInfoCount; i++) {
		const struct BurnRomInfo* ri = &pp->pRomInfo[i];
		if (!rd_cache_buffer_stringA(pBuffer, ri->szName) || !rd_cache_buffer_u32(pBuffer, ri->nLen) ||
			!rd_cache_buffer_u32(pBuffer, ri->nCrc) || !rd_cache_buffer_u32(pBuffer, ri->nType))
			return 0;
	}
	return 1;
}

static INT32 rd_cache_deserialize_entry(struct RDCacheReader* pReader, struct RDCacheEntry* pEntry)
{
	memset(pEntry, 0, sizeof(*pEntry));
	struct RomDataParsed* pp = &pEntry->Parsed;
	if (!rd_cache_reader_stringT(pReader, pEntry->szPath, MAX_PATH) || !rd_cache_reader_u64(pReader, &pEntry->nSize) ||
		!rd_cache_reader_u64(pReader, &pEntry->nWriteTime) || !rd_cache_reader_u64(pReader, &pEntry->nBaseFingerprint) ||
		!rd_cache_reader_stringA(pReader, pp->szShortName, sizeof(pp->szShortName)) ||
		!rd_cache_reader_stringA(pReader, pp->szDrvName,   sizeof(pp->szDrvName))   ||
		!rd_cache_reader_stringA(pReader, pp->szDate,      sizeof(pp->szDate))      ||
		!rd_cache_reader_stringA(pReader, pp->szExtraRom,  sizeof(pp->szExtraRom))  ||
		!rd_cache_reader_stringA(pReader, pp->szFullName,  sizeof(pp->szFullName))  ||
		!rd_cache_reader_u32(pReader, &pp->nRomInfoCount) || !pp->nRomInfoCount || pp->nRomInfoCount > RD_CACHE_MAX_ROMS)
		return 0;

	UINT32 nCount = pp->nRomInfoCount;
	pp->nRomInfoCount = 0;
	if (rd_reserve_roms(pp, nCount))
		return 0;

	for (UINT32 i = 0; i < nCount; i++) {
		char szName[MAX_PATH];
		struct BurnRomInfo* ri = &pp->pRomInfo[i];
		if (!rd_cache_reader_stringA(pReader, szName, sizeof(szName)) ||
			!rd_cache_reader_u32(pReader, &ri->nLen) ||
			!rd_cache_reader_u32(pReader, &ri->nCrc) ||
			!rd_cache_reader_u32(pReader, &ri->nType))
			return 0;

		ri->szName = rd_strdup(szName);
		if (!ri->szName)
			return 0;

		pp->nRomInfoCount++;
	}
	return 1;
}

static INT32 rd_cache_path(const TCHAR* szRequestedPath, TCHAR* szPath)
{
	if (rd_is_empty(szRequestedPath) || _tcslen(szRequestedPath) >= MAX_PATH)
		return 0;

	_tcsncpy(szPath, szRequestedPath, MAX_PATH - 1);
	szPath[MAX_PATH - 1] = _T('\0');
	return 1;
}

static INT32 rd_cache_load(const TCHAR* szCachePath, const TCHAR* szRoot, struct RDCache* pCache)
{
	FILE* fp = _tfopen(szCachePath, _T("rb"));
	if (!fp)
		return 0;

	size_t nFileLengthSize;
	UINT8* pFile = rd_read_all(fp, &nFileLengthSize);
	fclose(fp);

	if (!pFile || nFileLengthSize < 40 || nFileLengthSize > RD_CACHE_MAX_PAYLOAD + 40) {
		free_s((void**)&pFile);
		return 0;
	}

	UINT32 nFileLength = (UINT32)nFileLengthSize;
	struct RDCacheReader File = { pFile, nFileLength, 0 };
	UINT8  Magic[8], Trailer[8];
	UINT32 nSchema, nIdentify, nEntryCount, nPayloadLength, nCrc, nReserved;
	INT32  nHeader = rd_cache_reader_bytes(&File, Magic, 8) && rd_cache_reader_u32(&File, &nSchema) &&
		rd_cache_reader_u32(&File, &nIdentify)      &&
		rd_cache_reader_u32(&File, &nEntryCount)    &&
		rd_cache_reader_u32(&File, &nPayloadLength) &&
		rd_cache_reader_u32(&File, &nCrc)           &&
		rd_cache_reader_u32(&File, &nReserved);
	if (!nHeader || memcmp(Magic, RDCacheMagic, 8) || nSchema != RD_CACHE_SCHEMA_VERSION ||
		nIdentify != RD_CACHE_IDENTIFY_VERSION || nReserved != 0 || nEntryCount > RD_CACHE_MAX_ENTRIES || nPayloadLength > RD_CACHE_MAX_PAYLOAD ||
		nPayloadLength != nFileLength - 40 || rd_cache_crc32(pFile + 32, nPayloadLength) != nCrc ||
		!rd_cache_reader_bytes(&File, NULL, nPayloadLength) || !rd_cache_reader_bytes(&File, Trailer, 8) || memcmp(Trailer, RDCacheTrailer, 8)) {
		free_s((void**)&pFile);
		return 0;
	}

	struct RDCacheReader Payload = { pFile + 32, nPayloadLength, 0 };
	TCHAR szStoredRoot[MAX_PATH];
	if (!rd_cache_reader_stringT(&Payload, szStoredRoot, MAX_PATH) ||
#if defined(BUILD_WIN32) || defined(_WIN32)
		_tcsicmp(szStoredRoot, szRoot)
#else
		_tcscmp(szStoredRoot, szRoot)
#endif
	) {
		free_s((void**)&pFile);
		return 0;
	}

	for (UINT32 i = 0; i < nEntryCount; i++) {
		struct RDCacheEntry Entry;
		if (!rd_cache_deserialize_entry(&Payload, &Entry) || !rd_cache_add_move(pCache, &Entry)) {
			rd_cache_entry_free(&Entry);
			rd_cache_free(pCache);
			free_s((void**)&pFile);
			return 0;
		}
	}

	INT32 nResult = Payload.nOffset == Payload.nLength;
	if (!nResult)
		rd_cache_free(pCache);

	free_s((void**)&pFile);
	return nResult;
}

static INT32 rd_cache_save(const TCHAR* szCachePath, const TCHAR* szRoot, const struct RDCache* pCache)
{
	struct RDCacheBuffer Payload = { NULL, 0, 0 };
	if (!rd_cache_buffer_stringT(&Payload, szRoot))
		return 0;

	for (UINT32 i = 0; i < pCache->nCount; i++) {
		if (!rd_cache_serialize_entry(&Payload, &pCache->pEntries[i])) {
			free_s((void**)&Payload.pData);
			return 0;
		}
	}

	struct RDCacheBuffer File = { NULL, 0, 0 };
	UINT32 nCrc = rd_cache_crc32(Payload.pData, Payload.nLength);
	INT32 nBuilt = rd_cache_buffer_bytes(&File, RDCacheMagic, 8) &&
		rd_cache_buffer_u32(&File, RD_CACHE_SCHEMA_VERSION)   &&
		rd_cache_buffer_u32(&File, RD_CACHE_IDENTIFY_VERSION) &&
		rd_cache_buffer_u32(&File, pCache->nCount)            &&
		rd_cache_buffer_u32(&File, Payload.nLength)           &&
		rd_cache_buffer_u32(&File, nCrc)                      &&
		rd_cache_buffer_u32(&File, 0)                         &&
		rd_cache_buffer_bytes(&File, Payload.pData, Payload.nLength) &&
		rd_cache_buffer_bytes(&File, RDCacheTrailer, 8);
	free_s((void**)&Payload.pData);

	if (!nBuilt) {
		free_s((void**)&File.pData);
		return 0;
	}

	TCHAR szTemp[MAX_PATH];
	INT32 nLength = _sntprintf(szTemp, MAX_PATH, _T("%s.tmp"), szCachePath);
	if (nLength < 0 || nLength >= MAX_PATH) {
		free_s((void**)&File.pData);
		return 0;
	}

	FILE* fp = _tfopen(szTemp, _T("wb"));
	if (!fp) {
		free_s((void**)&File.pData);
		return 0;
	}

	INT32 nWritten = fwrite(File.pData, 1, File.nLength, fp) == File.nLength && fflush(fp) == 0;
#if defined(BUILD_WIN32) || defined(_WIN32)
	if (nWritten) {
		intptr_t nHandle = _get_osfhandle(_fileno(fp));
		nWritten = nHandle != -1 && FlushFileBuffers((HANDLE)nHandle);
	}
#else
	if (nWritten)
		nWritten = fsync(fileno(fp)) == 0;
#endif
	free_s((void**)&File.pData);
	if (fclose(fp))
		nWritten = 0;
#if defined(BUILD_WIN32) || defined(_WIN32)
	if (!nWritten || !MoveFileEx(szTemp, szCachePath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		DeleteFile(szTemp);
		return 0;
	}
#else
	if (!nWritten || rename(szTemp, szCachePath) != 0) {
		remove(szTemp);
		return 0;
	}
#endif
	return 1;
}

// Copy the base driver's ROM entry at the current index into the parsed table.
static INT32 rd_copy_base_entry(struct RomDataParsed* pp)
{
	if (pp->nBaseIdx < 0)
		return -1;											// '*' before DrvName

	struct BurnRomInfo ri;
	char* pszName = NULL;
	const UINT32 nOldDrvSel = nBurnDrvActive;
	nBurnDrvActive = (UINT32)pp->nBaseIdx;

	memset(&ri, 0, sizeof(ri));
	INT32 rInfo = BurnDrvGetRomInfo(&ri, pp->nRomInfoCount);
	INT32 rName = BurnDrvGetRomName(&pszName, pp->nRomInfoCount, 0);
	nBurnDrvActive = nOldDrvSel;

	if (rInfo || rName || !pszName || !pszName[0]) {		// past the base table: nothing to inherit
		rd_warn(pp, "'*' has no entry to inherit from", pp->szDrvName);
		return 1;
	}

	if (rd_reserve_roms(pp, pp->nRomInfoCount + 1))
		return -3;

	struct BurnRomInfo* dst = &pp->pRomInfo[pp->nRomInfoCount];
	memset(dst, 0, sizeof(*dst));
	dst->szName = rd_strdup(pszName);
	if (!dst->szName)
		return -3;

	dst->nLen = ri.nLen; dst->nCrc = ri.nCrc; dst->nType = ri.nType;
	pp->nRomInfoCount++;
	return 0;
}

// ---------------------------------------------------------------------------
//  Symbolic ROM-type lookup table  (populated from the platform headers)
// ---------------------------------------------------------------------------
struct RDMacroMap {
	const char* pszName;
	UINT32 nValue;
};

#define X(a) { #a, (UINT32)(a) }
static const struct RDMacroMap RDMacroTable[] = {
	X(BRF_PRG), X(BRF_GRA), X(BRF_SND), X(BRF_ESS), X(BRF_BIOS), X(BRF_SELECT), X(BRF_OPT), X(BRF_NODUMP),
	X(CPS1_68K_PROGRAM_BYTESWAP), X(CPS1_68K_PROGRAM_NO_BYTESWAP), X(CPS1_Z80_PROGRAM), X(CPS1_TILES),
	X(CPS1_OKIM6295_SAMPLES), X(CPS1_QSOUND_SAMPLES), X(CPS1_PIC),
	X(CPS1_EXTRA_TILES_SF2EBBL_400000), X(CPS1_EXTRA_TILES_400000), X(CPS1_EXTRA_TILES_SF2KORYU_400000),
	X(CPS1_EXTRA_TILES_SF2B_400000), X(CPS1_EXTRA_TILES_SF2MKOT_400000),
	X(CPS2_PRG_68K), X(CPS2_PRG_68K_SIMM), X(CPS2_PRG_68K_XOR_TABLE), X(CPS2_GFX), X(CPS2_GFX_SIMM),
	X(CPS2_GFX_SPLIT4), X(CPS2_GFX_SPLIT8), X(CPS2_GFX_19XXJ), X(CPS2_PRG_Z80), X(CPS2_QSND),
	X(CPS2_QSND_SIMM), X(CPS2_QSND_SIMM_BYTESWAP), X(CPS2_ENCRYPTION_KEY),
	X(GAL_ROM_Z80_PROG1), X(GAL_ROM_Z80_PROG2), X(GAL_ROM_Z80_PROG3), X(GAL_ROM_TILES_SHARED),
	X(GAL_ROM_TILES_CHARS), X(GAL_ROM_TILES_SPRITES), X(GAL_ROM_PROM), X(GAL_ROM_S2650_PROG1),
	X(SEGA_MD_ROM_LOAD_NORMAL), X(SEGA_MD_ROM_LOAD16_WORD_SWAP), X(SEGA_MD_ROM_LOAD16_BYTE),
	X(SEGA_MD_ROM_LOAD16_WORD_SWAP_CONTINUE_040000_100000), X(SEGA_MD_ROM_LOAD_NORMAL_CONTINUE_020000_080000),
	X(SEGA_MD_ROM_OFFS_000000), X(SEGA_MD_ROM_OFFS_000001), X(SEGA_MD_ROM_OFFS_020000), X(SEGA_MD_ROM_OFFS_080000),
	X(SEGA_MD_ROM_OFFS_100000), X(SEGA_MD_ROM_OFFS_100001), X(SEGA_MD_ROM_OFFS_200000), X(SEGA_MD_ROM_OFFS_300000),
	X(SEGA_MD_ROM_RELOAD_200000_200000), X(SEGA_MD_ROM_RELOAD_100000_300000), X(SEGA_MD_ARCADE_SUNMIXING),
	X(SYS16_ROM_PROG_FLAT), X(SYS16_ROM_PROG), X(SYS16_ROM_TILES), X(SYS16_ROM_SPRITES), X(SYS16_ROM_Z80PROG),
	X(SYS16_ROM_KEY), X(SYS16_ROM_7751PROG), X(SYS16_ROM_7751DATA), X(SYS16_ROM_UPD7759DATA), X(SYS16_ROM_PROG2),
	X(SYS16_ROM_ROAD), X(SYS16_ROM_PCMDATA), X(SYS16_ROM_Z80PROG2), X(SYS16_ROM_Z80PROG3), X(SYS16_ROM_Z80PROG4),
	X(SYS16_ROM_PCM2DATA), X(SYS16_ROM_PROM), X(SYS16_ROM_PROG3), X(SYS16_ROM_SPRITES2), X(SYS16_ROM_RF5C68DATA),
	X(SYS16_ROM_I8751), X(SYS16_ROM_MSM6295), X(SYS16_ROM_TILES_20000),
	X(TAITO_68KROM1), X(TAITO_68KROM1_BYTESWAP), X(TAITO_68KROM1_BYTESWAP_JUMPING), X(TAITO_68KROM1_BYTESWAP32),
	X(TAITO_68KROM2), X(TAITO_68KROM2_BYTESWAP), X(TAITO_68KROM3), X(TAITO_68KROM3_BYTESWAP),
	X(TAITO_Z80ROM1), X(TAITO_Z80ROM2), X(TAITO_CHARS), X(TAITO_CHARS_BYTESWAP), X(TAITO_CHARSB),
	X(TAITO_CHARSB_BYTESWAP), X(TAITO_SPRITESA), X(TAITO_SPRITESA_BYTESWAP), X(TAITO_SPRITESA_BYTESWAP32),
	X(TAITO_SPRITESA_TOPSPEED), X(TAITO_SPRITESA_DBLAXLEU), X(TAITO_SPRITESB), X(TAITO_SPRITESB_BYTESWAP), X(TAITO_SPRITESB_BYTESWAP32),
	X(TAITO_ROAD), X(TAITO_SPRITEMAP), X(TAITO_YM2610A), X(TAITO_YM2610B), X(TAITO_MSM5205),
	X(TAITO_MSM5205_BYTESWAP), X(TAITO_CHARS_PIVOT), X(TAITO_MSM6295), X(TAITO_ES5505), X(TAITO_ES5505_BYTESWAP),
	X(TAITO_DEFAULT_EEPROM), X(TAITO_CHARS_BYTESWAP32), X(TAITO_CCHIP_BIOS), X(TAITO_CCHIP_EEPROM),
};
#undef X

static bool romdata_lookup_macro(const char* pszName, UINT32* pOut)
{
	if (!pszName || !pOut)
		return false;

	for (UINT32 i = 0; i < ARRAY_SIZE(RDMacroTable); i++) {
		if (strcmp(pszName, RDMacroTable[i].pszName) == 0) {
			*pOut = RDMacroTable[i].nValue;
			return true;
		}
	}
	return false;
}

static INT32 rd_parse_rom_entry(char* pName, char** ppSaved, struct RomDataParsed* pp)
{
	if (rd_is_emptyA(pName))
		return -1;

	struct BurnRomInfo ri;
	memset(&ri, 0, sizeof(ri));
	ri.nLen  = ~0U;
	ri.nCrc  = ~0U;
	ri.nType =  0;

	char* tok;
	tok = rd_qtoken(NULL, ppSaved);
	if (!tok || !rd_hex(tok, &ri.nLen) || ri.nLen == 0 || ri.nLen == ~0U)
		return -1;

	tok = rd_qtoken(NULL, ppSaved);
	if (!tok || !rd_hex(tok, &ri.nCrc) || ri.nCrc == ~0U)
		return -1;

	while ((tok = rd_qtoken(NULL, ppSaved)) != NULL) {
		if (tok[0] == '/' && tok[1] == '/')
			break;											// trailing comment
		UINT32 v;
		if (romdata_lookup_macro(tok, &v))
			ri.nType |= v;									// symbolic token
		else if (rd_hex(tok, &v) && v != ~0U)
			ri.nType |= v;									// numeric token
		else
			rd_warn(pp, "unknown ROM type, ignored", tok);	// a typo here silently mis-loads the ROM
	}

	if (ri.nType == 0)
		return -2;											// type must be explicit ("*" inherits instead)

	if (rd_reserve_roms(pp, pp->nRomInfoCount + 1))
		return -3;

	struct BurnRomInfo* dst = &pp->pRomInfo[pp->nRomInfoCount];
	memset(dst, 0, sizeof(*dst));
	dst->szName = rd_strdup(pName);
	if (!dst->szName)
		return -3;

	dst->nLen  = ri.nLen;
	dst->nCrc  = ri.nCrc;
	dst->nType = ri.nType;
	pp->nRomInfoCount++;
	return 0;
}

// Full parse.  Returns 0 on success; pp holds header + ROM table on success.
static INT32 rd_parse_text(char* text, const TCHAR* szPath, struct RomDataParsed* pp)
{
	memset(pp, 0, sizeof(*pp));
	pp->szPath   = szPath;
	pp->nBaseIdx = -1;

	bool bHaveZip = false, bHaveDrv = false, bHaveDate = false, bHaveFull = false, bHaveExtra = false;

	char* line;
	char* nextLine = text;

	// Iterate lines (handle \n; \r already tolerated by the token delimiters).
	while ((line = nextLine) != NULL && *line) {
		char* nl = strchr(line, '\n');
		if (nl) {
			*nl = '\0';
			nextLine = nl + 1;
		}
		else {
			nextLine = NULL;
		}
		pp->nLine++;

		char* saved = NULL;
		char* label = rd_qtoken(line, &saved);
		if (!label)
			continue;
		if ('/' == label[0] && '/' == label[1])
			continue;											// comment

		if (_stricmp("ShortName", label) == 0 ||
			_stricmp("ZipName",   label) == 0 ||
			_stricmp("RomName",   label) == 0) {
			if (bHaveZip) {
				rd_warn(pp, "duplicate label, ignored", label);
				continue;
			}
			char* v = rd_qtoken(NULL, &saved);
			if (rd_is_emptyA(v) || strlen(v) > SHORT_MAX - 1)
				return -1;

			strncpy(pp->szShortName, v, sizeof(pp->szShortName) - 1);
			bHaveZip = true;
			continue;
		}
		if (_stricmp("DrvName", label) == 0 ||
			_stricmp("Parent",  label) == 0) {
			if (bHaveDrv) {
				rd_warn(pp, "duplicate label, ignored", label);
				continue;
			}
			char* v = rd_qtoken(NULL, &saved);
			if (rd_is_emptyA(v) || strlen(v) > SHORT_MAX - 1)
				return -1;

			pp->nBaseIdx = BurnDrvGetIndex(v);
			if (pp->nBaseIdx < 0) {								// base driver not found
				rd_warn(pp, "base driver not found", v);
				return -1;
			}
			strncpy(pp->szDrvName, v, sizeof(pp->szDrvName) - 1);
			bHaveDrv = true;
			continue;
		}
		if (_stricmp("Date",    label) == 0 ||
			_stricmp("Release", label) == 0) {
			if (bHaveDate) {
				rd_warn(pp, "duplicate label, ignored", label);
				continue;
			}
			char* v = rd_qtoken(NULL, &saved);
			if (!rd_is_emptyA(v)) {
				if (strlen(v) >= sizeof(pp->szDate))
					rd_warn(pp, "over-long date, truncated", v);

				snprintf(pp->szDate, sizeof(pp->szDate), "%s", v);
			}
			bHaveDate = true;
			continue;
		}
		if (_stricmp("ExtraRom", label) == 0) {
			if (bHaveExtra) {
				rd_warn(pp, "duplicate label, ignored", label);
				continue;
			}
			char* v = rd_qtoken(NULL, &saved);
			if (!rd_is_emptyA(v)) {
				if (strlen(v) >= sizeof(pp->szExtraRom))
					rd_warn(pp, "over-long ExtraRom, truncated", v);

				snprintf(pp->szExtraRom, sizeof(pp->szExtraRom), "%s", v);
			}
			bHaveExtra = true;
			continue;
		}
		if (_stricmp("FullName", label) == 0 ||
			_stricmp("Game",     label) == 0) {
			if (bHaveFull) {
				rd_warn(pp, "duplicate label, ignored", label);
				continue;
			}
			INT32 nAdd = 0; char* v;
			while ((v = rd_qtoken(NULL, &saved)) != NULL) {
				INT32 nRem = (INT32)sizeof(pp->szFullName) - nAdd - 1;
				if (nRem <= 0) {
					rd_warn(pp, "over-long FullName, truncated", v);
					break;
				}
				INT32 w = snprintf(pp->szFullName + nAdd, nRem, "%s ", v);
				if (w <= 0) break;
				nAdd = (INT32)strlen(pp->szFullName);
			}
			if (nAdd == 0)
				return -1;

			pp->szFullName[nAdd - 1] = '\0';					// strip trailing space
			bHaveFull = true;
			continue;
		}

		INT32 r;
		if (strcmp(label, "*") == 0)
			r = rd_copy_base_entry(pp);							// 0=copied, 1=skip, <0=error
		else
			r = rd_parse_rom_entry(label, &saved, pp);
		if (r < 0)
			return r;
	}

	// Required: ZipName, DrvName, FullName, at least one ROM.
	if (!bHaveZip || !bHaveDrv || !bHaveFull || pp->nRomInfoCount == 0)
		return -1;

	return 0;
}

// Map nBurnDrvActive to a RomData record, or NULL when the active driver is not one of ours.
static inline struct RomDataDrv* rd_active(void)
{
	// Every record starts with its BurnDriver, and only this file sets the flag,
	// so the linked pointer is the record.  No index bookkeeping to go stale.
	struct BurnDriver* drv = BurnGetActiveDriver();
	if (!drv || !(drv->Flags & BDF_ROMDATA_DRIVER))
		return NULL;

	return (struct RomDataDrv*)drv;
}

// Run one of the base driver's table functions with the base driver selected.
#define RD_CALL_BASE(rd, expr)											\
	do {																\
		INT32 nBase = BurnDrvGetIndex((rd)->pszDrvName);				\
		if (nBase < 0)													\
			return 1;													\
		const UINT32 nOldDrvSel = nBurnDrvActive;						\
		nBurnDrvActive = (UINT32)nBase;									\
		INT32 nResult  = (expr);										\
		nBurnDrvActive = nOldDrvSel;									\
		return nResult;													\
	} while (0)

static char rd_szEmptyName[] = "";								// name of a padded empty slot

static INT32 RomDataGetRomInfo(struct BurnRomInfo* pri, UINT32 i)
{
	struct RomDataDrv* rd = rd_active();
	if (!rd)
		return 1;

	// STDROMPICKEXT puts the board/BIOS set at 0x80+; only the game ROMs below
	// that come from the .dat, so let the base driver answer for the rest.
	if (i >= RD_BOARDROM_BASE) {
		if (!rd->pBaseDriver || !rd->pBaseDriver->GetRomInfo)
			return 1;

		RD_CALL_BASE(rd, rd->pBaseDriver->GetRomInfo(pri, i));
	}
	// Pad the gap up to 0x80 with empty slots.  Loaders count ROMs by iterating
	// until GetRomInfo != 0, and the board/BIOS set at 0x80+ is only reachable
	// across that bridge; STDROMPICKEXT does the same with emptyRomDesc.
	if (i >= rd->nRomInfoCount) {
		if (pri)
			memset(pri, 0, sizeof(*pri));

		return 0;
	}
	if (pri) {
		pri->nLen  = rd->pRomInfo[i].nLen;
		pri->nCrc  = rd->pRomInfo[i].nCrc;
		pri->nType = rd->pRomInfo[i].nType;
	}
	return 0;
}

static INT32 RomDataGetRomName(char** pszName, UINT32 i, INT32 nAka)
{
	struct RomDataDrv* rd = rd_active();
	if (!rd)
		return 1;

	if (i >= RD_BOARDROM_BASE) {
		if (!rd->pBaseDriver || !rd->pBaseDriver->GetRomName)
			return 1;

		RD_CALL_BASE(rd, rd->pBaseDriver->GetRomName(pszName, i, nAka));
	}
	if (i >= rd->nRomInfoCount) {			// padded empty slot
		if (nAka)
			return 1;
		if (pszName)
			*pszName = rd_szEmptyName;

		return 0;
	}
	if (nAka)
		return 1;							// RomData has no alternate names
	if (pszName)
		*pszName = rd->pRomInfo[i].szName;

	return 0;
}

//  Build + link a RomData driver from parsed data.
static void rd_free_record(struct RomDataDrv* rec)
{
	if (!rec)
		return;

	if (rec->pRomInfo) {
		for (UINT32 i = 0; i < rec->nRomInfoCount; i++)
			free_s((void**)&rec->pRomInfo[i].szName);

		free_s((void**)&rec->pRomInfo);
	}
	free_s((void**)&rec->pszShortName);
	free_s((void**)&rec->pszDrvName);
	free_s((void**)&rec->pszDate);
	free_s((void**)&rec->pszFullNameA);
	free_s((void**)&rec->pszFullNameW);
	free_s((void**)&rec->pszExtName);
	free(rec);
}

#ifdef _UNICODE
static wchar_t* rd_utf8_to_wide(const char* s)
{
	if (!s)
		return NULL;
#if defined(BUILD_WIN32) || defined(_WIN32)
	INT32 wn = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
	INT32 cp = CP_UTF8;
	if (wn <= 0) {																// not valid UTF-8: system code page
		cp = CP_ACP;
		wn = MultiByteToWideChar(CP_ACP, 0, s, -1, NULL, 0);
		if (wn <= 0)
			return NULL;
	}
	wchar_t* w = (wchar_t*)malloc(((size_t)wn + 1) * sizeof(wchar_t));
	if (!w)
		return NULL;

	MultiByteToWideChar(cp, 0, s, -1, w, wn);
	w[wn] = 0;
	return w;
#else
	size_t n = strlen(s);
	wchar_t* w = (wchar_t*)malloc((n + 2) * sizeof(wchar_t));
	if (!w)
		return NULL;

	const UINT8* p = (const UINT8*)s;
	size_t o = 0;
	while (*p) {
		UINT32 cp;
		if (p[0] < 0x80) {
			cp = p[0];
			p += 1;
		}
		else if ((p[0] & 0xe0) == 0xc0 && p[1]) {
			cp = ((p[0] & 0x1f) << 6) | (p[1] & 0x3f);
			p += 2;
		}
		else if ((p[0] & 0xf0) == 0xe0 && p[1] && p[2]) {
			cp = ((p[0] & 0x0f) << 12) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f);
			p += 3;
		}
		else if ((p[0] & 0xf8) == 0xf0 && p[1] && p[2] && p[3]) {
			cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3f) << 12) | ((p[2] & 0x3f) << 6) | (p[3] & 0x3f);
			p += 4;
		}
		else {
			cp = p[0];
			p += 1;
		}
		w[o++] = (wchar_t)cp;
	}
	w[o + 0] = 0;
	w[o + 1] = 0;
	return w;
#endif
}
#endif // _UNICODE

// Returns new driver index (>= 0) on success, negative on failure.
static INT32 rd_build_and_link(struct RomDataParsed* pp)
{
	INT32 nBaseIdx = pp->nBaseIdx;					// resolved once at parse time
	if (nBaseIdx < 0) {
		bprintf(PRINT_ERROR, _T("RomData: base driver '%hs' not found\n"), pp->szDrvName);
		return -1;
	}

	struct BurnDriver* base = BurnGetDriver(pp->szDrvName);
	if (!base) {
		bprintf(PRINT_ERROR, _T("RomData: failed to look up base driver '%hs'\n"), pp->szDrvName);
		return -1;
	}

	struct RomDataDrv* rec = (struct RomDataDrv*)calloc(1, sizeof(struct RomDataDrv));
	if (!rec) {
		bprintf(PRINT_ERROR, _T("RomData: out of memory allocating record for '%hs'\n"), pp->szShortName);
		return -1;
	}

	// Shallow-copy the base driver: inherit every field / function pointer.
	memcpy(&rec->drv, base, sizeof(struct BurnDriver));

	// Take ownership of the parsed ROM table (moved, not copied).
	rec->pRomInfo      = pp->pRomInfo;
	rec->nRomInfoCount = pp->nRomInfoCount;
	rec->pBaseDriver   = base;
	pp->pRomInfo         = NULL;					// ownership transferred
	pp->nRomInfoCount    = 0;
	pp->nRomInfoCapacity = 0;

	// Overridden identity strings.
	rec->pszShortName =  rd_strdup(pp->szShortName);
	rec->pszDrvName   =  rd_strdup(pp->szDrvName);
	rec->pszDate      = !rd_is_emptyA(pp->szDate) ? rd_strdup(pp->szDate) : NULL;
	rec->pszFullNameA =  rd_strdup(pp->szFullName);
#ifdef _UNICODE
	rec->pszFullNameW = rd_utf8_to_wide(pp->szFullName);
#else
	rec->pszFullNameW = NULL;
#endif
	if (!rd_is_emptyA(pp->szExtraRom))
		rec->pszExtName = rd_strdup(pp->szExtraRom);

	if (!rec->pszShortName || !rec->pszDrvName || !rec->pszFullNameA
#ifdef _UNICODE
		|| !rec->pszFullNameW
#endif
	) {
		bprintf(PRINT_ERROR, _T("RomData: out of memory duplicating strings for '%hs'\n"), pp->szShortName);
		rd_free_record(rec);
		return -1;
	}

	rec->drv.szShortName = rec->pszShortName;
	rec->drv.szParent    = base->szParent ? base->szParent : rec->pszDrvName;
	rec->drv.szDate      = rec->pszDate   ? rec->pszDate   : base->szDate;
	rec->drv.szFullNameA = rec->pszFullNameA;
	rec->drv.szFullNameW = rec->pszFullNameW;
	rec->drv.GetRomInfo  = RomDataGetRomInfo;
	rec->drv.GetRomName  = RomDataGetRomName;
	rec->drv.Flags      |= BDF_ROMDATA_DRIVER | BDF_CLONE;

	// From the first record on, the engine owns our teardown: BurnLibExit calls
	// the hook while pDriverEx is still alive, so no frontend has to.
	BurnLibExitHook = RomDataFree;

	if (rd_reserve_records(nRDDrvCount + 1)) {
		bprintf(PRINT_ERROR, _T("RomData: out of memory growing driver array\n"));
		rd_free_record(rec);
		return -1;
	}

	// Register the record, then link the driver into the engine list.
	if (LinkExtlDrivers(&rec->drv, &nBurnDrvCount) == ~0U) {
		bprintf(PRINT_ERROR, _T("RomData: failed to link driver '%hs'\n"), pp->szShortName);
		rd_free_record(rec);
		return -1;
	}

	pRDDrv[nRDDrvCount] = rec;
	nRDDrvCount++;

	bprintf(PRINT_NORMAL, _T("RomData: loaded driver '%hs' (based on '%hs')\n"), rec->pszShortName, rec->pszDrvName);
	return (INT32)(nBurnDrvCount - 1);				// index of the just-added driver
}

// =============================================================================
//  Public API
// =============================================================================

extern "C" INT32 RomDataIdentify(const TCHAR* szDatPath, char* szShortName, INT32 nShortNameLen)
{
	if (rd_is_empty(szDatPath) || !rd_is_dat(szDatPath))
		return 0;

	char* text = rd_load_text(szDatPath);
	if (!text)
		return 0;

	struct RomDataParsed Parsed;
	INT32 nResult = rd_parse_text(text, szDatPath, &Parsed);
	free_s((void**)&text);
	if (nResult != 0) {
		rd_parsed_free(&Parsed);
		return 0;
	}

	if (szShortName && nShortNameLen > 0)
		snprintf(szShortName, nShortNameLen, "%s", Parsed.szShortName);
	rd_parsed_free(&Parsed);
	return 1;
}

static INT32 rd_load_parsed_text(const TCHAR* szDatPath, UINT64 nSize, UINT64 nWriteTime, struct RomDataParsed* pParsed)
{
	char* text = rd_load_text(szDatPath);
	if (!text) {
		bprintf(PRINT_ERROR, _T("RomData: failed to read/convert '%s'\n"), szDatPath);
		return -1;
	}

	INT32 nResult = rd_parse_text(text, szDatPath, pParsed);
	free_s((void**)&text);

	if (nResult != 0) {
		bprintf(PRINT_ERROR, _T("RomData: failed to parse '%s' (error %d)\n"), szDatPath, nResult);
		rd_parsed_free(pParsed);
		return -1;
	}
	pParsed->nDatSize         = nSize;
	pParsed->nDatWriteTime    = nWriteTime;
	pParsed->nBaseFingerprint = rd_base_fingerprint(pParsed->nBaseIdx, pParsed->nRomInfoCount);
	return 0;
}

static INT32 rd_build_parsed(struct RomDataParsed* pParsed)
{
	INT32 nIndex = BurnDrvGetIndex(pParsed->szShortName);
	if (nIndex >= 0) {
		struct BurnDriver* pDriver = BurnGetDriver(pParsed->szShortName);
		if (!pDriver || !(pDriver->Flags & BDF_ROMDATA_DRIVER)) {
			bprintf(PRINT_ERROR, _T("RomData: driver '%hs' already exists\n"), pParsed->szShortName);
			return -1;
		}
		bprintf(PRINT_IMPORTANT, _T("RomData: skipping '%hs' (already loaded)\n"), pParsed->szShortName);
		return nIndex;
	}
	return rd_build_and_link(pParsed);
}

static INT32 rd_normalize_path(const TCHAR* szInput, TCHAR* szOutput)
{
	if (rd_is_empty(szInput) || !szOutput)
		return 0;

#if defined(BUILD_WIN32) || defined(_WIN32)
	DWORD nFullLength = GetFullPathName(szInput, MAX_PATH, szOutput, NULL);
	if (!nFullLength || nFullLength >= MAX_PATH)
		return 0;
	for (TCHAR* p = szOutput; *p; p++) {
		if (*p == _T('/'))
			*p = _T('\\');
	}
#else
	if (!realpath(szInput, szOutput)) {
		if (szInput[0] == _T('/')) {
			_tcsncpy(szOutput, szInput, MAX_PATH - 1);
			szOutput[MAX_PATH - 1] = _T('\0');
		} else {
			if (!getcwd(szOutput, MAX_PATH))
				return 0;
			size_t nCurrentLength = _tcslen(szOutput);
			INT32 nLength = _sntprintf(szOutput + nCurrentLength, MAX_PATH - nCurrentLength, _T("/%s"), szInput);
			if (nLength < 0 || nLength >= MAX_PATH - (INT32)nCurrentLength)
				return 0;
		}
	}
#endif

	size_t nLength = _tcslen(szOutput);
	while (nLength > 1 && (szOutput[nLength - 1] == _T('/') || szOutput[nLength - 1] == _T('\\')))
		szOutput[--nLength] = _T('\0');
	return 1;
}

static INT32 rd_same_directory(const TCHAR* szFilePath, const TCHAR* szDirectory)
{
	TCHAR szFileDir[MAX_PATH];
	_tcsncpy(szFileDir, szFilePath, MAX_PATH - 1);
	szFileDir[MAX_PATH - 1] = _T('\0');

	TCHAR* pSlash = _tcsrchr(szFileDir, _T('/'));
	TCHAR* pBackslash = _tcsrchr(szFileDir, _T('\\'));
	if (!pSlash || (pBackslash && pBackslash > pSlash))
		pSlash = pBackslash;
	if (!pSlash)
		_tcsncpy(szFileDir, _T("."), MAX_PATH);
	else
		*pSlash = _T('\0');

	TCHAR szSource[MAX_PATH];
	TCHAR szDestination[MAX_PATH];
	if (!rd_normalize_path(szFileDir, szSource) || !rd_normalize_path(szDirectory, szDestination))
		return 0;
#if defined(BUILD_WIN32) || defined(_WIN32)
	return _tcsicmp(szSource, szDestination) == 0;
#else
	return _tcscmp(szSource, szDestination) == 0;
#endif
}

static INT32 rd_create_directory(const TCHAR* szDirectory)
{
	if (rd_is_empty(szDirectory))
		return 0;

	TCHAR szPath[MAX_PATH];
	if (!rd_normalize_path(szDirectory, szPath))
		return 0;

	TCHAR* pStart = szPath + 1;
#if defined(BUILD_WIN32) || defined(_WIN32)
	if (szPath[0] == _T('\\') && szPath[1] == _T('\\')) {
		pStart = _tcschr(szPath + 2, _T('\\'));
		if (!pStart)
			return 0;
		pStart = _tcschr(pStart + 1, _T('\\'));
		if (!pStart)
			return GetFileAttributes(szPath) != INVALID_FILE_ATTRIBUTES;
	}
#endif

	for (TCHAR* p = pStart; *p; p++) {
		if (*p != _T('/') && *p != _T('\\'))
			continue;
#if defined(BUILD_WIN32) || defined(_WIN32)
		if (p == szPath + 2 && szPath[1] == _T(':'))
			continue;
#endif
		TCHAR c = *p;
		*p = _T('\0');
#if defined(BUILD_WIN32) || defined(_WIN32)
		if (!CreateDirectory(szPath, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
#else
		if (mkdir(szPath, 0777) != 0 && errno != EEXIST)
#endif
			return 0;
		*p = c;
	}

#if defined(BUILD_WIN32) || defined(_WIN32)
	return CreateDirectory(szPath, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
#else
	return mkdir(szPath, 0777) == 0 || errno == EEXIST;
#endif
}

static INT32 rd_import_file(const TCHAR* szSource, const TCHAR* szDestination)
{
	if (rd_same_directory(szSource, szDestination))
		return 1;
	if (!rd_create_directory(szDestination))
		return 0;

	const TCHAR* pszName = _tcsrchr(szSource, _T('/'));
	const TCHAR* pszBackslash = _tcsrchr(szSource, _T('\\'));
	if (!pszName || (pszBackslash && pszBackslash > pszName))
		pszName = pszBackslash;
	pszName = pszName ? pszName + 1 : szSource;

	TCHAR szBase[MAX_PATH];
	TCHAR szExtension[MAX_PATH] = _T("");
	_tcsncpy(szBase, pszName, MAX_PATH - 1);
	szBase[MAX_PATH - 1] = _T('\0');
	TCHAR* pDot = _tcsrchr(szBase, _T('.'));
	if (pDot) {
		_tcsncpy(szExtension, pDot, MAX_PATH - 1);
		szExtension[MAX_PATH - 1] = _T('\0');
		*pDot = _T('\0');
	}

	for (INT32 i = 0; i < 10000; i++) {
		TCHAR szName[MAX_PATH];
		INT32 nNameLength = i
			? _sntprintf(szName, MAX_PATH, _T("%s_%d%s"), szBase, i, szExtension)
			: _sntprintf(szName, MAX_PATH, _T("%s%s"), szBase, szExtension);
		if (nNameLength < 0 || nNameLength >= MAX_PATH)
			continue;

		TCHAR szTarget[MAX_PATH];
		INT32 nTargetLength = _sntprintf(szTarget, MAX_PATH, _T("%s/%s"), szDestination, szName);
		if (nTargetLength < 0 || nTargetLength >= MAX_PATH)
			continue;

#if defined(BUILD_WIN32) || defined(_WIN32)
		if (CopyFile(szSource, szTarget, TRUE)) {
			bprintf(PRINT_NORMAL, _T("RomData: imported '%s' to '%s'\n"), szSource, szTarget);
			return 1;
		}
		if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS)
			break;
#else
		if (access(szTarget, F_OK) == 0)
			continue;
		FILE* pSource = _tfopen(szSource, _T("rb"));
		FILE* pTarget = pSource ? _tfopen(szTarget, _T("wb")) : NULL;
		UINT8 Buffer[65536];
		INT32 nSuccess = pTarget != NULL;
		while (nSuccess && !feof(pSource)) {
			size_t nRead = fread(Buffer, 1, sizeof(Buffer), pSource);
			if (nRead && fwrite(Buffer, 1, nRead, pTarget) != nRead)
				nSuccess = 0;
			if (ferror(pSource))
				nSuccess = 0;
		}
		if (pTarget && fclose(pTarget))
			nSuccess = 0;
		if (pSource)
			fclose(pSource);
		if (!nSuccess) {
			remove(szTarget);
			continue;
		}
		bprintf(PRINT_NORMAL, _T("RomData: imported '%s' to '%s'\n"), szSource, szTarget);
		return 1;
#endif
	}

	bprintf(PRINT_ERROR, _T("RomData: failed to import '%s' to '%s'\n"), szSource, szDestination);
	return 0;
}

extern "C" INT32 RomDataLoadOne(const TCHAR* szDatPath)
{
	if (rd_is_empty(szDatPath) || !rd_is_dat(szDatPath))
		return -1;

	UINT64 nSize, nWriteTime;
	if (!rd_get_stamp(szDatPath, &nSize, &nWriteTime))
		return -1;

	struct RomDataParsed Parsed;
	memset(&Parsed, 0, sizeof(Parsed));
	if (rd_load_parsed_text(szDatPath, nSize, nWriteTime, &Parsed))
		return -1;

	INT32 nIndex = rd_build_parsed(&Parsed);
	rd_parsed_free(&Parsed);
	return nIndex;
}

extern "C" INT32 RomDataCopyOne(const TCHAR* szDatPath, const TCHAR* szDestDir)
{
	if (rd_is_empty(szDatPath) || !rd_is_dat(szDatPath) || rd_is_empty(szDestDir))
		return 0;
	return rd_import_file(szDatPath, szDestDir);
}

extern "C" INT32 RomDataImportOne(const TCHAR* szDatPath, const TCHAR* szDestDir)
{
	UINT32 nCount = nBurnDrvCount;
	INT32 nIndex = RomDataLoadOne(szDatPath);
	if (nIndex < 0)
		return -1;
	if (nBurnDrvCount > nCount)
		RomDataCopyOne(szDatPath, szDestDir);
	return nIndex;
}

struct RomDataScanStat {
	UINT32 nLoaded;
	UINT32 nSkipped;
	UINT32 nFailed;
};

struct RomDataCandidates {
	TCHAR (*pPaths)[MAX_PATH];
	UINT32 nCount;
	UINT32 nCapacity;
};

static INT32 rd_reserve_candidates(struct RomDataCandidates* pCandidates, UINT32 nRequired)
{
	if (!pCandidates)
		return 1;
	if (nRequired <= pCandidates->nCapacity)
		return 0;

	UINT32 nCapacity = pCandidates->nCapacity ? pCandidates->nCapacity : 32;
	while (nCapacity < nRequired) {
		if (nCapacity > ~0U / 2) {
			nCapacity = nRequired;
			break;
		}
		nCapacity *= 2;
	}

	size_t nSize;
	if (!rd_multiply_size(nCapacity, sizeof(*pCandidates->pPaths), &nSize))
		return 1;

	TCHAR (*pPaths)[MAX_PATH] = (TCHAR (*)[MAX_PATH])realloc(pCandidates->pPaths, nSize);
	if (!pPaths)
		return 1;

	pCandidates->pPaths    = pPaths;
	pCandidates->nCapacity = nCapacity;
	return 0;
}

static INT32 rd_add_candidate(struct RomDataCandidates* pCandidates, const TCHAR* szPath)
{
	if (!pCandidates || !szPath || rd_reserve_candidates(pCandidates, pCandidates->nCount + 1))
		return 1;

	_tcsncpy(pCandidates->pPaths[pCandidates->nCount], szPath, MAX_PATH - 1);
	pCandidates->pPaths[pCandidates->nCount][MAX_PATH - 1] = _T('\0');
	pCandidates->nCount++;
	return 0;
}

static INT32 rd_collect_dir(const TCHAR* szDir, INT32 depth, struct RomDataCandidates* pCandidates)
{
	if (!szDir || depth > 4 || !pCandidates)
		return 1;

	RD_DIR* dp = rd_opendir(szDir);
	if (!dp)
		return 1;

	INT32 nResult = 0;
	struct RD_dirent* de;
	TCHAR path[MAX_PATH];
	while ((de = rd_readdir(dp)) != NULL) {
		const TCHAR* pszName = rd_name(de);
		if (_tcscmp(pszName, _T(".")) == 0 || _tcscmp(pszName, _T("..")) == 0)
			continue;

		INT32 nLen = _sntprintf(path, MAX_PATH, _T("%s/%s"), szDir, pszName);
		if (nLen < 0 || nLen >= MAX_PATH)
			continue;

		RD_DIR* sub = rd_opendir(path);
		if (sub) {
			rd_closedir(sub);
			if (depth < 4 && rd_collect_dir(path, depth + 1, pCandidates)) nResult = 1;
			continue;
		}

		if (rd_is_dat(path) && rd_add_candidate(pCandidates, path)) {
			nResult = 1;
			break;
		}
	}
	rd_closedir(dp);
	return nResult;
}

extern "C" void RomDataImportDirectory(const TCHAR* szDir, const TCHAR* szDestDir)
{
	if (rd_is_empty(szDir) || rd_is_empty(szDestDir))
		return;

	struct RomDataCandidates Candidates = { NULL, 0, 0 };
	if (rd_collect_dir(szDir, 0, &Candidates)) {
		bprintf(PRINT_ERROR, _T("RomData: failed to enumerate '%s'\n"), szDir);
		free_s((void**)&Candidates.pPaths);
		return;
	}

	if (rd_reserve_records(nRDDrvCount + Candidates.nCount) || BurnReserveExtlDrivers(Candidates.nCount)) {
		bprintf(PRINT_ERROR, _T("RomData: out of memory reserving %u driver(s)\n"), Candidates.nCount);
		free_s((void**)&Candidates.pPaths);
		return;
	}

	struct RomDataScanStat st = { 0, 0, 0 };
	for (UINT32 i = 0; i < Candidates.nCount; i++) {
		UINT32 nCount = nBurnDrvCount;
		if (RomDataImportOne(Candidates.pPaths[i], szDestDir) < 0)
			st.nFailed++;
		else if (nBurnDrvCount > nCount)
			st.nLoaded++;
		else
			st.nSkipped++;
	}
	free_s((void**)&Candidates.pPaths);

	if (st.nLoaded || st.nSkipped || st.nFailed) {
		bprintf(st.nFailed ? PRINT_ERROR : PRINT_NORMAL, _T("RomData: imported %u, skipped %u, failed %u of %u .dat file(s) from '%s'\n"),
				st.nLoaded, st.nSkipped, st.nFailed, st.nLoaded + st.nSkipped + st.nFailed, szDir);
	}
}

static void rd_scan(const TCHAR* szDir, const TCHAR* szRequestedCachePath, INT32 bAppend)
{
	if (rd_is_empty(szDir))
		return;

	struct RomDataCandidates Candidates = { NULL, 0, 0 };
	if (rd_collect_dir(szDir, 0, &Candidates)) {
		bprintf(PRINT_ERROR, _T("RomData: failed to enumerate '%s'\n"), szDir);
		free_s((void**)&Candidates.pPaths);
		return;
	}

	if (!bAppend)
		RomDataFree();
	if (rd_reserve_records(nRDDrvCount + Candidates.nCount) || BurnReserveExtlDrivers(Candidates.nCount)) {
		bprintf(PRINT_ERROR, _T("RomData: out of memory reserving %u driver(s)\n"), Candidates.nCount);
		free_s((void**)&Candidates.pPaths);
		return;
	}

	TCHAR szCachePath[MAX_PATH];
	INT32 nHaveCachePath = rd_cache_path(szRequestedCachePath, szCachePath);
	if (!nHaveCachePath) {
		struct RomDataScanStat st = { 0, 0, 0 };
		for (UINT32 i = 0; i < Candidates.nCount; i++) {
			UINT32 nCount = nBurnDrvCount;
			if (RomDataLoadOne(Candidates.pPaths[i]) < 0)
				st.nFailed++;
			else if (nBurnDrvCount > nCount)
				st.nLoaded++;
			else
				st.nSkipped++;
		}
		free_s((void**)&Candidates.pPaths);
		if (st.nLoaded || st.nSkipped || st.nFailed) {
			bprintf(st.nFailed ? PRINT_ERROR : PRINT_NORMAL, _T("RomData: added %u, skipped %u, failed %u of %u .dat file(s) from '%s'\n"),
					st.nLoaded, st.nSkipped, st.nFailed, st.nLoaded + st.nSkipped + st.nFailed, szDir);
		}
		return;
	}

	struct RDCache OldCache = { NULL, 0, 0 };
	struct RDCache NewCache = { NULL, 0, 0 };
	INT32 nHaveOldCache = rd_cache_load(szCachePath, szDir, &OldCache);
	INT32 nCacheDirty   = !nHaveOldCache;
	UINT32 nCacheHits   = 0;

	struct RomDataScanStat st = { 0, 0, 0 };
	for (UINT32 i = 0; i < Candidates.nCount; i++) {
		const TCHAR* szPath = Candidates.pPaths[i];
		UINT64 nSize, nWriteTime;
		struct RomDataParsed Parsed;
		memset(&Parsed, 0, sizeof(Parsed));

		INT32 nLoaded = 0;
		struct RDCacheEntry* pCached = rd_cache_find(&OldCache, szPath);
		if (nHaveOldCache && rd_get_stamp(szPath, &nSize, &nWriteTime) && pCached && pCached->nSize == nSize && pCached->nWriteTime == nWriteTime) {
			INT32 nBaseIdx = BurnDrvGetIndex(pCached->Parsed.szDrvName);
			UINT64 nFingerprint = rd_base_fingerprint(nBaseIdx, pCached->Parsed.nRomInfoCount);
			if (nBaseIdx >= 0 && nFingerprint == pCached->nBaseFingerprint) {
				Parsed = pCached->Parsed;
				memset(&pCached->Parsed, 0, sizeof(pCached->Parsed));
				Parsed.nBaseIdx         = nBaseIdx;
				Parsed.szPath           = szPath;
				Parsed.nDatSize         = nSize;
				Parsed.nDatWriteTime    = nWriteTime;
				Parsed.nBaseFingerprint = nFingerprint;
				nLoaded = 1;
				nCacheHits++;
			}
		}
		if (!nLoaded) {
			nCacheDirty = 1;
			if (!rd_get_stamp(szPath, &nSize, &nWriteTime) || rd_load_parsed_text(szPath, nSize, nWriteTime, &Parsed)) {
				st.nFailed++;
				continue;
			}
		}

		struct RDCacheEntry Entry;
		memset(&Entry, 0, sizeof(Entry));
		_tcsncpy(Entry.szPath, szPath, MAX_PATH - 1);
		Entry.szPath[MAX_PATH - 1] = _T('\0');
		Entry.nSize            = Parsed.nDatSize;
		Entry.nWriteTime       = Parsed.nDatWriteTime;
		Entry.nBaseFingerprint = Parsed.nBaseFingerprint;
		Entry.Parsed           = Parsed;
		memset(&Parsed, 0, sizeof(Parsed));

		struct RomDataParsed Build;
		if (!rd_parsed_copy(&Build, &Entry.Parsed)) {
			rd_cache_entry_free(&Entry);
			st.nFailed++;
			continue;
		}
		Build.szPath = szPath;
		Build.nBaseIdx = BurnDrvGetIndex(Build.szDrvName);
		UINT32 nCount = nBurnDrvCount;
		if (rd_build_parsed(&Build) >= 0) {
			if (!rd_cache_add_move(&NewCache, &Entry)) {
				nCacheDirty = 1;
				rd_cache_entry_free(&Entry);
			}
			if (nBurnDrvCount > nCount)
				st.nLoaded++;
			else
				st.nSkipped++;
		} else {
			nCacheDirty = 1;
			rd_cache_entry_free(&Entry);
			st.nFailed++;
		}
		rd_parsed_free(&Build);
	}
	free_s((void**)&Candidates.pPaths);

	if (nHaveOldCache && nCacheHits != OldCache.nCount)
		nCacheDirty = 1;

	rd_cache_free(&OldCache);
	if (nCacheDirty && !rd_cache_save(szCachePath, szDir, &NewCache)) {
		bprintf(PRINT_ERROR, _T("RomData: failed to update cache '%s'\n"), szCachePath);
	}
	rd_cache_free(&NewCache);

	if (st.nLoaded || st.nSkipped || st.nFailed) {
		bprintf(st.nFailed ? PRINT_ERROR : PRINT_NORMAL, _T("RomData: added %u, skipped %u, failed %u of %u .dat file(s), %u cache hit(s), from '%s'\n"),
				st.nLoaded, st.nSkipped, st.nFailed, st.nLoaded + st.nSkipped + st.nFailed, nCacheHits, szDir);
	}
}

extern "C" void RomDataScan(const TCHAR* szDir, const TCHAR* szCachePath)
{
	rd_scan(szDir, szCachePath, 0);
}

extern "C" void RomDataScanAppend(const TCHAR* szDir, const TCHAR* szCachePath)
{
	rd_scan(szDir, szCachePath, 1);
}

extern "C" void RomDataFree(void)
{
	// Unlink back to front: each removal shifts the slots above it down by one.
	for (UINT32 i = pRDDrv ? nRDDrvCount : 0; i > 0; i--) {
		struct RomDataDrv* rec = pRDDrv[i - 1];
		if (!rec)
			continue;

		if (UnlinkExtlDriver(&rec->drv) != 0) {
			bprintf(PRINT_ERROR, _T("RomData: failed to unlink '%hs'\n"), rec->pszShortName);
		}
		rd_free_record(rec);
	}
	free_s((void**)&pRDDrv);
	nRDDrvCount    = 0;
	nRDDrvCapacity = 0;
}

extern "C" bool IsRomDataDrv(void)
{
	// Only this file sets the flag, so the flag alone identifies our drivers.
	return (rd_active() != NULL);
}

extern "C" char* RomDataDrvGetDrvName(void)
{
	struct RomDataDrv* rd = rd_active();
	return rd ? rd->pszDrvName : NULL;
}

extern "C" struct BurnRomInfo* RomDataDrvGetRomInfo(UINT32* pRomCount)
{
	struct RomDataDrv* rd = rd_active();
	if (!rd)
		return NULL;

	if (pRomCount)
		*pRomCount = rd->nRomInfoCount;

	return rd->pRomInfo;
}

extern "C" const char* RomDataDrvGetExtName(void)
{
	struct RomDataDrv* rd = rd_active();
	return rd ? rd->pszExtName : NULL;
}

#undef SHORT_MAX
#undef DATE_MAX
#undef RD_BOARDROM_BASE
#undef RD_CALL_BASE
#undef RD_DIR
#undef RD_dirent
#undef rd_opendir
#undef rd_readdir
#undef rd_closedir
#undef rd_name
