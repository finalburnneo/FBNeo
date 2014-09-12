#include <QtWidgets>
#include "qrubyviewport.h"
#include "burner.h"
#include "ruby/ruby.hpp"
#include "nall/traits.hpp"

static QRubyViewport *viewport = nullptr;
static int videoLenght = 0;
static int videoPitch = 0;
static int videoBpp = 0;
static int videoClipWidth = 0;
static int videoClipHeight = 0;
static int videoWidth = 0;
static int videoHeight = 0;
static unsigned char *videoMemory = nullptr;
static bool rotateVideo = false;


static inline unsigned c16to32(unsigned color)
{
    // convert from BGR565 to BGRA32
    unsigned b = color & 0x001F;
    unsigned g = color & 0x07E0;
    unsigned r = color & 0xF800;
    return 0xFF000000 | (r << 8) | (g << 5) | (b << 3);
}

void rubyBlitImage(QImage &image)
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

static void render16(const uint32_t* data, unsigned pitch,
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

static void renderRotate16(const unsigned short *data, unsigned pitch,
                 unsigned width, unsigned height) {
    uint32_t *output;
    unsigned outputPitch;

    if (ruby::video.lock(output, outputPitch, height, width)) {
        pitch >>= 2;
        outputPitch >>= 2;

        for (unsigned y = 0; y < height; y++) {
            auto dst = output + y;
            for (unsigned x = 0; x < width; x++) {
                *dst = c16to32(data[y * width + x]);
                dst += outputPitch;
            }
        }
        ruby::video.unlock();
        ruby::video.refresh();
    }
}

static void renderRotateVFlip16(const unsigned short *data, unsigned pitch,
                 unsigned width, unsigned height) {
    uint32_t *output;
    unsigned outputPitch;

    if (ruby::video.lock(output, outputPitch, height, width)) {
        pitch >>= 2;
        outputPitch >>= 2;

        for (unsigned y = 0; y < height; y++) {
            auto dst = output + outputPitch * (width - 1) + y;
            for (unsigned x = 0; x < width; x++) {
                *dst = c16to32(data[y * width + x]);
                dst -= outputPitch;
            }
        }
        ruby::video.unlock();
        ruby::video.refresh();
    }
}

static INT32 rubyInit()
{
    qDebug() << __func__;
    viewport = QRubyViewport::get();
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

    return 0;
}

static INT32 rubyExit()
{
    qDebug() << __func__;
    delete [] videoMemory;
    return 0;
}

static INT32 rubyFrame(bool bRedraw)
{
    nBurnBpp = 2;

    nBurnPitch = videoPitch;
    pBurnDraw = videoMemory;

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

static INT32 rubyPaint(INT32 bValidate)
{
    if (rotateVideo)
        renderRotateVFlip16((const unsigned short*) videoMemory, videoPitch, videoWidth, videoHeight);
    else
        render16((const uint32_t*) videoMemory, videoPitch, videoWidth, videoHeight);
    return 0;
}

static INT32 rubyImageSize(RECT* pRect, INT32 nGameWidth, INT32 nGameHeight)
{
    return 0;
}

// Get plugin info
static INT32 rubyGetPluginSettings(InterfaceInfo* pInfo)
{
    return 0;
}

struct VidOut VidRuby = { rubyInit, rubyExit, rubyFrame, rubyPaint,
                          rubyImageSize, rubyGetPluginSettings,
                          ("ruby-video") };


QRubyViewport *QRubyViewport::m_onlyInstance = nullptr;
QRubyViewport::QRubyViewport(QWidget *parent) :
    QWidget(parent)
{
    setAttribute(Qt::WA_PaintOnScreen, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
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

QPaintEngine *QRubyViewport::paintEngine() const
{
    return 0;
}

void QRubyViewport::paintEvent(QPaintEvent *)
{

}
