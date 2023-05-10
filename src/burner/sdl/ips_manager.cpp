#include "burner.h"

// Game patching

#define UTF8_SIGNATURE    "\xef\xbb\xbf"
#define IPS_SIGNATURE     "PATCH"
#define IPS_TAG_EOF       "EOF"
#define IPS_EXT           ".ips"

#define BYTE3_TO_UINT(bp)                         \
   (((unsigned int)(bp)[0] << 16) & 0x00FF0000) | \
   (((unsigned int)(bp)[1] <<  8) & 0x0000FF00) | \
   (( unsigned int)(bp)[2] & 0x000000FF)

#define BYTE2_TO_UINT(bp)                    \
   (((unsigned int)(bp)[0] << 8) & 0xFF00) | \
   (( unsigned int)(bp)[1]       & 0x00FF)

bool bDoIpsPatch = false;

unsigned int nIpsDrvDefine = 0, nIpsMemExpLen[SND2_ROM + 1] = { 0 };

static void PatchFile(const char* ips_path, UINT8* base, bool readonly)
{
#if 0
	char buf[6];
	FILE* f = NULL;
	INT32 Offset = 0, Size = 0;
	UINT8* mem8 = NULL;

	if (NULL == (f = fopen(ips_path, "rb"))) {
		bprintf(0, _T("IPS - Can't open file %S!  Aborting.\n"), ips_path);
		return;
	}

	memset(buf, 0, sizeof(buf));
	fread(buf, 1, 5, f);
	if (strcmp(buf, IPS_SIGNATURE)) {
		bprintf(0, _T("IPS - Bad IPS-Signature in: %S.\n"), ips_path);
		if (f)
		{
			fclose(f);
		}
		return;
	}
	else {
		bprintf(0, _T("IPS - Patching with: %S.\n"), ips_path);
		UINT8 ch = 0;
		INT32 bRLE = 0;
		while (!feof(f)) {
			// read patch address offset
			fread(buf, 1, 3, f);
			buf[3] = 0;
			if (strcmp(buf, IPS_TAG_EOF) == 0)
				break;

			Offset = BYTE3_TO_UINT(buf);

			// read patch length
			fread(buf, 1, 2, f);
			Size = BYTE2_TO_UINT(buf);

			bRLE = (Size == 0);
			if (bRLE) {
				fread(buf, 1, 2, f);
				Size = BYTE2_TO_UINT(buf);
				ch = fgetc(f);
			}

			while (Size--) {
				if (!readonly) mem8 = base + Offset + nRomOffset;
				Offset++;
				if (readonly) {
					if (!bRLE) fgetc(f);
				}
				else {
					*mem8 = bRLE ? ch : fgetc(f);
				}
			}
		}
	}

	// When in the read-only state, the only thing is to get nIpsMemExpLen[LOAD_ROM], thus avoiding memory out-of-bounds.
	if (readonly && (Offset > nIpsMemExpLen[LOAD_ROM])) nIpsMemExpLen[LOAD_ROM] = Offset; // file size is growing

	fclose(f);
#endif
}

