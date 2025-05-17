#include "burner.h"
#include "spng.h"

#define SSHOT_NOERROR 0
#define SSHOT_ERROR_BPP_NOTSUPPORTED 1
#define SSHOT_LIBPNG_ERROR 2
#define SSHOT_OTHER_ERROR 3

#if defined(BUILD_SDL2) && !defined(SDL_WINDOWS)
	TCHAR *SSHOT_DIRECTORY = NULL;
#else
	#define SSHOT_DIRECTORY "screenshots/"
#endif

static UINT8* pSShot = NULL;
static UINT8* pFreeMe[3] = { NULL, NULL, NULL };
static FILE* ff = NULL;

static void free_temp_imagen()
{
	// free temp memory from the conversion processeses
	for (int i = 0; i < 3; i++) {
		if (pFreeMe[i]) {
			free(pFreeMe[i]);
			pFreeMe[i] = NULL;
		}
	}
}


INT32 MakeScreenShot(INT32 bType)
{
	char szAuthor[256]; char szDescription[256]; char szCopyright[256];	char szTime[256]; char szSoftware[256]; char szSource[256];
	spng_text text_ptr[8];
	INT32 num_text = 8;

    time_t currentTime;
    tm* tmTime;

	char szSShotName[MAX_PATH] = { 0, };
    INT32 w, h;

	if (pVidImage == NULL) {
		return SSHOT_OTHER_ERROR;
	}

    if (nVidImageBPP < 2 || nVidImageBPP > 4) {
        return SSHOT_ERROR_BPP_NOTSUPPORTED;
    }

	BurnDrvGetVisibleSize(&w, &h);

	pSShot = pVidImage;

	// Convert the image to 32-bit
	if (nVidImageBPP < 4) {
		UINT8* pTemp = (UINT8*)malloc(w * h * sizeof(INT32));

		pFreeMe[0] = pTemp;

		if (nVidImageBPP == 2) {
			for (INT32 i = 0; i < h * w; i++) {
				UINT16 nColour = ((UINT16*)pSShot)[i];

				// Red
		        *(pTemp + i * 4 + 0) = (UINT8)((nColour & 0x1F) << 3);
			    *(pTemp + i * 4 + 0) |= *(pTemp + 4 * i + 0) >> 5;

				if (nVidImageDepth == 15) {
					// Green
					*(pTemp + i * 4 + 1) = (UINT8)(((nColour >> 5) & 0x1F) << 3);
					*(pTemp + i * 4 + 1) |= *(pTemp + i * 4 + 1) >> 5;
					// Blue
					*(pTemp + i * 4 + 2) = (UINT8)(((nColour >> 10)& 0x1F) << 3);
					*(pTemp + i * 4 + 2) |= *(pTemp + i * 4 + 2) >> 5;
				}

				if (nVidImageDepth == 16) {
					// Green
					*(pTemp + i * 4 + 1) = (UINT8)(((nColour >> 5) & 0x3F) << 2);
					*(pTemp + i * 4 + 1) |= *(pTemp + i * 4 + 1) >> 6;
					// Blue
					*(pTemp + i * 4 + 2) = (UINT8)(((nColour >> 11) & 0x1F) << 3);
					*(pTemp + i * 4 + 2) |= *(pTemp + i * 4 + 2) >> 5;
				}
			}
        } else {
			memset(pTemp, 0, w * h * sizeof(INT32));
			for (INT32 i = 0; i < h * w; i++) {
		        *(pTemp + i * 4 + 0) = *(pSShot + i * 3 + 0);
		        *(pTemp + i * 4 + 1) = *(pSShot + i * 3 + 1);
		        *(pTemp + i * 4 + 2) = *(pSShot + i * 3 + 2);
			}
        }

        pSShot = pTemp;
	}

	// Rotate and flip the image
	if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
		UINT8* pTemp = (UINT8*)malloc(w * h * sizeof(INT32));

		pFreeMe[1] = pTemp;

		for (INT32 x = 0; x < h; x++) {
			if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) {
				for (INT32 y = 0; y < w; y++) {
					((UINT32*)pTemp)[(w - y - 1) + x * w] = ((UINT32*)pSShot)[x + y * h];
				}
			} else {
				for (INT32 y = 0; y < w; y++) {
					((UINT32*)pTemp)[y + (h - x - 1) * w] = ((UINT32*)pSShot)[x + y * h];
				}
			}
		}

        pSShot = pTemp;
	}
	else if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) { // fixed rotation by regret
		UINT8* pTemp = (UINT8*)malloc(w * h * sizeof(INT32));

		pFreeMe[1] = pTemp;

		for (INT32 y = h - 1; y >= 0; y--) {
			for (INT32 x = w - 1; x >= 0; x--) {
				((UINT32*)pTemp)[(w - x - 1) + (h - y - 1) * w] = ((UINT32*)pSShot)[x + y * w];
			}
		}

        pSShot = pTemp;
	}

	{
		UINT8* pTemp = (UINT8*)malloc(w * h * 3); // bgrbgrbgr...

		pFreeMe[2] = pTemp;

		// convert (int*)argb to bgr as needed by libspng
		for (int i = 0; i < w * h; i++) {
			int r = pSShot[i * 4 + 0]; // (int)ARGB (little endian)
			int g = pSShot[i * 4 + 1];
			int b = pSShot[i * 4 + 2];

			pTemp[i * 3 + 0] = b;      // BGR (byte order)
			pTemp[i * 3 + 1] = g;
			pTemp[i * 3 + 2] = r;
		}

		pSShot = pTemp;
	}


	// Get the time
	time(&currentTime);
    tmTime = localtime(&currentTime);
	//png_convert_from_time_t(&png_time_now, currentTime);

