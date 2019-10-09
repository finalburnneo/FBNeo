// Burner Config file module
#include "burner.h"

int piLoadConfig()
{
	FILE *f;
	if ((f = fopen("fbneo.ini","r")) == NULL) {
		return 1;
	}

#define VAR(x) { char *szValue = LabelCheck(line, #x);		\
  if (szValue) x = strtol(szValue, NULL, 0); }
#define FLT(x) { char *szValue = LabelCheck(line, #x);		\
  if (szValue) x = atof(szValue); }
#define STR(x) { char *szValue = LabelCheck(line, #x " ");	\
  if (szValue) strcpy(x,szValue); }

	char line[256];
	while (fgets(line, sizeof(line), f)) {
		// Get rid of the linefeed at the end
		int len = strlen(line);
		if (line[len - 1] == 10) {
			line[len - 1] = 0;
			len--;
		}

		VAR(bVidScanlines);

		// Other
		STR(szAppRomPaths[0]);
		STR(szAppRomPaths[1]);
		STR(szAppRomPaths[2]);
		STR(szAppRomPaths[3]);
		STR(szAppRomPaths[4]);
		STR(szAppRomPaths[5]);
		STR(szAppRomPaths[6]);
		STR(szAppRomPaths[7]);
		STR(szAppRomPaths[8]);
		STR(szAppRomPaths[9]);
		STR(szAppRomPaths[10]);
		STR(szAppRomPaths[11]);
		STR(szAppRomPaths[12]);
		STR(szAppRomPaths[13]);
		STR(szAppRomPaths[14]);
		STR(szAppRomPaths[15]);
		STR(szAppRomPaths[16]);
		STR(szAppRomPaths[17]);
		STR(szAppRomPaths[18]);
		STR(szAppRomPaths[19]);
		VAR(nAudSampleRate[0]);
	}

#undef STR
#undef FLT
#undef VAR

	fclose(f);
	return 0;
}

int piSaveConfig()
{
	FILE *f;
	if ((f = fopen("fbapi.ini", "w")) == NULL) {
		return 1;
	}


#define VAR(x) fprintf(f, #x " %d\n", x)
#define FLT(x) fprintf(f, #x " %f\n", x)
#define STR(x) fprintf(f, #x " %s\n", x)

	fprintf(f,"\n// If non-zero, enable scanlines\n");
	VAR(bVidScanlines);

	fprintf(f,"// ROM paths (include trailing slash)\n");
	STR(szAppRomPaths[0]);
	STR(szAppRomPaths[1]);
	STR(szAppRomPaths[2]);
	STR(szAppRomPaths[3]);
	STR(szAppRomPaths[4]);
	STR(szAppRomPaths[5]);
	STR(szAppRomPaths[6]);
	STR(szAppRomPaths[7]);
	STR(szAppRomPaths[8]);
	STR(szAppRomPaths[9]);
	STR(szAppRomPaths[10]);
	STR(szAppRomPaths[11]);
	STR(szAppRomPaths[12]);
	STR(szAppRomPaths[13]);
	STR(szAppRomPaths[14]);
	STR(szAppRomPaths[15]);
	STR(szAppRomPaths[16]);
	STR(szAppRomPaths[17]);
	STR(szAppRomPaths[18]);
	STR(szAppRomPaths[19]);
	VAR(nAudSampleRate[0]);

#undef STR
#undef FLT
#undef VAR

	fclose(f);
	return 0;
}
