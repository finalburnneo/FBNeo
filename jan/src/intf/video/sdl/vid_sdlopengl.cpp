// OpenGL via SDL
#include "burner.h"
#include "vid_support.h"
#include "vid_softfx.h"

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>

#ifdef frame_timer
#include <sys/time.h>
#endif

static int nInitedSubsytems = 0;

static int nGamesWidth = 0, nGamesHeight = 0; // screen size

static SDL_Surface *screen=NULL;
static unsigned char *texture = NULL;
static unsigned char *gamescreen=NULL;

static GLint color_type = GL_RGB;
static GLint texture_type = GL_UNSIGNED_BYTE;

static int nTextureWidth = 512;
static int nTextureHeight = 512;

static int nSize;
static int nUseBlitter;

static int nRotateGame = 0;
static bool bFlipped = false;

static int PrimClear()
{
	return 0;
}

// Create a secondary DD surface for the screen
static int BlitFXMakeSurf()
{
	return 0;
}

static int BlitFXExit()
{
	SDL_FreeSurface(screen);

	free(texture);
	free(gamescreen);
	nRotateGame = 0;

	return 0;
}
static int GetTextureSize(int Size)
{
	int nTextureSize = 128;

	while (nTextureSize < Size) {
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

	if (!nRotateGame) {
		nVidImageWidth = nGamesWidth;
		nVidImageHeight = nGamesHeight;
	} else {
		nVidImageWidth = nGamesHeight;
		nVidImageHeight = nGamesWidth;
	}

	nVidImagePitch = nVidImageWidth * nVidImageBPP;
	nBurnPitch = nVidImagePitch;

	nMemLen = nVidImageWidth * nVidImageHeight * nVidImageBPP;

	printf("nVidImageWidth=%d nVidImageHeight=%d nVidImagePitch=%d\n",
		nVidImageWidth, nVidImageHeight, nVidImagePitch);
	printf("nTextureWidth=%d nTextureHeight=%d TexturePitch=%d\n",
		nTextureWidth, nTextureHeight, nTextureWidth * nVidImageBPP);

	texture = (unsigned char *)malloc(nTextureWidth * nTextureHeight * nVidImageBPP);

	gamescreen = (unsigned char *)malloc(nMemLen);
	if (gamescreen) {
		memset(gamescreen, 0, nMemLen);
		pVidImage = gamescreen;
		return 0;
	} else {
		pVidImage = NULL;
		return 1;
	}

	return 0;
}

static int Exit()
{
	BlitFXExit();

	if (!(nInitedSubsytems & SDL_INIT_VIDEO)) {
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}

	nInitedSubsytems = 0;

	return 0;
}

void init_gl()
{
	const unsigned char *glVersion;
	int isGL12 = GL_FALSE;

	printf("opengl config\n");

	if ((BurnDrvGetFlags() & BDF_16BIT_ONLY) || (nVidImageBPP != 3)) {
		texture_type = GL_UNSIGNED_SHORT_5_6_5;
	} else {
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
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, nTextureWidth, nTextureHeight,
		     0, GL_RGB, texture_type, texture);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	if (!nRotateGame) {
		glRotatef(0.0, 0.0, 0.0, 1.0);
	 	glOrtho(0, nGamesWidth, nGamesHeight, 0, -1, 1);
	} else {
		glRotatef((bFlipped ? 270.0 : 90.0), 0.0, 0.0, 1.0);
		glOrtho(0, nGamesHeight, nGamesWidth, 0, -1, 1);
	}

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	printf("opengl config done . . . \n");
}

int VidSScaleImage(RECT* pRect)
{
	int nScrnWidth, nScrnHeight;

	int nGameAspectX = 4, nGameAspectY = 3;
	int nWidth = pRect->right - pRect->left;
	int nHeight = pRect->bottom - pRect->top;

	if (bVidFullStretch) { // Arbitrary stretch
		return 0;
	}

	if (bDrvOkay) {
		if ((BurnDrvGetFlags() & (BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED))) {
			BurnDrvGetAspect(&nGameAspectY, &nGameAspectX);
		} else {
			BurnDrvGetAspect(&nGameAspectX, &nGameAspectY);
		}
	}

	nScrnWidth = nGameAspectX;
	nScrnHeight = nGameAspectY;

	int nWidthScratch = nHeight * nVidScrnAspectY * nGameAspectX * nScrnWidth /
			    (nScrnHeight * nVidScrnAspectX * nGameAspectY);

	if (nWidthScratch > nWidth) {	// The image is too wide
		if (nGamesWidth < nGamesHeight) { // Vertical games
			nHeight = nWidth * nVidScrnAspectY * nGameAspectY * nScrnWidth /
				  (nScrnHeight * nVidScrnAspectX * nGameAspectX);
		} else {		// Horizontal games
			nHeight = nWidth * nVidScrnAspectX * nGameAspectY * nScrnHeight /
				  (nScrnWidth * nVidScrnAspectY * nGameAspectX);
		}
	} else {
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

	if (!(nInitedSubsytems & SDL_INIT_VIDEO)) {
		SDL_InitSubSystem(SDL_INIT_VIDEO);
	}

	nGamesWidth = nVidImageWidth;
	nGamesHeight = nVidImageHeight;

	nRotateGame = 0;

	if (bDrvOkay) {
		// Get the game screen size
		BurnDrvGetVisibleSize(&nGamesWidth, &nGamesHeight);

		if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
			printf("Vertical\n");
			nRotateGame = 1;
		}

		if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) {
			printf("Flipped\n");
			bFlipped = true;
		} 
	}

	if (!nRotateGame) {
		nTextureWidth = GetTextureSize(nGamesWidth);
		nTextureHeight = GetTextureSize(nGamesHeight);
	} else {
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

	printf("correctx before %d, %d\n", test_rect.right, test_rect.bottom);
	VidSScaleImage(&test_rect);
	printf("correctx after %d, %d\n", test_rect.right, test_rect.bottom);

	screen = SDL_SetVideoMode(test_rect.right * nSize,
				  test_rect.bottom * nSize, 32, SDL_OPENGL);
	SDL_WM_SetCaption("FB Alpha", NULL);

	// Initialize the buffer surfaces
	BlitFXInit();

	// Init opengl
	init_gl();

	return 0;
}

// Run one frame and render the screen
static int Frame(bool bRedraw) // bRedraw = 0
{
	if (pVidImage == NULL) {
		return 1;
	}

	if (bDrvOkay) {
		if (bRedraw) { // Redraw current frame
			if (BurnDrvRedraw()) {
				BurnDrvFrame(); // No redraw function provided, advance one frame
			}
		} else {
			BurnDrvFrame(); // Run one frame and draw the screen
		}
	}

	return 0;
}

static void SurfToTex()
{
	int nVidPitch = nTextureWidth * nVidImageBPP;

	unsigned char *ps = (unsigned char *)gamescreen;
	unsigned char *pd = (unsigned char *)texture;

	for (int y = nVidImageHeight; y--;) {
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
#ifdef frame_timer
	timeval start, end;
	time_t sec;
	suseconds_t usec;
	gettimeofday(&start, NULL);
#endif
	SurfToTex();
	TexToQuad();
	SDL_GL_SwapBuffers();

#ifdef frame_timer
	gettimeofday(&end, NULL);
	sec = end.tv_sec - start.tv_sec;
	usec = end.tv_usec - start.tv_usec;
	if (usec < 0) {
		usec += 1000000;
		sec--;
	}
	printf("Elapsed time : %ld.%ld\n", sec, usec);
#endif

	return 0;
}

static int vidScale(RECT *, int, int)
{
	return 0;
}


static int GetSettings(InterfaceInfo *pInfo)
{
	TCHAR szString[MAX_PATH] = _T("");

	_sntprintf(szString, MAX_PATH, _T("Prescaling using %s (%i× zoom)"),
					VidSoftFXGetEffect(nUseBlitter), nSize);
	IntInfoAddStringModule(pInfo, szString);

	if (nRotateGame) {
		IntInfoAddStringModule(pInfo, _T("Using software rotation"));
	}

	return 0;
}

// The Video Output plugin:
struct VidOut VidOutSDLOpenGL = { Init, Exit, Frame, Paint, vidScale, GetSettings, _T("SDL OpenGL Video output") };
