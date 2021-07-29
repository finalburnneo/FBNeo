// Based on piSNES by Squid
// https://github.com/squidrpi/pisnes

#include <stdlib.h>
#include <stdio.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <SDL.h>

#include "burner.h"

extern "C" {
#include "matrix.h"
#include "pigl.h"
}

typedef	struct ShaderInfo {
	GLuint program;
	GLint a_position;
	GLint a_texcoord;
	GLint u_vp_matrix;
	GLint u_texture;
} ShaderInfo;

static void drawQuad(const ShaderInfo *sh);
static GLuint createShader(GLenum type, const char *shaderSrc);
static GLuint createProgram(const char *vertexShaderSrc, const char *fragmentShaderSrc);

static int bufferWidth;
static int bufferHeight;
static int bufferBpp;
static int bufferPitch;
static unsigned char *bufferBitmap;

static int textureWidth;
static int textureHeight;
static int texturePitch;
static int textureFormat;
static unsigned char *textureBitmap;

static int screenRotated = 0;
static int screenFlipped = 0;

static ShaderInfo shader;
static GLuint buffers[3];
static GLuint texture;

static SDL_Surface *sdlScreen;

static const char* vertexShaderSrc =
	"uniform mat4 u_vp_matrix;\n"
	"attribute vec4 a_position;\n"
	"attribute vec2 a_texcoord;\n"
	"varying mediump vec2 v_texcoord;\n"
	"void main() {\n"
	"	v_texcoord = a_texcoord;\n"
	"	gl_Position = u_vp_matrix * a_position;\n"
	"}\n";

static const char* fragmentShaderNone =
	"varying mediump vec2 v_texcoord;\n"
	"uniform sampler2D u_texture;\n"
	"void main() {\n"
	"	gl_FragColor = texture2D(u_texture, v_texcoord);\n"
	"}\n";
static const char *fragmentShaderScanline = 
	"varying mediump vec2 v_texcoord;\n"
	"uniform sampler2D u_texture;\n"
	"void main()\n"
	"{\n"
	"	vec3 rgb = texture2D(u_texture, v_texcoord).rgb;\n"
	"	vec3 intens;\n"
	"	if (fract(gl_FragCoord.y * (0.5*4.0/3.0)) > 0.5)\n"
	"		intens = vec3(0);\n"
	"	else\n"
	"		intens = smoothstep(0.2,0.8,rgb) + normalize(rgb);\n"
	"	float level = (4.0-0.0) * 0.19;\n"
	"	gl_FragColor = vec4(intens * (0.5-level) + rgb * 1.1, 1.0);\n"
	"}\n";

static const GLushort indices[] = {
	0, 1, 2,
	0, 2, 3,
};

static const int kVertexCount = 4;
static const int kIndexCount = 6;

static const int vborder_thickness = 16;

static int screen_width;
static int screen_height;

static struct phl_matrix projection;

static const GLfloat vertices[] = {
	-0.5f, -0.5f, 0.0f,
	+0.5f, -0.5f, 0.0f,
	+0.5f, +0.5f, 0.0f,
	-0.5f, +0.5f, 0.0f,
};

static int piInitVideo()
{
	if (!pigl_init()) {
		return 0;
	}

	screen_width = pigl_screen_width;
	screen_height = pigl_screen_height - vborder_thickness;

	fprintf(stderr, "Initializing shaders...\n");

	// Init shader resources
	memset(&shader, 0, sizeof(ShaderInfo));

	const char *fragmentShaderSrc;
	if (bVidScanlines) {
		fragmentShaderSrc = fragmentShaderScanline;
	} else {
		fragmentShaderSrc = fragmentShaderNone;
	}

	shader.program = createProgram(vertexShaderSrc, fragmentShaderSrc);
	if (!shader.program) {
		fprintf(stderr, "createProgram() failed\n");
		return 0;
	}

	shader.a_position	= glGetAttribLocation(shader.program,	"a_position");
	shader.a_texcoord	= glGetAttribLocation(shader.program,	"a_texcoord");
	shader.u_vp_matrix	= glGetUniformLocation(shader.program,	"u_vp_matrix");
	shader.u_texture	= glGetUniformLocation(shader.program,	"u_texture");

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_DITHER);

	fprintf(stderr, "Initializing SDL video...\n");

	// We're doing our own video rendering - this is just so SDL-based keyboard
	// can work
	sdlScreen = SDL_SetVideoMode(0, 0, 0, 0);

	return 1;
}

static void piDestroyVideo()
{
	fprintf(stderr, "Destroying video...\n");

	if (sdlScreen) {
		SDL_FreeSurface(sdlScreen);
	}

	// Destroy shader resources
	if (shader.program) {
		glDeleteProgram(shader.program);
	}

	pigl_shutdown();
}

