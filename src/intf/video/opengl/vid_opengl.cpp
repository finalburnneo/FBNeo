// OpenGL 3.3 Core Profile renderer
#include <GL/glew.h>
#include <iostream>
#include "burner.h"

#include "shader.h"

static int videoLenght = 0;
static int videoPitch = 0;
static int videoBpp = 0;
static int videoClipWidth = 0;
static int videoClipHeight = 0;
static int videoWidth = 0;
static int videoHeight = 0;
static unsigned char *videoMemory = nullptr;
static bool rotateVideo = false;

static bool isGlewInitialized = false;
static GLuint vao, texture;

static void (*oglSwapBuffers)(void*) = nullptr;
static void (*oglMakeCurrent)(void*) = nullptr;
static void (*oglDoneCurrent)(void*) = nullptr;
static void *oglContext = nullptr;
static Shader *stockShader = nullptr;


static const string defaultVertexShader = R"(
        #version 330 core

        layout (location = 0) in vec2 position;
        layout (location = 1) in vec2 uvCoord;

        out vec2 texCoord;

        void main() {
            gl_Position = vec4(position, 0, 1.0f);
            texCoord = uvCoord;
        }
)";

static const string defaultFragmentShader = R"(
        #version 330 core

        out vec4 color;
        in vec2 texCoord;

        uniform sampler2D tex;
        uniform float width;
        uniform float height;

        void main() {
            color = texture(tex, texCoord);
        }
)";


void OGLSetContext(void *ptr)
{
    oglContext = ptr;
}

void OGLSetSwapBuffers(void (*f)(void*))
{
    oglSwapBuffers = f;
}

void OGLSetMakeCurrent(void (*f)(void*))
{
    oglMakeCurrent = f;
}

void OGLSetDoneCurrent(void (*f)(void*))
{
    oglDoneCurrent = f;
}

void OGLResize(int w, int h)
{
    glViewport(0, 0, w, h);
}

static INT32 oglInit()
{
    if (!isGlewInitialized) {
        glewExperimental = true;
        glewInit();
    }

    stockShader = new Shader(defaultVertexShader, defaultFragmentShader, true);

    unsigned flags = BurnDrvGetFlags();

    if (flags & BDF_ORIENTATION_VERTICAL) {
        BurnDrvGetVisibleSize(&videoClipHeight, &videoClipWidth);
        BurnDrvGetFullSize(&videoWidth, &videoHeight);
        rotateVideo = true;
    } else {
        BurnDrvGetVisibleSize(&videoClipWidth, &videoClipHeight);
        BurnDrvGetFullSize(&videoWidth, &videoHeight);
        rotateVideo = false;
    }

    videoBpp = nBurnBpp = 2;
    videoPitch = videoWidth * videoBpp;
    videoLenght = videoHeight * videoPitch;
    videoMemory = new unsigned char[videoLenght];
    SetBurnHighCol(16);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glGenVertexArrays(1, &vao);
    glGenTextures(1, &texture);

    return 0;
}

static INT32 oglExit()
{
    delete stockShader;
    glDeleteTextures(1, &texture);
    glDeleteVertexArrays(1, &vao);
    delete [] videoMemory;
    return 0;
}

static INT32 oglFrame(bool bRedraw)
{
    nBurnBpp = 2;

    nBurnPitch = videoPitch;
    pBurnDraw = videoMemory;

    if (bDrvOkay) {
        pBurnSoundOut = nAudNextSound;
        nBurnSoundLen = nAudSegLen;
        BurnDrvFrame();
    }

    pBurnDraw = NULL;
    nBurnPitch = 0;
    return 0;
}

static INT32 oglPaint(INT32 bValidate)
{
    if (oglMakeCurrent)
        oglMakeCurrent(oglContext);

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, videoWidth, videoHeight, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, videoMemory);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    const GLfloat vtx[] = {
        // vtx          uv
        -1.0,  1.0,     0.0, 0.0,
        -1.0, -1.0,     0.0, 1.0,
         1.0,  1.0,     1.0, 0.0,
         1.0, -1.0,     1.0, 1.0
    };
    glBindVertexArray(vao);

    GLuint vbo;
    glGenBuffers(1, &vbo);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vtx), vtx, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*) (2 * sizeof(GLfloat)));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    stockShader->use();
    stockShader->setUniform("width", static_cast<float>(videoWidth));
    stockShader->setUniform("height", static_cast<float>(videoHeight));
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glDeleteBuffers(1, &vbo);
    glBindVertexArray(0);

    if (oglSwapBuffers)
        oglSwapBuffers(oglContext);

    if (oglDoneCurrent)
        oglDoneCurrent(oglContext);
    return 0;
}

static INT32 oglImageSize(RECT* pRect, INT32 nGameWidth, INT32 nGameHeight)
{
    return 0;
}

// Get plugin info
static INT32 oglGetPluginSettings(InterfaceInfo* pInfo)
{
    return 0;
}

struct VidOut VidOutOGL = { oglInit, oglExit, oglFrame, oglPaint,
                          oglImageSize, oglGetPluginSettings,
                          ("ogl-video") };
