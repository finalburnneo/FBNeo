// Standard ROM/input/DIP info functions

#ifndef ARRAY_SIZE
#ifdef _MSC_VER
#define ARRAY_SIZE(arr) _countof(arr)
#else
#define ARRAY_SIZE(arr) ((INT32)(sizeof(arr) / sizeof((arr)[0])))
#endif // _MSC_VER
#endif // !ARRAY_SIZE

// A function to pick a rom, or return NULL if i is out of range
#define STD_ROM_PICK(Name)											\
static struct BurnRomInfo* Name##PickRom(UINT32 i)					\
{																	\
	/* RomDataDrv ROMs */											\
	if (IsRomDataDrv()) {											\
		UINT32 nCount = 0;											\
		struct BurnRomInfo* pInfo = RomDataDrvGetRomInfo(&nCount);	\
		if (!pInfo || i >= nCount) {								\
			return NULL;											\
		}															\
		return pInfo + i;											\
	}																\
	/* ROMs for built-in games */									\
	if (i >= ARRAY_SIZE(Name##RomDesc)) {							\
		return NULL;												\
	}																\
	return Name##RomDesc + i;										\
}

#define STDROMPICKEXT(Name, Info1, Info2)							\
static struct BurnRomInfo* Name##PickRom(UINT32 i)					\
{																	\
	/* Board ROMs */												\
	if (i >= 0x80) {												\
		i &= 0x7F;													\
		if (i >= ARRAY_SIZE(Info2##RomDesc)) {						\
			return NULL;											\
		}															\
		return Info2##RomDesc + i;									\
	}																\
	/* RomDataDrv ROMs */											\
	if (IsRomDataDrv()) {											\
		UINT32 nCount = 0;											\
		struct BurnRomInfo* pInfo = RomDataDrvGetRomInfo(&nCount);	\
		if (!pInfo || i >= nCount) {								\
			return emptyRomDesc + 0;								\
		}															\
		return pInfo + i;											\
	}																\
	/* ROMs for built-in games */									\
	if (i >= ARRAY_SIZE(Info1##RomDesc)) {							\
		return emptyRomDesc + 0;									\
	}																\
	return Info1##RomDesc + i;										\
}

// Standard rom functions for returning Length, Crc, Type and one one Name
#define STD_ROM_FN(Name)											\
static INT32 Name##RomInfo(struct BurnRomInfo* pri, UINT32 i)		\
{																	\
	struct BurnRomInfo* por = Name##PickRom(i);						\
	if (por == NULL) {												\
		return 1;													\
	}																\
	if (pri) {														\
		pri->nLen = por->nLen;										\
		pri->nCrc = por->nCrc;										\
		pri->nType = por->nType;									\
	}																\
	return 0;														\
}																	\
																	\
static INT32 Name##RomName(char** pszName, UINT32 i, INT32 nAka)	\
{											   		 				\
	struct BurnRomInfo *por = Name##PickRom(i);						\
	if (por == NULL) {												\
		return 1;													\
	}																\
	if (nAka) {														\
		return 1;													\
	}																\
	*pszName = por->szName;											\
	return 0;														\
}

#define STDINPUTINFO(Name)											\
static INT32 Name##InputInfo(struct BurnInputInfo* pii, UINT32 i)	\
{																	\
	if (i >= ARRAY_SIZE(Name##InputList)) {							\
		return 1;													\
	}																\
	if (pii) {														\
		*pii = Name##InputList[i];									\
	}																\
	return 0;														\
}

#define STDINPUTINFOSPEC(Name, Info1)								\
static INT32 Name##InputInfo(struct BurnInputInfo* pii, UINT32 i)	\
{																	\
	if (i >= ARRAY_SIZE(Info1)) {									\
		return 1;													\
	}																\
	if (pii) {														\
		*pii = Info1[i];											\
	}																\
	return 0;														\
}

#define STDINPUTINFOEXT(Name, Info1, Info2)							\
static INT32 Name##InputInfo(struct BurnInputInfo* pii, UINT32 i)	\
{																	\
	if (i >= ARRAY_SIZE(Info1##InputList)) {						\
		i -= ARRAY_SIZE(Info1##InputList);							\
		if (i >= ARRAY_SIZE(Info2##InputList)) {					\
			return 1;												\
		}															\
		if (pii) {													\
			*pii = Info2##InputList[i];								\
		}															\
		return 0;													\
	}																\
	if (pii) {														\
		*pii = Info1##InputList[i];									\
	}																\
	return 0;														\
}

#define STDDIPINFO(Name)											\
static INT32 Name##DIPInfo(struct BurnDIPInfo* pdi, UINT32 i)		\
{																	\
	if (i >= ARRAY_SIZE(Name##DIPList)) {							\
		return 1;													\
	}																\
	if (pdi) {														\
		*pdi = Name##DIPList[i];									\
	}																\
	return 0;														\
}

#define STDDIPINFOEXT(Name, Info1, Info2)							\
static INT32 Name##DIPInfo(struct BurnDIPInfo* pdi, UINT32 i)		\
{																	\
	if (i >= ARRAY_SIZE(Info1##DIPList)) {							\
		i -= ARRAY_SIZE(Info1##DIPList);							\
		if (i >= ARRAY_SIZE(Info2##DIPList)) {						\
			return 1;												\
		}															\
		if (pdi) {													\
			*pdi = Info2##DIPList[i];								\
		}															\
		return 0;													\
	}																\
	if (pdi) {														\
		*pdi = Info1##DIPList[i];									\
	}																\
	return 0;														\
}

// sample support
#define STD_SAMPLE_PICK(Name)										\
static struct BurnSampleInfo* Name##PickSample(UINT32 i)			\
{																	\
	if (i >= ARRAY_SIZE(Name##SampleDesc)) {						\
		return NULL;												\
	}																\
	return Name##SampleDesc + i;									\
}

#define STD_SAMPLE_FN(Name)											\
static INT32 Name##SampleInfo(struct BurnSampleInfo* pri, UINT32 i)	\
{																	\
	struct BurnSampleInfo* por = Name##PickSample(i);				\
	if (por == NULL) {												\
		return 1;													\
	}																\
	if (pri) {														\
		pri->nFlags = por->nFlags;									\
	}																\
	return 0;														\
}																	\
																	\
static INT32 Name##SampleName(char** pszName, UINT32 i, INT32 nAka)	\
{											   		 				\
	struct BurnSampleInfo *por = Name##PickSample(i);				\
	if (por == NULL) {												\
		return 1;													\
	}																\
	if (nAka) {														\
		return 1;													\
	}																\
	*pszName = por->szName;											\
	return 0;														\
}

// hdd support
#define STD_HDD_PICK(Name)											\
static struct BurnHDDInfo* Name##PickHDD(UINT32 i)					\
{																	\
	if (i >= ARRAY_SIZE(Name##HDDDesc)) {							\
		return NULL;												\
	}																\
	return Name##HDDDesc + i;										\
}

#define STD_HDD_FN(Name)											\
static INT32 Name##HDDInfo(struct BurnHDDInfo* pri, UINT32 i)		\
{																	\
	struct BurnHDDInfo* por = Name##PickHDD(i);						\
	if (por == NULL) {												\
		return 1;													\
	}																\
	if (pri) {														\
		pri->nLen = por->nLen;										\
		pri->nCrc = por->nCrc;										\
	}																\
	return 0;														\
}																	\
																	\
static INT32 Name##HDDName(char** pszName, UINT32 i, INT32 nAka)	\
{											   		 				\
	struct BurnHDDInfo *por = Name##PickHDD(i);						\
	if (por == NULL) {												\
		return 1;													\
	}																\
	if (nAka) {														\
		return 1;													\
	}																\
	*pszName = por->szName;											\
	return 0;														\
}
