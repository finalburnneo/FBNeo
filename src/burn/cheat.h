#define CHEAT_MAX_ADDRESS (512)
#define CHEAT_MAX_OPTIONS (512)
#define CHEAT_MAX_NAME	  (128)

extern bool bCheatsAllowed;

struct CheatAddressInfo {
	INT32 nCPU;
	INT32 nAddress;
	INT32 nMultiByte; // byte number for this address
	INT32 nTotalByte; // total bytes for this address,  "1, 2, 3, 4"  (..or  8, 16, 24, 32bit)
	UINT32 nValue;
	UINT32 nExtended;                           // Full extended field
	UINT8 nMask;                                // Mask for this byte (based on extended field)
	UINT32 nOriginalValue;

	INT32 bRelAddress;                          // Relative address (pointer offset) cheat, see :rdft2: or :dreamwld: in cheat.dat
	INT32 nRelAddressOffset;                    // The offset
	INT32 nRelAddressBits;                      // 0, 1, 2, 3 = 8, 16, 24, 32bit
	char szGenieCode[0x10];                     // Game Genie Code (NES)
};

struct CheatOption {
	TCHAR szOptionName[CHEAT_MAX_NAME];
	struct CheatAddressInfo AddressInfo[CHEAT_MAX_ADDRESS + 1];
};

struct CheatInfo {
	struct CheatInfo* pNext;
	struct CheatInfo* pPrevious;
	INT32 nType;								// Cheat type
	INT32 nStatus;								// 0 = Inactive
	INT32 nCurrent;								// Currently selected option
	INT32 nDefault;								// Default option
	INT32 bOneShot;                             // For one-shot cheats, also acts as a frame counter for them.
	INT32 bRestoreOnDisable;                    // Restore previous value on disable
	INT32 nPrefillMode;                         // Prefill data (1: 0xff, 2: 0x00, 3: 0x01)
	INT32 bWatchMode;                           // Display value on screen
	INT32 bWaitForModification;                 // Wait for Modification before changing
	INT32 bModified;                            // Wrote cheat?
	INT32 bWriteWithMask;                       // Use nExtended field as mask

	TCHAR szCheatName[CHEAT_MAX_NAME];
	struct CheatOption* pOption[CHEAT_MAX_OPTIONS];
};

extern CheatInfo* pCheatInfo;

INT32 CheatUpdate();
INT32 CheatEnable(INT32 nCheat, INT32 nOption);
INT32 CheatApply();
INT32 CheatInit();
void CheatExit();

#define CHEATSEARCH_SHOWRESULTS		3
extern UINT32 CheatSearchShowResultAddresses[CHEATSEARCH_SHOWRESULTS];
extern UINT32 CheatSearchShowResultValues[CHEATSEARCH_SHOWRESULTS];

INT32 CheatSearchInit();
void CheatSearchExit();
int CheatSearchStart();
UINT32 CheatSearchValueNoChange();
UINT32 CheatSearchValueChange();
UINT32 CheatSearchValueDecreased();
UINT32 CheatSearchValueIncreased();
void CheatSearchDumptoFile();

typedef void (*CheatSearchInitCallback)();
extern CheatSearchInitCallback CheatSearchInitCallbackFunction;
void CheatSearchExcludeAddressRange(UINT32 nStart, UINT32 nEnd);

typedef UINT32 HWAddressType;

unsigned int ReadValueAtHardwareAddress(HWAddressType address, unsigned int size, int isLittleEndian);
bool WriteValueAtHardwareAddress(HWAddressType address, unsigned int value, unsigned int size, int isLittleEndian);
bool IsHardwareAddressValid(HWAddressType address);
