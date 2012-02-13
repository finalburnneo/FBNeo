// Zip module
#include "burner.h"
#include "unzip.h"

static unzFile Zip=NULL;
static INT32 nCurrFile=0; // The current file we are pointing to

INT32 ZipOpen(char* szZip)
{
	if (szZip == NULL) return 1;
	
	strcat(szZip, ".zip");

	Zip = unzOpen(szZip);
	if (Zip == NULL) return 1;
	unzGoToFirstFile(Zip); nCurrFile=0;
	return 0;
}

INT32 ZipClose()
{
	if (Zip != NULL) {
		unzClose(Zip);
		Zip = NULL;
	}
	return 0;
}

// Get the contents of a zip file into an array of ZipEntrys
INT32 ZipGetList(struct ZipEntry** pList, INT32* pnListCount)
{
	if (Zip == NULL) return 1;
	if (pList == NULL) return 1;

	unz_global_info ZipGlobalInfo;
	memset(&ZipGlobalInfo, 0, sizeof(ZipGlobalInfo));

	unzGetGlobalInfo(Zip, &ZipGlobalInfo);
	INT32 nListLen = ZipGlobalInfo.number_entry;

	// Make an array of File Entries
	struct ZipEntry* List = (struct ZipEntry *)malloc(nListLen * sizeof(struct ZipEntry));
	if (List == NULL) { unzClose(Zip); return 1; }
	memset(List, 0, nListLen * sizeof(struct ZipEntry));

	INT32 nRet = unzGoToFirstFile(Zip);
	if (nRet != UNZ_OK) { unzClose(Zip); return 1; }

	// Step through all of the files, until we get to the end
	INT32 nNextRet = 0;

	for (nCurrFile = 0, nNextRet = UNZ_OK;
		nCurrFile < nListLen && nNextRet == UNZ_OK;
		nCurrFile++, nNextRet = unzGoToNextFile(Zip))
	{
		unz_file_info FileInfo;
		memset(&FileInfo, 0, sizeof(FileInfo));

		nRet = unzGetCurrentFileInfo(Zip, &FileInfo, NULL, 0, NULL, 0, NULL, 0);
		if (nRet != UNZ_OK) continue;

		// Allocate space for the filename
		char* szName = (char *)malloc(FileInfo.size_filename + 1);
		if (szName == NULL) continue;

		nRet = unzGetCurrentFileInfo(Zip, &FileInfo, szName, FileInfo.size_filename + 1, NULL, 0, NULL, 0);
		if (nRet != UNZ_OK) continue;

		List[nCurrFile].szName = szName;
		List[nCurrFile].nLen = FileInfo.uncompressed_size;
		List[nCurrFile].nCrc = FileInfo.crc;
	}

	// return the file list
	*pList = List;
	if (pnListCount != NULL) *pnListCount = nListLen;

	unzGoToFirstFile(Zip);
	nCurrFile = 0;
	return 0;
}

INT32 ZipLoadFile(UINT8* Dest, INT32 nLen, INT32* pnWrote, INT32 nEntry)
{
	if (Zip == NULL) return 1;

	INT32 nRet = 0;
	if (nEntry < nCurrFile)
	{
		// We'll have to go through the zip file again to get to our entry
		nRet = unzGoToFirstFile(Zip);
		if (nRet != UNZ_OK) return 1;
		nCurrFile = 0;
	}

	// Now step through to the file we need
	while (nCurrFile < nEntry)
	{
		nRet = unzGoToNextFile(Zip);
		if (nRet != UNZ_OK) return 1;
		nCurrFile++;
	}

	nRet = unzOpenCurrentFile(Zip);
	if (nRet != UNZ_OK) return 1;

	nRet = unzReadCurrentFile(Zip, Dest, nLen);
	// Return how many bytes were copied
	if (nRet >= 0 && pnWrote != NULL) *pnWrote = nRet;

	nRet = unzCloseCurrentFile(Zip);
	if (nRet == UNZ_CRCERROR) return 2;
	if (nRet != UNZ_OK) return 1;

	return 0;
}

// Load one file directly, added by regret
INT32 __cdecl ZipLoadOneFile(char* arcName, const char* fileName, void** Dest, INT32* pnWrote)
{
	if (ZipOpen(arcName)) {
		return 1;
	}

	unz_global_info ZipGlobalInfo;
	memset(&ZipGlobalInfo, 0, sizeof(ZipGlobalInfo));

	unzGetGlobalInfo(Zip, &ZipGlobalInfo);
	INT32 nListLen = ZipGlobalInfo.number_entry;
	if (nListLen <= 0) {
		ZipClose();
		return 1;
	}

	INT32 nRet = unzGoToFirstFile(Zip);
	if (nRet != UNZ_OK) { unzClose(Zip); return 1; }

	unz_file_info FileInfo;
	memset(&FileInfo, 0, sizeof(FileInfo));

	if (fileName != NULL) {
		// Step through all of the files, until we get to the end
		INT32 nNextRet = 0;
		char szName[MAX_PATH] = "";

		for (nCurrFile = 0, nNextRet = UNZ_OK;
			nCurrFile < nListLen && nNextRet == UNZ_OK;
			nCurrFile++, nNextRet = unzGoToNextFile(Zip))
		{
			nRet = unzGetCurrentFileInfo(Zip, &FileInfo, szName, MAX_PATH, NULL, 0, NULL, 0);
			if (nRet != UNZ_OK) continue;

			if (!strcmp(szName, fileName)) {
				break;
			}
		}

		if (nCurrFile == nListLen) {
			ZipClose();
			return 1; // didn't find
		}
	}
	else {
		nRet = unzGetCurrentFileInfo(Zip, &FileInfo, NULL, 0, NULL, 0, NULL, 0);
		if (nRet != UNZ_OK) {
			ZipClose();
			return 1;
		}
	}

	// Extract file
	nRet = unzOpenCurrentFile(Zip);
	if (nRet != UNZ_OK) {
		unzCloseCurrentFile(Zip);
		ZipClose();
		return 1;
	}

	if (*Dest == NULL) {
		*Dest = (UINT8*)malloc(FileInfo.uncompressed_size);
		if (!*Dest) {
			unzCloseCurrentFile(Zip);
			ZipClose();
			return 1;
		}
	}

	nRet = unzReadCurrentFile(Zip, *Dest, FileInfo.uncompressed_size);
	// Return how many bytes were copied
	if (nRet >= 0 && pnWrote != NULL) *pnWrote = nRet;

	nRet = unzCloseCurrentFile(Zip);
	ZipClose();

	if (nRet == UNZ_CRCERROR) {
		if (*Dest) {
			free(*Dest);
		}
		return 2;
	}
	if (nRet != UNZ_OK) {
		if (*Dest) {
			free(*Dest);
		}
		return 1;
	}

	return 0;
}
