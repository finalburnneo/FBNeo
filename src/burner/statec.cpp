// FB Neo Driver State load/save from buffer module        - dink 2024
#include "burnint.h"

// BurnStateCompress: Save a state, "Compress" == organized into a buffer
// BurnStateDecompress: Load a state from buffer

#define DEBUG_STATEC 1
#define stateclog(x) do { if (DEBUG_STATEC) bprintf x; } while (0)

extern bool bWithEEPROM; // from state.cpp, win32/replay.cpp

static INT32 nStateLoadFileLength = 0; // file size loaded
static INT32 nCalculatedLength = 0; // calculated size from driver
static INT32 nCalculatedVars = 0; // calculated variable count from driver
static INT32 nBufferPosition = 0; // position of load/save progress
static bool bNeedToRecover = false;
static UINT8 *Buffer = NULL;
static UINT8 *pBuffer = NULL;

static const UINT32 BLOCK_ID_STATE = 0xa55a0000;
static const UINT32 BLOCK_ID_VARIABLE = 0xa55a0001;
static const UINT32 STATE_SIZE_1_ENTRY = sizeof(UINT32) * 3; // block id, data length, data name hash
static const UINT32 STATE_SIZE_HEADER = sizeof(UINT32) * 2; // block id, reserved (32 bits future data)

static UINT32 HashString(char *in_str)
{
	UINT32 hash = 0xc0ffee;
	const int in_strlen = strlen(in_str);
	for (INT32 i = 0; i < in_strlen; i++) {
		hash = (hash ^ in_str[i]) + ((hash << 0x1a) + (hash >> 0x06));
	}
	return hash;
}

static void AddToBuffer(void *data, UINT32 data_size)
{
	memcpy(pBuffer, data, data_size);
	pBuffer += data_size;
	nBufferPosition += data_size;
}

static void GetFromBuffer(void *data, UINT32 data_size)
{
	if (data != NULL) memcpy(data, pBuffer, data_size);
	pBuffer += data_size;
	nBufferPosition += data_size;
}

static INT32 __cdecl LenAcb(struct BurnArea* pba)
{
	// block id, data length, data name hash, data
	nCalculatedLength += STATE_SIZE_1_ENTRY + pba->nLen;
	nCalculatedVars ++;
	return 0;
}

static INT32 __cdecl SaveAcb(struct BurnArea* pba)
{
	if (pba->nLen + STATE_SIZE_1_ENTRY + nBufferPosition > nCalculatedLength) {
		stateclog((0, _T("SaveAcb(): No space for \"%S\"  nCalculatedLength  %x  pba->nLen  %x  nBufferPosition  %x\n"), pba->szName, nCalculatedLength, pba->nLen, nBufferPosition));
		return 1;
	}

	UINT32 strhash = HashString(pba->szName);

	stateclog((0, _T("save  %S\t\tlength  %x %x\n"), pba->szName, pba->nLen, strhash));

	// copy block identifier
	AddToBuffer((void*)&BLOCK_ID_VARIABLE, sizeof(UINT32));

	// copy length of variable
	AddToBuffer(&pba->nLen, sizeof(UINT32));

	// copy hash of variable string
	AddToBuffer(&strhash, sizeof(UINT32));

	// copy variable
	AddToBuffer(pba->Data, pba->nLen);

	return 0;
}

static INT32 __cdecl LoadAcb(struct BurnArea* pba)
{
	int tries = 0;
	bool gotit = false;

	UINT8 *pBufferSave = pBuffer;
	UINT32 nBufferPositionSave = nBufferPosition;

	do {
		if (pba->nLen + STATE_SIZE_1_ENTRY + nBufferPosition > nStateLoadFileLength) {
			stateclog((0, _T("LoadAcb(): No space to load \"%S\"  nCalculatedLength  %x  pba->nLen  %x  nBufferPosition  %x\n"), pba->szName, nCalculatedLength, pba->nLen, nBufferPosition));
			break;
		}

		UINT32 length = 0;
		UINT32 loadedblockid = 0;
		UINT32 loadedhash = 0;
		UINT32 ourhash = HashString(pba->szName);

		stateclog((0, _T("load  %S\t\tlength  %x  %x  (%x, %x)\n"), pba->szName, pba->nLen, ourhash, nBufferPosition, nCalculatedLength));

		GetFromBuffer(&loadedblockid, sizeof(UINT32));

		if (BLOCK_ID_VARIABLE != loadedblockid) {
			stateclog((0, _T("-- block ID fail - we probably can't recover from this!\n")));
			break;
		}

		GetFromBuffer(&length, sizeof(UINT32));
		GetFromBuffer(&loadedhash, sizeof(UINT32));

		if (length == pba->nLen && ourhash == loadedhash) {
			GetFromBuffer(pba->Data, pba->nLen);
			gotit = true;
		} else {
			stateclog((0, _T("|-- length doesn't match driver length/hash!  skipping.\n")));

			GetFromBuffer(NULL, length); // just increment pointer using length from state buffer
		}

	} while (!gotit && ++tries < 50);

	if (gotit == false) {
		stateclog((0, _T("Can't seem to match, let's rewind pBuffer!\n")));

		bNeedToRecover = true;
		pBuffer = pBufferSave;
		nBufferPosition = nBufferPositionSave;
	} else {
		if (bNeedToRecover) {
			stateclog((0, _T("|-- Success - We made a nice recovery!  That was close :)\n")));

			bNeedToRecover = false;
		}
	}

	return 0;
}

