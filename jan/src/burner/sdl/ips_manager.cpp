
#include "burner.h"

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif
// Game patching

#define UTF8_SIGNATURE	"\xef\xbb\xbf"
#define IPS_SIGNATURE	"PATCH"
#define IPS_TAG_EOF	"EOF"
#define IPS_EXT		".ips"

#define BYTE3_TO_UINT(bp) \
     (((unsigned int)(bp)[0] << 16) & 0x00FF0000) | \
     (((unsigned int)(bp)[1] << 8) & 0x0000FF00) | \
     ((unsigned int)(bp)[2] & 0x000000FF)

#define BYTE2_TO_UINT(bp) \
    (((unsigned int)(bp)[0] << 8) & 0xFF00) | \
    ((unsigned int) (bp)[1] & 0x00FF)
    
bool bDoIpsPatch = FALSE;

static void PatchFile(const char* ips_path, UINT8* base)
{
	char buf[6];
	FILE* f = NULL;
	int Offset, Size;
	UINT8* mem8 = NULL;

	if (NULL == (f = fopen(ips_path, "rb")))
		return;

	memset(buf, 0, sizeof buf);
	fread(buf, 1, 5, f);
	if (strcmp(buf, IPS_SIGNATURE)) {
		return;
	} else {
		UINT8 ch = 0;
		int bRLE = 0;
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
				mem8 = base + Offset;
				Offset++;
				*mem8 = bRLE ? ch : fgetc(f);
			}
		}
	}

	fclose(f);
}

static void DoPatchGame(const char* patch_name, char* game_name, UINT8* base)
{
	char s[MAX_PATH];
	char* p = NULL;
	char* rom_name = NULL;
	char* ips_name = NULL;
	FILE* fp = NULL;
	unsigned long nIpsSize;

	if ((fp = fopen(patch_name, "rb")) != NULL) {
		// get ips size
		fseek(fp, 0, SEEK_END);
		nIpsSize = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		while (!feof(fp)) {
			if (fgets(s, sizeof s, fp) != NULL) {
				p = s;

				// skip UTF-8 sig
				if (strncmp(p, UTF8_SIGNATURE, strlen(UTF8_SIGNATURE)) == 0)
					p += strlen(UTF8_SIGNATURE);

				if (p[0] == '[')	// '['
					break;

				rom_name = strtok(p, " \t\r\n");
				if (!rom_name)
					continue;
				if (*rom_name == '#')
					continue;
				if (_stricmp(rom_name, game_name))
					continue;

				ips_name = strtok(NULL, " \t\r\n");
				if (!ips_name)
					continue;

				// skip CRC check
				strtok(NULL, "\r\n");

				char ips_path[MAX_PATH];
				char ips_dir[MAX_PATH];
				TCHARToANSI(szAppIpsPath, ips_dir, sizeof(ips_dir));

				if (strchr(ips_name, '\\')) {
					// ips in parent's folder
					sprintf(ips_path, "%s\\%s%s", ips_dir, ips_name, IPS_EXT);
				} else {
					sprintf(ips_path, "%s%s\\%s%s", ips_dir, BurnDrvGetTextA(DRV_NAME), ips_name, IPS_EXT);
				}

				PatchFile(ips_path, base);
			}
		}
		fclose(fp);
	}
}

void IpsApplyPatches(UINT8* base, char* rom_name)
{
#if 0
	char ips_data[MAX_PATH];
	
	int nActivePatches = GetIpsNumActivePatches();
	
	for (int i = 0; i < nActivePatches; i++) {
		memset(ips_data, 0, MAX_PATH);
		TCHARToANSI(szIpsActivePatches[i], ips_data, sizeof(ips_data));
		DoPatchGame(ips_data, rom_name, base);
	}
#endif
}

void IpsPatchExit()
{
	bDoIpsPatch = FALSE;
}
