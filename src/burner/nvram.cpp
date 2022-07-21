// Driver nvram module
#include "burner.h"

static UINT8 *pNvramData;
static INT32 nTotalLen = 0;

static INT32 __cdecl StateLenAcb(struct BurnArea* pba)
{
	nTotalLen += pba->nLen;
	return 0;
}

static INT32 __cdecl NvramLoadAcb(struct BurnArea* pba)
{
	memcpy(pba->Data, pNvramData, pba->nLen);
	pNvramData += pba->nLen;
	return 0;
}

static INT32 __cdecl NvramSaveAcb(struct BurnArea* pba)
{
	memcpy(pNvramData, pba->Data, pba->nLen);
	pNvramData += pba->nLen;
	return 0;
}

static INT32 NvramInfo(int* pnLen)
{
	INT32 nMin = 0;
	nTotalLen = 0;
	BurnAcb = StateLenAcb;

	BurnAreaScan(ACB_NVRAM | ACB_READ, &nMin);

	*pnLen = nTotalLen;

	return 0;
}

INT32 BurnNvramLoad(TCHAR* szName)
{
	INT32 nLen = 0;
	char ReadHeader[8];

	FILE* fp = _tfopen(szName, _T("rb"));
	if (fp == NULL) {
		return 1;
	}

	// abort the loading of older headered/compressed nvrams
	memset(ReadHeader, 0, 8);
	fread(ReadHeader, 1, 8, fp);
	if (memcmp(ReadHeader, "FB1 FS1 ", 8) == 0) {
		fclose(fp);
		return 1;
	}
	fseek(fp, 0L, SEEK_SET);

	fseek(fp, 0L, SEEK_END);
	nLen = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	UINT8 *data = (UINT8*)malloc(nLen);
	if (data == NULL) {
		fclose(fp);
		return 1;
	}
	fread(data, 1, nLen, fp);
	fclose(fp);

	BurnAcb = NvramLoadAcb;
	pNvramData = data;
	BurnAreaScan(ACB_NVRAM | ACB_WRITE, NULL);

	if (data) {
		free(data);
		data = NULL;
	}

	return 0;
}

INT32 BurnNvramSave(TCHAR* szName)
{
	INT32 nLen = 0;
	INT32 nRet = 0;

	NvramInfo(&nLen);

	if (nLen <= 0) {
		return 1;
	}

	FILE* fp = _tfopen(szName, _T("wb"));
	if (fp == NULL) {
		return 1;
	}

	UINT8 *data = (UINT8*)malloc(nLen);
	if (data == NULL) {
		fclose(fp);
		return 1;
	}

	BurnAcb = NvramSaveAcb;
	pNvramData = data;
	BurnAreaScan(ACB_NVRAM | ACB_READ, NULL);

	nRet = fwrite(data, 1, nLen, fp);

	fclose(fp);

	if (data) {
		free(data);
		data = NULL;
	}

	if (nRet != nLen) {
		return 1;
	}

	return 0;
}
