// OpenGL via SDL2
#include "burner.h"
#include "vid_support.h"
#include "vid_softfx.h"

#include <SDL.h>
#include <SDL_opengl.h>

extern char videofiltering[3];

static SDL_GLContext glContext = NULL;

static int nInitedSubsytems = 0;

static int nGamesWidth = 0, nGamesHeight = 0; // screen size

static GLint texture_type = GL_UNSIGNED_BYTE;
extern SDL_Window* sdlWindow;
static unsigned char* texture = NULL;
static unsigned char* gamescreen = NULL;

static int nTextureWidth = 512;
static int nTextureHeight = 512;

static int nSize;
static int nUseBlitter;

static int nRotateGame = 0;
static bool bFlipped = false;

static char Windowtitle[512];

static int BlitFXExit()
{
	free(texture);
	free(gamescreen);

	SDL_DestroyWindow(sdlWindow);
	sdlWindow = NULL;
	
	SDL_GL_DeleteContext(glContext);
	glContext = NULL;
	
	nRotateGame = 0;

	return 0;
}
static int GetTextureSize(int Size)
{
	int nTextureSize = 128;

	while (nTextureSize < Size)
	{
		nTextureSize <<= 1;
	}

	return nTextureSize;
}

static int BlitFXInit()
{
	int nMemLen = 0;

	nVidImageDepth = bDrvOkay ? 16 : 32;
	nVidImageBPP = (nVidImageDepth + 7) >> 3;
	nBurnBpp = nVidImageBPP;

	SetBurnHighCol(nVidImageDepth);

	if (!nRotateGame)
	{
		nVidImageWidth = nGamesWidth;
		nVidImageHeight = nGamesHeight;
	}
	else
	{
		nVidImageWidth = nGamesHeight;
		nVidImageHeight = nGamesWidth;
	}

	nVidImagePitch = nVidImageWidth * nVidImageBPP;
	nBurnPitch = nVidImagePitch;

	nMemLen = nVidImageWidth * nVidImageHeight * nVidImageBPP;

#ifdef FBNEO_DEBUG
	printf("nVidImageWidth=%d nVidImageHeight=%d nVidImagePitch=%d\n", nVidImageWidth, nVidImageHeight, nVidImagePitch);
	printf("nTextureWidth=%d nTextureHeight=%d TexturePitch=%d\n", nTextureWidth, nTextureHeight, nTextureWidth * nVidImageBPP);
#endif

	texture = (unsigned char*)malloc(nTextureWidth * nTextureHeight * nVidImageBPP);

	gamescreen = (unsigned char*)malloc(nMemLen);
	if (gamescreen)
	{
		memset(gamescreen, 0, nMemLen);
		pVidImage = gamescreen;
		return 0;
	}
	else
	{
		pVidImage = NULL;
		return 1;
	}

	return 0;
}

static int Exit()
{
	BlitFXExit();

	if (!(nInitedSubsytems & SDL_INIT_VIDEO))
	{
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}

	nInitedSubsytems = 0;

	return 0;
}

void init_gl()
{
#ifdef FBNEO_DEBUG
	printf("opengl config\n");
#endif
	if ((BurnDrvGetFlags() & BDF_16BIT_ONLY) || (nVidImageBPP != 3))
	{
		texture_type = GL_UNSIGNED_SHORT_5_6_5;
	}
	else
	{
		texture_type = GL_UNSIGNED_BYTE;
	}

	glShadeModel(GL_FLAT);
	glDisable(GL_POLYGON_SMOOTH);
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_POINT_SMOOTH);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	if (strcmp(videofiltering, "0") == 0) 
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, nTextureWidth, nTextureHeight, 0, GL_RGB, texture_type, texture);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	if (!nRotateGame)
	{
		glRotatef((bFlipped ? 180.0 : 0.0), 0.0, 0.0, 1.0);
		glOrtho(0, nGamesWidth, nGamesHeight, 0, -1, 1);
	}
	else
	{
		glRotatef((bFlipped ? 270.0 : 90.0), 0.0, 0.0, 1.0);
		glOrtho(0, nGamesHeight, nGamesWidth, 0, -1, 1);
	}

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	
#ifdef FBNEO_DEBUG
	printf("opengl config done . . . \n");
#endif
}

static int VidSScaleImage(RECT* pRect)
{
	int nScrnWidth, nScrnHeight;

	int nGameAspectX = 4, nGameAspectY = 3;
	int nWidth = pRect->right - pRect->left;
	int nHeight = pRect->bottom - pRect->top;

	if (bVidFullStretch)
	{ // Arbitrary stretch
		return 0;
	}

	if (bDrvOkay)
	{
		if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)
		{
			BurnDrvGetAspect(&nGameAspectY, &nGameAspectX);
		}
		else
		{
			BurnDrvGetAspect(&nGameAspectX, &nGameAspectY);
		}
	}

	nScrnWidth = nGameAspectX;
	nScrnHeight = nGameAspectY;

	int nWidthScratch = nHeight * nVidScrnAspectY * nGameAspectX * nScrnWidth /
		(nScrnHeight * nVidScrnAspectX * nGameAspectY);

	if (nWidthScratch > nWidth)
	{ // The image is too wide
		if (nGamesWidth < nGamesHeight)
		{ // Vertical games
			nHeight = nWidth * nVidScrnAspectY * nGameAspectY * nScrnWidth /
				(nScrnHeight * nVidScrnAspectX * nGameAspectX);
		}
		else
		{ // Horizontal games
			nHeight = nWidth * nVidScrnAspectX * nGameAspectY * nScrnHeight /
				(nScrnWidth * nVidScrnAspectY * nGameAspectX);
		}
	}
	else
	{
		nWidth = nWidthScratch;
	}

	pRect->left = (pRect->right + pRect->left) / 2;
	pRect->left -= nWidth / 2;
	pRect->right = pRect->left + nWidth;

	pRect->top = (pRect->top + pRect->bottom) / 2;
	pRect->top -= nHeight / 2;
	pRect->bottom = pRect->top + nHeight;

	return 0;
}