// Save a state, "Compress" == organized into a buffer
INT32 BurnStateCompress(UINT8** pDef, INT32* pnDefLen, INT32 bAll)
{
	UINT32 nAddEEPROM = (bWithEEPROM) ? ACB_EEPROM : 0;

	nCalculatedLength = STATE_SIZE_HEADER;
	nCalculatedVars = 0;
	BurnAcb = LenAcb; // Get length of state buffer

	if (bAll) BurnAreaScan(ACB_FULLSCAN | ACB_READ, NULL);				// scan all ram (read from driver variables)
	else      BurnAreaScan(ACB_NVRAM | nAddEEPROM | ACB_READ, NULL);	// scan nvram

	stateclog((0, _T("BurnStateCompress(Save): Sizes, Calculated:  %x  Entries:  %x\n"), nCalculatedLength, nCalculatedVars));

	// Set-up the buffer
	Buffer = (UINT8*)malloc(nCalculatedLength); // note: this is free()'d in state.cpp
	nBufferPosition = 0;
	pBuffer = Buffer;
	BurnAcb = SaveAcb;

	// Add BLOCKID/header to the beginning of state buffer
	UINT32 future_use = 0;
	AddToBuffer((void *)&BLOCK_ID_STATE, sizeof(UINT32));
	AddToBuffer(&future_use, sizeof(UINT32));

	if (bAll) BurnAreaScan(ACB_FULLSCAN | ACB_READ, NULL);				// scan all ram (read from driver variables)
	else      BurnAreaScan(ACB_NVRAM | nAddEEPROM | ACB_READ, NULL);	// scan nvram

	if (pDef) *pDef = Buffer;
	if (pnDefLen) *pnDefLen = nCalculatedLength;

	return 0;
}

// Load a state from buffer
INT32 BurnStateDecompress(UINT8* Def, INT32 nDefLen, INT32 bAll)
{
	UINT32 nAddEEPROM = (bWithEEPROM) ? ACB_EEPROM : 0;

	nStateLoadFileLength = nDefLen;

	nCalculatedLength = 0;
	nCalculatedVars = 0;
	BurnAcb = LenAcb; // Get length of state buffer

	if (bAll) BurnAreaScan(ACB_FULLSCAN | ACB_WRITE, NULL);				// scan all ram (write to driver variables)
	else      BurnAreaScan(ACB_NVRAM | nAddEEPROM | ACB_WRITE, NULL);	// scan nvram

	stateclog((0, _T("BurnStateDecompress(Load): Sizes, Calculated:  %x  Loaded:  %x\n"), nCalculatedLength, nDefLen));

	bNeedToRecover = false;
	nBufferPosition = 0;
	pBuffer = Def;
	BurnAcb = LoadAcb;

	UINT32 test_header;
	UINT32 future_use;
	GetFromBuffer(&test_header, sizeof(UINT32));
	GetFromBuffer(&future_use, sizeof(UINT32));

	if (test_header != BLOCK_ID_STATE) {
		stateclog((0, _T("State file header is missing or corrupt!\n")));
		return 1;
	}

	if (bAll) BurnAreaScan(ACB_FULLSCAN | ACB_WRITE, NULL);             // scan all ram (write to driver variables)
	else      BurnAreaScan(ACB_NVRAM | nAddEEPROM | ACB_WRITE, NULL);	// scan nvram

	return 0;
}