static void piUpdateEmuDisplay()
{
	if (!shader.program) {
		fprintf(stderr, "Shader not initialized\n");
		return;
	}

	glClear(GL_COLOR_BUFFER_BIT);
	glViewport(0, pigl_screen_height - screen_height, screen_width, screen_height);

	ShaderInfo *sh = &shader;

	glDisable(GL_BLEND);
	glUseProgram(sh->program);
	glUniformMatrix4fv(sh->u_vp_matrix, 1, GL_FALSE, &projection.xx);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);

	unsigned char *ps = (unsigned char *)bufferBitmap;
	unsigned char *pd = (unsigned char *)textureBitmap;

	for (int y = nVidImageHeight; y--;) {
		memcpy(pd, ps, nVidImagePitch);
		pd += texturePitch;
		ps += nVidImagePitch;
	}

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, textureWidth, textureHeight,
		GL_RGB, textureFormat, textureBitmap);

	drawQuad(sh);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	pigl_swap();
}

static GLuint createShader(GLenum type, const char *shaderSrc)
{
	GLuint shader = glCreateShader(type);
	if (!shader) {
		fprintf(stderr, "glCreateShader() failed: %d\n", glGetError());
		return 0;
	}

	// Load and compile the shader source
	glShaderSource(shader, 1, &shaderSrc, NULL);
	glCompileShader(shader);

	// Check the compile status
	GLint compiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		GLint infoLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 1) {
			char* infoLog = (char *)malloc(sizeof(char) * infoLen);
			glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
			fprintf(stderr, "Error compiling shader:\n%s\n", infoLog);
			free(infoLog);
		}

		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

static GLuint createProgram(const char *vertexShaderSrc, const char *fragmentShaderSrc)
{
	GLuint vertexShader = createShader(GL_VERTEX_SHADER, vertexShaderSrc);
	if (!vertexShader) {
		fprintf(stderr, "createShader(GL_VERTEX_SHADER) failed\n");
		return 0;
	}

	GLuint fragmentShader = createShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);
	if (!fragmentShader) {
		fprintf(stderr, "createShader(GL_FRAGMENT_SHADER) failed\n");
		glDeleteShader(vertexShader);
		return 0;
	}

	GLuint programObject = glCreateProgram();
	if (!programObject) {
		fprintf(stderr, "glCreateProgram() failed: %d\n", glGetError());
		return 0;
	}

	glAttachShader(programObject, vertexShader);
	glAttachShader(programObject, fragmentShader);

	// Link the program
	glLinkProgram(programObject);

	// Check the link status
	GLint linked = 0;
	glGetProgramiv(programObject, GL_LINK_STATUS, &linked);
	if (!linked) {
		GLint infoLen = 0;
		glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 1) {
			char* infoLog = (char *)malloc(infoLen);
			glGetProgramInfoLog(programObject, infoLen, NULL, infoLog);
			fprintf(stderr, "Error linking program: %s\n", infoLog);
			free(infoLog);
		}

		glDeleteProgram(programObject);
		return 0;
	}

	// Delete these here because they are attached to the program object.
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return programObject;
}

static void drawQuad(const ShaderInfo *sh)
{
	glUniform1i(sh->u_texture, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
	glVertexAttribPointer(sh->a_position, 3, GL_FLOAT,
		GL_FALSE, 3 * sizeof(GLfloat), NULL);
	glEnableVertexAttribArray(sh->a_position);

	glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
	glVertexAttribPointer(sh->a_texcoord, 2, GL_FLOAT,
		GL_FALSE, 2 * sizeof(GLfloat), NULL);
	glEnableVertexAttribArray(sh->a_texcoord);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[2]);

	glDrawElements(GL_TRIANGLES, kIndexCount, GL_UNSIGNED_SHORT, 0);
}

static int closestPowerOfTwo(int num)
{
    int rv = 1;
    while (rv < num) {
    	rv *= 2;
    }
    return rv;
}