static void DoPatchGame(const char* patch_name, char* game_name, UINT8* base, bool readonly)
{
#if 0
	char s[MAX_PATH];
	char* p = NULL;
	char* rom_name = NULL;
	char* ips_name = NULL;
	char* ips_offs = NULL;
	FILE* fp = NULL;
	unsigned long nIpsSize;

	if ((fp = fopen(patch_name, "rb")) != NULL) {
		// get ips size
		fseek(fp, 0, SEEK_END);
		nIpsSize = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		while (!feof(fp)) {
			if (fgets(s, sizeof(s), fp) != NULL) {
				p = s;

				// skip UTF-8 sig
				if (strncmp(p, UTF8_SIGNATURE, strlen(UTF8_SIGNATURE)) == 0)
					p += strlen(UTF8_SIGNATURE);

				if (p[0] == '[')	// '['
					break;

				// Can support linetypes:
				// "rom name.bin" "patch file.ips" CRC(abcd1234)
				// romname.bin patchfile CRC(abcd1234)

				if (p[0] == '\"') { // "quoted rom name with spaces.bin"
					p++;
					rom_name = strtok(p, "\"");
				}
				else {
					rom_name = strtok(p, " \t\r\n");
				}
				if (!rom_name)
					continue;
				if (*rom_name == '#')
					continue;
				if (_stricmp(rom_name, game_name))
					continue;

				ips_name = strtok(NULL, "\"\t\r\n");
				//				if (p[0] == '\"') { // "quoted ips name with spaces.ips"
				//					p++;
				//					ips_name = strtok(NULL, "\"");
				//				} else {
				//					ips_name = strtok(NULL, "\t\r\n");
				//				}
				if (!ips_name)
					continue;

				nRomOffset = 0; // Reset to 0
				if (NULL != (ips_offs = strtok(NULL, " \t\r\n"))) {	// Parameters of the offset increment
					if (0 == strcmp(ips_offs, "IPS_OFFSET_016")) nRomOffset = 0x1000000;
					else if (0 == strcmp(ips_offs, "IPS_OFFSET_032")) nRomOffset = 0x2000000;
					else if (0 == strcmp(ips_offs, "IPS_OFFSET_048")) nRomOffset = 0x3000000;
					else if (0 == strcmp(ips_offs, "IPS_OFFSET_064")) nRomOffset = 0x4000000;
					else if (0 == strcmp(ips_offs, "IPS_OFFSET_080")) nRomOffset = 0x5000000;
					else if (0 == strcmp(ips_offs, "IPS_OFFSET_096")) nRomOffset = 0x6000000;
					else if (0 == strcmp(ips_offs, "IPS_OFFSET_112")) nRomOffset = 0x7000000;
					else if (0 == strcmp(ips_offs, "IPS_OFFSET_128")) nRomOffset = 0x8000000;
					else if (0 == strcmp(ips_offs, "IPS_OFFSET_144")) nRomOffset = 0x9000000;
				}

				// remove crc portion, and end quote/spaces from ips name
				char* c = stristr_int(ips_name, "crc");
				if (c) {
					c--; // "derp.ips" CRC(abcd1234)\n"
					//           ^ we're now here.
					while (*c && (*c == ' ' || *c == '\t' || *c == '\"'))
					{
						*c = '\0';
						c--;
					}
				}

				// clean-up IPS name beginning (could be quoted or not)
				while (ips_name && (ips_name[0] == '\t' || ips_name[0] == ' ' || ips_name[0] == '\"'))
					ips_name++;

				char* has_ext = stristr_int(ips_name, ".ips");

				bprintf(0, _T("ips name:[%S]\n"), ips_name);
				bprintf(0, _T("rom name:[%S]\n"), rom_name);

				char ips_path[MAX_PATH * 2];
				char ips_dir[MAX_PATH];
				TCHARToANSI(szAppIpsPath, ips_dir, sizeof(ips_dir));

				if (strchr(ips_name, '\\')) {
					// ips in parent's folder
					sprintf(ips_path, "%s\\%s%s", ips_dir, ips_name, (has_ext) ? "" : IPS_EXT);
				}
				else {
					sprintf(ips_path, "%s%s\\%s%s", ips_dir, BurnDrvGetTextA(DRV_NAME), ips_name, (has_ext) ? "" : IPS_EXT);
				}

				PatchFile(ips_path, base, readonly);
			}
		}
		fclose(fp);
	}
#endif
}

