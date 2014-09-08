#include <QtWidgets>
#include "qrubyviewport.h"
#include "burner.h"
#include "ruby/ruby.hpp"
#include "nall/traits.hpp"

static QRubyViewport *viewport = nullptr;
static int VidMemLen	= 0;
static int VidMemPitch	= 0;
static int VidBpp		= 0;
static int clipx		= 0;
static int clipy		= 0;
static int sizex		= 0;
static int sizey		= 0;
static unsigned char* VidMem = NULL;

static inline unsigned c16to32(unsigned color)
{
    // convert from BGR565 to BGRA32
    unsigned b = color & 0x001F;
    unsigned g = color & 0x07E0;
    unsigned r = color & 0xF800;
    return 0xFF000000 | (r << 8) | (g << 5) | (b << 3);
}

void RubyVidBlit(QImage &image)
{
    uint32_t *output;
    unsigned outputPitch;
    unsigned pitch = image.bytesPerLine();
    const int width = image.width();
    const int height = image.height();

    if (ruby::video.lock(output, outputPitch, width, height)) {
        pitch >>= 2;
        outputPitch >>= 2;

        for (unsigned y = 0; y < height; y++) {
            const uint32_t *psrc = (const uint32_t *)(image.scanLine(y));
            uint32_t *pdst = output + y * outputPitch;

            for (unsigned x = 0; x < width; x++) {
                *pdst = *psrc;
                ++pdst;
                ++psrc;
            }
        }
        ruby::video.unlock();
        ruby::video.refresh();
    }
}

void RubyVidCopy16(const uint32_t* data, unsigned pitch,
                 unsigned width, unsigned height) {
    uint32_t *output;
    unsigned outputPitch;

    if (ruby::video.lock(output, outputPitch, width, height)) {
        pitch >>= 2;
        outputPitch >>= 2;

        for (unsigned y = 0; y < height; y++) {
            const uint32_t *psrc = data + y * pitch;
            uint32_t *pdst = output + y * outputPitch;

            for (unsigned x = 0; x < width; x++) {
                *pdst++ = c16to32(*psrc);
                psrc = (uint32_t*)((uint16_t*)psrc + 1);
            }
        }
        ruby::video.unlock();
        ruby::video.refresh();
    }
}

static INT32 RubyVidInit()
{
    qDebug() << __func__;
    viewport = QRubyViewport::get();

    if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
        BurnDrvGetVisibleSize(&clipy, &clipx);
        BurnDrvGetFullSize(&sizex, &sizey);
    } else {
        BurnDrvGetVisibleSize(&clipx, &clipy);
        BurnDrvGetFullSize(&sizex, &sizey);
    }
    VidBpp		= nBurnBpp = 2;
    VidMemPitch	= sizex * VidBpp;
    VidMemLen	= sizey * VidMemPitch;
    VidMem = (unsigned char*)malloc(VidMemLen);
    SetBurnHighCol(16);

    return 0;
}

static INT32 RubyVidExit()
{
    qDebug() << __func__;
    free(VidMem);
    return 0;
}

static INT32 RubyVidFrame(bool bRedraw)
{
    nBurnBpp = 2;

    nBurnPitch = VidMemPitch;
    pBurnDraw = VidMem;

    if (bDrvOkay) {
        pBurnSoundOut = nAudNextSound;
        nBurnSoundLen = nAudSegLen;
        BurnDrvFrame();							// Run one frame and draw the screen
    }

    nFramesRendered++;

    pBurnDraw = NULL;
    nBurnPitch = 0;
    return 0;
}

static INT32 RubyVidPaint(INT32 bValidate)
{
    if (bValidate & 2) {
    }
    RubyVidCopy16((const uint32_t*) VidMem, VidMemPitch, sizex, sizey);
    return 0;
}

static INT32 RubyVidImageSize(RECT* pRect, INT32 nGameWidth, INT32 nGameHeight)
{
    return 0;
}

// Get plugin info
static INT32 RubyVidGetPluginSettings(InterfaceInfo* pInfo)
{
    return 0;
}

struct VidOut VidRuby = { RubyVidInit, RubyVidExit, RubyVidFrame, RubyVidPaint,
                           RubyVidImageSize, RubyVidGetPluginSettings,
                           ("ruby-video") };


QRubyViewport *QRubyViewport::m_onlyInstance = nullptr;
QRubyViewport::QRubyViewport(QWidget *parent) :
    QWidget(parent)
{
    setAutoFillBackground(false);
}

QRubyViewport::~QRubyViewport()
{
    m_onlyInstance = nullptr;
}

QRubyViewport *QRubyViewport::get(QWidget *parent)
{
    if (m_onlyInstance != nullptr)
        return m_onlyInstance;
    m_onlyInstance = new QRubyViewport(parent);
    return m_onlyInstance;
}

uintptr_t QRubyViewport::id()
{
    return (uintptr_t)winId();
}