static int reinitTextures()
{
	fprintf(stderr, "Initializing textures...\n");
	
	textureWidth = closestPowerOfTwo(nVidImageWidth); // adjusted for rotation
	textureHeight = closestPowerOfTwo(nVidImageHeight); // adjusted for rotation
	texturePitch = textureWidth * bufferBpp;
	textureFormat = GL_UNSIGNED_SHORT_5_6_5;

	GLfloat minU = 0.0f;
	GLfloat minV = 0.0f;
	GLfloat maxU = ((float)nVidImageWidth / textureWidth - minU);
	GLfloat maxV = ((float)nVidImageHeight / textureHeight);
	GLfloat uvs[] = {
		minU, minV,
		maxU, minV,
		maxU, maxV,
		minU, maxV,
	};

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureWidth, textureHeight, 0, GL_RGB, textureFormat, NULL);

	glGenBuffers(3, buffers);
	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
	glBufferData(GL_ARRAY_BUFFER, kVertexCount * sizeof(GLfloat) * 3, vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
	glBufferData(GL_ARRAY_BUFFER, kVertexCount * sizeof(GLfloat) * 2, uvs, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[2]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, kIndexCount * sizeof(GL_UNSIGNED_SHORT), indices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	float sx = 1.0f;
	float sy = 1.0f;
	float zoom = 1.0f;

	// Screen aspect ratio adjustment
	float a = (float)screen_width / (float)screen_height;
	float a0 = (float)bufferWidth / (float)bufferHeight;
	if (a > a0) {
		sx = a0/a;
	} else {
		sy = a/a0;
	}

	phl_matrix_identity(&projection);
	if (screenRotated) {
		phl_matrix_rotate_z(&projection, screenFlipped ? 270 : 90);
	}
	phl_matrix_ortho(&projection, -0.5f, +0.5f, +0.5f, -0.5f, -1.0f, 1.0f);
	phl_matrix_scale(&projection, sx * zoom, sy * zoom, 0);

	fprintf(stderr, "Setting up screen...\n");

	int bufferSize = bufferWidth * bufferHeight * bufferBpp;
	free(bufferBitmap);
	if ((bufferBitmap = (unsigned char *)malloc(bufferSize)) == NULL) {
		fprintf(stderr, "Error allocating buffer bitmap\n");
		return 0;
	}
	
	nBurnBpp = bufferBpp;
	nBurnPitch = nVidImagePitch;
	pVidImage = bufferBitmap;
	
	memset(bufferBitmap, 0, bufferSize);
	
	int textureSize = textureWidth * textureHeight * bufferBpp;
	free(textureBitmap);
	if ((textureBitmap = (unsigned char *)calloc(1, textureSize)) == NULL) {
		fprintf(stderr, "Error allocating buffer bitmap\n");
		return 0;
	}

	return 1;
}

// Specific to FB

static int FbInit()
{
	if (!piInitVideo()) {
		return 1;
	}

	int virtualWidth;
	int virtualHeight;
	int xAspect;
	int yAspect;

	if (bDrvOkay) {
		BurnDrvGetAspect(&xAspect, &yAspect);
		BurnDrvGetVisibleSize(&virtualWidth, &virtualHeight);
		if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
		    screenRotated = 1;
		}
		
		if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) {
			screenFlipped = 1;
		}
		
		fprintf(stderr, "Game screen size: %dx%d (%s,%s) (aspect: %d:%d)\n",
			virtualWidth, virtualHeight,
			screenRotated ? "rotated" : "not rotated",
			screenFlipped ? "flipped" : "not flipped",
			xAspect, yAspect);

		nVidImageDepth = 16;
		nVidImageBPP = 2;
		
		float ratio = (float) yAspect / xAspect;

		if (!screenRotated) {
			nVidImageWidth = virtualWidth;
			nVidImageHeight = virtualHeight;
		} else {
			nVidImageWidth = virtualHeight;
			nVidImageHeight = virtualWidth;
		}

		nVidImagePitch = nVidImageWidth * nVidImageBPP;
		
		SetBurnHighCol(nVidImageDepth);
		
		bufferWidth = virtualWidth;
		bufferHeight = virtualHeight;
		bufferBpp = nVidImageBPP;

		if (!screenRotated) {
			if (bufferHeight / ratio >= bufferWidth)
				bufferWidth = bufferHeight / ratio;
			else
				bufferHeight = bufferWidth * ratio;
		} else {
			bufferWidth = bufferHeight / ratio;
		}
		fprintf(stderr, "W: %d; H: %d (%f ratio)\n", bufferWidth, bufferHeight, ratio);
		
		if (!reinitTextures()) {
			fprintf(stderr, "Error initializing textures\n");
			return 1;
		}
	}

	return 0;
}

static int FbExit()
{
	glDeleteBuffers(3, buffers);
	glDeleteTextures(1, &texture);

	free(bufferBitmap);
	free(textureBitmap);
	bufferBitmap = NULL;
	textureBitmap = NULL;

	piDestroyVideo();

	return 0;
}

static int FbRunFrame(bool bRedraw)
{
	if (pVidImage == NULL) {
		return 1;
	}

	VidFrameCallback(bRedraw);

	return 0;
}

static int FbPaint(int bValidate)
{
	piUpdateEmuDisplay();
	return 0;
}

static int FbGetSettings(InterfaceInfo *)
{
	return 0;
}

static int FbVidScale(RECT *, int, int)
{
	return 0;
}

struct VidOut VidOutPi = { FbInit, FbExit, FbRunFrame, FbPaint, FbVidScale, FbGetSettings, _T("Raspberry Pi video output") };