void GetIpsDrvDefine()
{
	if (!bDoIpsPatch)
		return;

	nIpsDrvDefine = 0;
	memset(nIpsMemExpLen, 0, sizeof(nIpsMemExpLen));

#if 0

	char ips_data[MAX_PATH];
	INT32 nActivePatches = GetIpsNumActivePatches();

	for (INT32 i = 0; i < nActivePatches; i++) {
		memset(ips_data, 0, MAX_PATH);
		TCHARToANSI(szIpsActivePatches[i], ips_data, sizeof(ips_data));

		char str[MAX_PATH] = { 0 }, * ptr = NULL, * tmp = NULL;
		FILE* fp = NULL;

		if (NULL != (fp = fopen(ips_data, "rb"))) {
			while (!feof(fp)) {
				if (NULL != fgets(str, sizeof(str), fp)) {
					ptr = str;

					// skip UTF-8 sig
					if (0 == strncmp(ptr, UTF8_SIGNATURE, strlen(UTF8_SIGNATURE)))
						ptr += strlen(UTF8_SIGNATURE);

					if (NULL == (tmp = strtok(ptr, " \t\r\n")))
						continue;
					if (0 != strcmp(tmp, "#define"))
						break;
					if (NULL == (tmp = strtok(NULL, " \t\r\n")))
						break;

					UINT32 nNewValue = 0;

					if (0 == strcmp(tmp, "IPS_NOT_PROTECT")) {
						nIpsDrvDefine |= IPS_NOT_PROTECT;
						continue;
					}
					if (0 == strcmp(tmp, "IPS_PGM_SPRHACK")) {
						nIpsDrvDefine |= IPS_PGM_SPRHACK;
						continue;
					}
					if (0 == strcmp(tmp, "IPS_PGM_MAPHACK")) {
						nIpsDrvDefine |= IPS_PGM_MAPHACK;
						continue;
					}
					if (0 == strcmp(tmp, "IPS_PGM_SNDOFFS")) {
						nIpsDrvDefine |= IPS_PGM_SNDOFFS;
						continue;
					}
					if (0 == strcmp(tmp, "IPS_LOAD_EXPAND")) {
						nIpsDrvDefine |= IPS_LOAD_EXPAND;
						nIpsMemExpLen[EXP_FLAG] = 1;
						if ((nNewValue = GetIpsDefineExpValue(tmp)) > nIpsMemExpLen[LOAD_ROM])
							nIpsMemExpLen[LOAD_ROM] = nNewValue;
						continue;
					}
					if (0 == strcmp(tmp, "IPS_EXTROM_INCL")) {
						nIpsDrvDefine |= IPS_EXTROM_INCL;
						if ((nNewValue = GetIpsDefineExpValue(tmp)) > nIpsMemExpLen[EXTR_ROM])
							nIpsMemExpLen[EXTR_ROM] = nNewValue;
						continue;
					}
					if (0 == strcmp(tmp, "IPS_PRG1_EXPAND")) {
						nIpsDrvDefine |= IPS_PRG1_EXPAND;
						if ((nNewValue = GetIpsDefineExpValue(tmp)) > nIpsMemExpLen[PRG1_ROM])
							nIpsMemExpLen[PRG1_ROM] = nNewValue;
						continue;
					}
					if (0 == strcmp(tmp, "IPS_PRG2_EXPAND")) {
						nIpsDrvDefine |= IPS_PRG2_EXPAND;
						if ((nNewValue = GetIpsDefineExpValue(tmp)) > nIpsMemExpLen[PRG2_ROM])
							nIpsMemExpLen[PRG2_ROM] = nNewValue;
						continue;
					}
					if (0 == strcmp(tmp, "IPS_GRA1_EXPAND")) {
						nIpsDrvDefine |= IPS_GRA1_EXPAND;
						if ((nNewValue = GetIpsDefineExpValue(tmp)) > nIpsMemExpLen[GRA1_ROM])
							nIpsMemExpLen[GRA1_ROM] = nNewValue;
						continue;
					}
					if (0 == strcmp(tmp, "IPS_GRA2_EXPAND")) {
						nIpsDrvDefine |= IPS_GRA2_EXPAND;
						if ((nNewValue = GetIpsDefineExpValue(tmp)) > nIpsMemExpLen[GRA2_ROM])
							nIpsMemExpLen[GRA2_ROM] = nNewValue;
						continue;
					}
					if (0 == strcmp(tmp, "IPS_GRA3_EXPAND")) {
						nIpsDrvDefine |= IPS_GRA3_EXPAND;
						if ((nNewValue = GetIpsDefineExpValue(tmp)) > nIpsMemExpLen[GRA3_ROM])
							nIpsMemExpLen[GRA3_ROM] = nNewValue;
						continue;
}
					if (0 == strcmp(tmp, "IPS_ACPU_EXPAND")) {
						nIpsDrvDefine |= IPS_ACPU_EXPAND;
						if ((nNewValue = GetIpsDefineExpValue(tmp)) > nIpsMemExpLen[ACPU_ROM])
							nIpsMemExpLen[ACPU_ROM] = nNewValue;
						continue;
					}
					if (0 == strcmp(tmp, "IPS_SND1_EXPAND")) {
						nIpsDrvDefine |= IPS_SND1_EXPAND;
						if ((nNewValue = GetIpsDefineExpValue(tmp)) > nIpsMemExpLen[SND1_ROM])
							nIpsMemExpLen[SND1_ROM] = nNewValue;
						continue;
					}
					if (0 == strcmp(tmp, "IPS_SND2_EXPAND")) {
						nIpsDrvDefine |= IPS_SND2_EXPAND;
						if ((nNewValue = GetIpsDefineExpValue(tmp)) > nIpsMemExpLen[SND2_ROM])
							nIpsMemExpLen[SND2_ROM] = nNewValue;
						continue;
					}
				}
			}
			fclose(fp);
		}
	}
#endif
}

void IpsApplyPatches(UINT8* base, char* rom_name, UINT32 crc, bool readonly)
{
	if (!bDoIpsPatch)
		return;

#if 0
	char ips_data[MAX_PATH];

	INT32 nActivePatches = GetIpsNumActivePatches();

	for (INT32 i = 0; i < nActivePatches; i++) {
		memset(ips_data, 0, MAX_PATH);
		TCHARToANSI(szIpsActivePatches[i], ips_data, sizeof(ips_data));
		DoPatchGame(ips_data, rom_name, base, readonly);
	}
#endif
}

void IpsPatchExit()
{
	memset(nIpsMemExpLen, 0, sizeof(nIpsMemExpLen));

	nIpsDrvDefine = 0;
	bDoIpsPatch = false;
}