static int Init()
{
	nInitedSubsytems = SDL_WasInit(SDL_INIT_VIDEO);

	if (!(nInitedSubsytems & SDL_INIT_VIDEO))
	{
		SDL_InitSubSystem(SDL_INIT_VIDEO);
	}

	nGamesWidth = nVidImageWidth;
	nGamesHeight = nVidImageHeight;

	nRotateGame = 0;

	if (bDrvOkay) {
		// Get the game screen size
		BurnDrvGetVisibleSize(&nGamesWidth, &nGamesHeight);

		if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)
		{
#ifdef FBNEO_DEBUG
			printf("Vertical\n");
#endif
			nRotateGame = 1;
		}

		if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED)
		{
#ifdef FBNEO_DEBUG
			printf("Flipped\n");
#endif
			bFlipped = true;
		}
	}

	if (!nRotateGame)
	{
		nTextureWidth = GetTextureSize(nGamesWidth);
		nTextureHeight = GetTextureSize(nGamesHeight);
	}
	else
	{
		nTextureWidth = GetTextureSize(nGamesHeight);
		nTextureHeight = GetTextureSize(nGamesWidth);
	}

	nSize = 2;
	bVidScanlines = 0;

	RECT test_rect;
	test_rect.left = 0;
	test_rect.right = nGamesWidth;
	test_rect.top = 0;
	test_rect.bottom = nGamesHeight;
	
#ifdef FBNEO_DEBUG
	printf("correctx before %d, %d\n", test_rect.right, test_rect.bottom);
#endif
	VidSScaleImage(&test_rect);
#ifdef FBNEO_DEBUG
	printf("correctx after %d, %d\n", test_rect.right, test_rect.bottom);
#endif

	Uint32 screenFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

	if (bAppFullscreen)
	{
		screenFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN;
	}

	sprintf(Windowtitle, "FBNeo - %s - %s", BurnDrvGetTextA(DRV_NAME), BurnDrvGetTextA(DRV_FULLNAME));
	
	sdlWindow = SDL_CreateWindow(Windowtitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, test_rect.right * nSize,
		test_rect.bottom * nSize, screenFlags);
	if (sdlWindow == NULL) {
		printf("OPENGL failed : %s\n", SDL_GetError());
		return -1;		
	}
	glContext = SDL_GL_CreateContext(sdlWindow);
	if (glContext == NULL) {
		printf("OPENGL failed : %s\n", SDL_GetError());
		return -1;		
	}
	
	// Initialize the buffer surfaces
	BlitFXInit();

	// Init opengl
	init_gl();

	return 0;
}

// Run one frame and render the screen
static int Frame(bool bRedraw) // bRedraw = 0
{
	if (pVidImage == NULL)
	{
		return 1;
	}

	VidFrameCallback(bRedraw);

	return 0;
}

static void SurfToTex()
{
	int nVidPitch = nTextureWidth * nVidImageBPP;

	unsigned char* ps = (unsigned char*)gamescreen;
	unsigned char* pd = (unsigned char*)texture;

	for (int y = nVidImageHeight; y--;)
	{
		memcpy(pd, ps, nVidImagePitch);
		pd += nVidPitch;
		ps += nVidImagePitch;
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, nTextureWidth, nTextureHeight, 0,
		GL_RGB, texture_type, texture);
}

static void TexToQuad()
{
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2i(0, 0);
	glTexCoord2f(0, 1);
	glVertex2i(0, nTextureHeight);
	glTexCoord2f(1, 1);
	glVertex2i(nTextureWidth, nTextureHeight);
	glTexCoord2f(1, 0);
	glVertex2i(nTextureWidth, 0);
	glEnd();
	glFinish();
}

// Paint the BlitFX surface onto the primary surface
static int Paint(int bValidate)
{

	SurfToTex();
	TexToQuad();
	
	if (bAppShowFPS && !bAppFullscreen)
	{
		sprintf(Windowtitle, "FBNeo - FPS: %s - %s - %s", fpsstring, BurnDrvGetTextA(DRV_NAME), BurnDrvGetTextA(DRV_FULLNAME));
		SDL_SetWindowTitle(sdlWindow, Windowtitle);
	}
	SDL_GL_SwapWindow(sdlWindow);


	return 0;
}

static int vidScale(RECT*, int, int)
{
	return 0;
}


static int GetSettings(InterfaceInfo* pInfo)
{
	TCHAR szString[MAX_PATH] = _T("");

	//_sntprintf(szString, MAX_PATH, _T("Prescaling using %s (%i� zoom)"), VidSoftFXGetEffect(nUseBlitter), nSize);
	//IntInfoAddStringModule(pInfo, szString);

	if (nRotateGame)
	{
		IntInfoAddStringModule(pInfo, _T("Using software rotation"));
	}

	return 0;
}

// The Video Output plugin:
struct VidOut VidOutSDL2Opengl = { Init, Exit, Frame, Paint, vidScale, GetSettings, _T("SDL OpenGL Video output") };