#if defined(BUILD_SDL2) && !defined(SDL_WINDOWS)
	SSHOT_DIRECTORY = SDL_GetPrefPath("fbneo", "screenshots");
#endif
	
	if (bType == 0) {
		// construct our filename -> "romname-mm-dd-hms.png"
    		sprintf(szSShotName,"%s%s-%.2d-%.2d-%.2d%.2d%.2d.png", SSHOT_DIRECTORY, BurnDrvGetTextA(DRV_NAME), tmTime->tm_mon + 1, tmTime->tm_mday, tmTime->tm_hour, tmTime->tm_min, tmTime->tm_sec);
	}
	else if (bType == 1) {
		sprintf(szSShotName,"%s%s.png", _TtoA(szAppTitlesPath), BurnDrvGetTextA(DRV_NAME));
    	}
    	else {
		sprintf(szSShotName,"%s%s.png", _TtoA(szAppPreviewsPath), BurnDrvGetTextA(DRV_NAME));
	}
	//sprintf(szTime,"%.2d-%.2d-%.2d %.2d:%.2d:%.2d", tmTime->tm_mon + 1, tmTime->tm_mday, tmTime->tm_year, tmTime->tm_hour, tmTime->tm_min, tmTime->tm_sec);
	sprintf(szTime, "%s", asctime(tmTime));
#if defined(BUILD_SDL2) && !defined(SDL_WINDOWS)
	SDL_free(SSHOT_DIRECTORY);
#endif

	ff = fopen(szSShotName, "wb");
	if (ff == NULL) {
		free_temp_imagen();

		return SSHOT_OTHER_ERROR;
	}

	// Fill the PNG text fields
#ifdef _UNICODE
	sprintf(szAuthor, APP_TITLE " v%.20ls", szAppBurnVer);
#else
	sprintf(szAuthor, APP_TITLE " v%.20s", szAppBurnVer);
#endif
	sprintf(szDescription, "Screenshot of %s", DecorateGameName(nBurnDrvActive));
	sprintf(szCopyright, "%s %s", BurnDrvGetTextA(DRV_DATE), BurnDrvGetTextA(DRV_MANUFACTURER));
#ifdef _UNICODE
	sprintf(szSoftware, APP_TITLE " v%.20ls using LibSPNG v%d.%d.%d", szAppBurnVer, SPNG_VERSION_MAJOR, SPNG_VERSION_MINOR, SPNG_VERSION_PATCH);
#else
	sprintf(szSoftware, APP_TITLE " v%.20s using LibSPNG v%d.%d.%d", szAppBurnVer, SPNG_VERSION_MAJOR, SPNG_VERSION_MINOR, SPNG_VERSION_PATCH);
#endif
	sprintf(szSource, "%s video game hardware", BurnDrvGetTextA(DRV_SYSTEM));

	memset(text_ptr, 0, sizeof(text_ptr));

	strcpy(text_ptr[0].keyword, "Title");			text_ptr[0].text = BurnDrvGetTextA(DRV_FULLNAME);
	strcpy(text_ptr[1].keyword, "Author");			text_ptr[1].text = szAuthor;
	strcpy(text_ptr[2].keyword, "Description");		text_ptr[2].text = szDescription;
	strcpy(text_ptr[3].keyword, "Copyright");		text_ptr[3].text = szCopyright;
	strcpy(text_ptr[4].keyword, "Creation Time");	text_ptr[4].text = szTime;
	strcpy(text_ptr[5].keyword, "Software");		text_ptr[5].text = szSoftware;
	strcpy(text_ptr[6].keyword, "Source");			text_ptr[6].text = szSource;
	strcpy(text_ptr[7].keyword, "Comment");			text_ptr[7].text = "This screenshot was created by running the game in an emulator; it might not accurately reflect the actual hardware the game was designed to run on.";

	for (int i = 0; i < num_text; i++) {
		text_ptr[i].type = SPNG_TEXT;
		text_ptr[i].length = strlen(text_ptr[i].text);
	}

	// png it on! (dink was here)
	spng_ctx *ctx = NULL;
    struct spng_ihdr ihdr = { 0 };

    ctx = spng_ctx_new(SPNG_CTX_ENCODER);
	spng_set_png_file(ctx, ff);

	int rv = spng_set_text(ctx, &text_ptr[0], num_text);
	if (rv) {
		bprintf(0, _T("spng_set_text() error: %d / %S\n"), rv, spng_strerror(rv));
	}

	ihdr.width = w;
    ihdr.height = h;
    ihdr.color_type = SPNG_COLOR_TYPE_TRUECOLOR;
	ihdr.bit_depth = 8;
	spng_set_ihdr(ctx, &ihdr);

	rv = spng_encode_image(ctx, pSShot, w * h * 3, SPNG_FMT_PNG, SPNG_ENCODE_FINALIZE);

	if (rv) {
		bprintf(0, _T("spng_encode_image() error: %d / %S\n"), rv, spng_strerror(rv));
	}

	spng_ctx_free(ctx);

	fclose(ff);

	free_temp_imagen();

	return SSHOT_NOERROR;
}
