#ifndef _neocdlist_
#define _neocdlist_

struct NGCDGAME
{
	TCHAR* pszName;			// Short name
	TCHAR* pszTitle;		// Title
	TCHAR* pszYear;			// Release Year
	TCHAR* pszCompany;		// Developer
	UINT32 id;				// Game ID
};

NGCDGAME* GetNeoGeoCDInfo(UINT32 nID);
INT32  GetNeoCDTitle(UINT32 nGameID);
INT32  GetNeoGeoCD_Identifier();

#ifdef BUILD_NEOGEO
bool   IsNeoGeoCD();		// neo_run.cpp
TCHAR* GetIsoPath();		// cd_isowav.cpp

INT32  NeoCDInfo_Init();
TCHAR* NeoCDInfo_Text(INT32 nText);
INT32  NeoCDInfo_ID();
void   NeoCDInfo_SetTitle();
void   NeoCDInfo_Exit();
#else
static inline bool IsNeoGeoCD() { return false; }
static inline TCHAR* GetIsoPath() { return NULL; }
static inline int NeoCDInfo_Init() { return 0; }
static inline TCHAR* NeoCDInfo_Text(int) { return NULL; }
static inline int NeoCDInfo_ID() { return 0; }
static inline void NeoCDInfo_SetTitle() { }
static inline void NeoCDInfo_Exit() { }
#endif

// ------------------------------------------------------
// ISO9660 STUFF

// ISO9660 date and time
struct iso9660_date
{
	UINT8 Year;				// Number of year since 1900
	UINT8 Month;			// Month (1 to 12)
	UINT8 Day;				// Day (1 to 31)
	UINT8 Hour;				// Hour (0 to 23)
	UINT8 Minute;			// Minute (0 to 59)
	UINT8 Second;			// Second (0 to 59)
	UINT8 Zone;				// Offset related to GMT, by 15-min INT32erval (from -48 to +52)
};

// ISO9660 BCD date and time
struct iso9660_bcd_date
{
	UINT8 Year[4];
    UINT8 Month[2];
    UINT8 Day[2];
    UINT8 Hour[2];
    UINT8 Minute[2];
    UINT8 Second[2];
	UINT8 SecFrac[2];
    char Zone;
};

struct iso9660_DirectoryRecord
{
    UINT8 len_dr;					// [1] Length of Directory Record (bytes)
    UINT8 ext_attr_rec_len;			// [1] Extended Attribute Record Length (bytes)
    UINT8 location_of_extent[8];	// [8] [LEF / BEF] LBN / Sector location of the file data
    UINT8 data_lenth[8];			// [8] [LEF / BEF] Length of the file section (bytes)
	iso9660_date rec_date_time;		// [7] Recording Date and Time
	UINT8 file_flags;				// [1] 8-bit flags
											// [bit 0] File is Hidden if this bit is 1
											// [bit 1] Entry is a Directory if this bit is 1
											// [bit 2] Entry is an Associated file is this bit is 1
											// [bit 3] Information is structured according to the extended attribute record if this bit is 1
											// [bit 4] Owner, group and permissions are specified in the extended attribute record if this bit is 1
											// [bit 5] Reserved (0)
											// [bit 6] Reserved (0)
											// [bit 7] File has more than one directory record if this bit is 1
	UINT8 file_unit_size;			// [1] This field is only valid if the file is recorded in INT32erleave mode, otherwise this field is (00)
    UINT8 INT32_gap_size;			// [1] This field is only valid if the file is recorded in INT32erleave mode, otherwise this field is (00)
   	UINT8 vol_seq_number[4];		// [4] The ordinal number of the volume in the Volume Set on which the file described by the directory record is recorded.
    UINT8 len_fi;					// [1] Length of File Identifier (LEN_FI)
    char* file_id;					// [LEN_FI] File Identifier
};

struct iso9660_PathTableRecord
{
	UINT8 len_di;					// [1] Length of Directory Identifier
	UINT8 extended_attr_len;		// [1] Extended Attribute Record Length
	UINT8 extent_location[4];		// [4] Extent Location (Sector)
	UINT8 parent_dir_num;			// [1] Parent Directory Number
	char* directory_id;				// [LEN_DI] Directory Identifier
};

// Volume Descriptor Header (Sector 16)
struct iso9660_VDH
{
	char vdtype;					// [1] Volume Descriptor Type
	UINT8 stdid[5];					// [5] ISO9660 Standard Identifier
	char vdver;						// [1] Volume Descriptor Version
};

// Primary Volume Descriptor (Volume Descriptor Type 1)
struct iso9660_PVD
{
	// -----------------------------
	// Volume Descriptor Header
	char type;
	UINT8 id[5];
	char version;
	// -----------------------------
	// Primary Volume Descriptor
	char unused1;
	UINT8 system_id[32];
	UINT8 volume_id[32];
	UINT8 unused2[8];
	UINT8 volume_space_size[8];
	UINT8 unused3[32];
	UINT8 volume_set_size[4];
	UINT8 volume_sequence_number[4];
	UINT8 logical_block_size[4];
	UINT8 path_table_size[8];
	UINT8 type_l_path_table[4];
	UINT8 opt_type_l_path_table[4];
	UINT8 type_m_path_table[4];
	UINT8 opt_type_m_path_table[4];
	iso9660_DirectoryRecord root_directory_record;
	UINT8 volume_set_id[128];
	UINT8 publisher_id[128];
	UINT8 preparer_id[128];
	UINT8 application_id[128];
	UINT8 copyright_file_id[37];
	UINT8 abstract_file_id[37];
	UINT8 bibliographic_file_id[37];

	iso9660_bcd_date creation_date;		// [17]
	iso9660_bcd_date modification_date;	// [17]
	iso9660_bcd_date expiration_date;	// [17]
	iso9660_bcd_date effective_date;	// [17]

	char file_structure_version;
	char unused4;
	UINT8 application_data[512];
	UINT8 unused5[643];
};

void iso9660_ReadOffset(UINT8 *Dest, FILE* fp, UINT32 lOffset, UINT32 lSize, UINT32 lLength);

// ------------------------------------------------------


#endif
