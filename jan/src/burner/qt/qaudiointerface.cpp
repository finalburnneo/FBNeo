#include <QtWidgets>
#include <QtMultimedia>
#include "burner.h"
#include "qaudiointerface.h"

#define QT_DEBUG_SOUNDBACKEND 0

QAudioInterface *qAudio = nullptr;
int (*QtSoundGetNextSound)(int);

static int nQtAudioFps = 0;
static int cbLoopLen = 0;

static QAudioInterfaceBuffer *qSoundBuffer = nullptr;

static int QtSoundGetNextSoundFiller(int)
{
    qDebug() << __func__;
    if (nAudNextSound == nullptr)
        return 1;
    memset(nAudNextSound, 0, nAudSegLen * 4);
    return 0;
}

static int QtSoundBlankSound()
{
    qDebug() << __func__;
    if (nAudNextSound != nullptr)
        AudWriteSilence();
    return 0;
}

static int QtSoundCheck()
{
    int avail = 0;
    while ((avail = qSoundBuffer->bytesAvailable()) > (nAudAllocSegLen * 3)) {
        return 0;
    }

    QtSoundGetNextSound(1);
    qSoundBuffer->writeData((const char *)nAudNextSound, nAudSegLen << 2);
    return 0;
}

static int QtSoundExit()
{
    qDebug() << __func__;
    return 0;
}

static int QtSoundSetCallback(int (*pCallback)(int))
{
    qDebug() << __func__;
    if (pCallback == nullptr)
        QtSoundGetNextSound = QtSoundGetNextSoundFiller;
    else
        QtSoundGetNextSound = pCallback;
    return 0;
}

static int QtSoundInit()
{
    qDebug() << __func__;
    nQtAudioFps = nAppVirtualFps;
    nAudSegLen = (nAudSampleRate[0] * 100 + (nQtAudioFps / 2)) / nQtAudioFps;

    // seglen * 2 channels * 2 bytes per sample (16bits)
    nAudAllocSegLen = nAudSegLen * 4;

    // seg * nsegs * 2 channels
    if (qSoundBuffer != nullptr)
        delete qSoundBuffer;

    qSoundBuffer = new QAudioInterfaceBuffer();
    qSoundBuffer->setBufferSize(nAudAllocSegLen * nAudSegCount);
    nAudNextSound = new short[nAudAllocSegLen / sizeof(short)];

    QtSoundSetCallback(nullptr);
    QtSoundGetNextSoundFiller(0);

    //qSoundBuffer->writeData((const char *) nAudNextSound, nAudAllocSegLen);
    //qSoundBuffer->writeData((const char *) nAudNextSound, nAudAllocSegLen);

    pBurnSoundOut = nAudNextSound;
    nBurnSoundRate = nAudSampleRate[0];
    nBurnSoundLen = nAudAllocSegLen;

    qAudio = QAudioInterface::get();
    qAudio->setBuffer(qSoundBuffer);
    QAudioFormat format;
    format.setChannelCount(2);
    format.setSampleRate(nAudSampleRate[0]);
    format.setSampleType(QAudioFormat::SignedInt);
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleSize(16);
    qAudio->setFormat(format);

    // we need to invoke this method on sound's thread, so we can't call this
    // method from our object
    QMetaObject::invokeMethod(qAudio, "init", Qt::QueuedConnection);

    bAudOkay = 1;
    return 0;
}

static int QtSoundPlay()
{
    qDebug() << __func__;
    QMetaObject::invokeMethod(qAudio, "play", Qt::QueuedConnection);
    bAudPlaying = 1;
    return 0;
}

static int QtSoundStop()
{
    qDebug() << __func__;
    if (qAudio)
        QMetaObject::invokeMethod(qAudio, "stop", Qt::QueuedConnection);
    bAudPlaying = 0;
    return 0;
}

static int QtSoundSetVolume()
{
    qDebug() << __func__;
    return 1;
}

static int QtSoundGetSettings(InterfaceInfo *info)
{
    Q_UNUSED(info);
    return 0;
}

struct AudOut AudOutQtSound = { QtSoundBlankSound, QtSoundCheck,
                                QtSoundInit, QtSoundSetCallback, QtSoundPlay,
                                QtSoundStop, QtSoundExit, QtSoundSetVolume,
                                QtSoundGetSettings, ("qt-audio") };



QAudioInterface *QAudioInterface::m_onlyInstance = nullptr;
QAudioInterface::QAudioInterface(QObject *parent) :
    QThread()
{
    // start thread
    start();
    m_audioOutput = nullptr;

    moveToThread(this);
}

QAudioInterface::~QAudioInterface()
{
    if (m_audioOutput != nullptr) {
        delete m_audioOutput;
        m_audioOutput = nullptr;
    }
}

void QAudioInterface::run()
{
    exec();
}

QAudioInterface *QAudioInterface::get(QObject *parent)
{
    if (m_onlyInstance != nullptr)
        return m_onlyInstance;
    m_onlyInstance = new QAudioInterface(parent);

    return m_onlyInstance;
}

void QAudioInterface::init()
{
    if (m_audioOutput != nullptr) {
        stop();
        delete m_audioOutput;
        m_audioOutput = nullptr;
    }
    m_audioOutput = new QAudioOutput(m_format, this);
    m_audioOutput->setBufferSize(nAudAllocSegLen);
}

void QAudioInterface::play()
{
    if (m_buffer != nullptr)
        m_buffer->start();
    m_audioOutput->start(m_buffer);
}

void QAudioInterface::stop()
{
    m_audioOutput->stop();
    if (m_buffer != nullptr)
        m_buffer->stop();
}

void QAudioInterface::setBuffer(QAudioInterfaceBuffer *buffer)
{
    m_buffer = buffer;
}

void QAudioInterface::setFormat(QAudioFormat &format)
{
    m_format = format;
}

void QAudioInterface::writeMoreData()
{
}


QAudioInterfaceBuffer::QAudioInterfaceBuffer(int size) :
    QIODevice()
{

    m_buffer = nullptr;
    if (size > 0) {
        m_buffer = new char[size];
    }
    m_size = size;
    m_readPos = 0;
    m_writePos = 0;
    m_readWrap = 0;
    m_writeWrap = 0;
}

QAudioInterfaceBuffer::~QAudioInterfaceBuffer()
{
    delete[] m_buffer;
    m_buffer = nullptr;
    m_size = 0;
}

void QAudioInterfaceBuffer::start()
{
    open(QIODevice::ReadWrite | QIODevice::Unbuffered);
}

void QAudioInterfaceBuffer::stop()
{
    close();
}


qint64 QAudioInterfaceBuffer::readData(char *data, qint64 maxlen)
{

    if (m_size <= 0 || maxlen <= 0) {
        return 0;
    }

    int avail = this->bytesAvailable();

#if QT_DEBUG_SOUNDBACKEND
    qDebug() << QThread::currentThreadId() << (quint64)m_elapsed.elapsed() <<
                "rd:" << maxlen << "/" << avail;
#endif
    if (avail <= 0) {
        memset(data, 0, maxlen);
        return maxlen;
    }

    m_mutex.lock();

    int count = maxlen;
    char *pdata = data;
    int increment = 0;

    while (count > 0) {
        bool wrap = (m_readPos + count) >= m_size;
        if (wrap) {
            int block = m_size - m_readPos;
            memcpy(pdata, m_buffer + m_readPos, block);
            count -= block;
            pdata += block;
            increment = block;
            m_readWrap++;
        } else {
            memcpy(pdata, m_buffer + m_readPos, count);
            increment = count;
            count -= count;
            pdata += count;
        }
        m_readPos = (m_readPos + increment) % m_size;
    }

    int retval = avail;
    if (avail >= maxlen)
        retval = maxlen;

    m_mutex.unlock();
    return retval;
}

qint64 QAudioInterfaceBuffer::writeData(const char *data, qint64 len)
{
    if (m_size <= 0 || len <= 0) {
        return 0;
    }

    m_mutex.lock();
    int count = len;
    const char *pdata = data;
    int increment = 0;

#if QT_DEBUG_SOUNDBACKEND
    qDebug() << QThread::currentThreadId() << (quint64)m_elapsed.elapsed() <<
                "wr:" << len << "+" << (writeCounter() - readCounter());
#endif
    while (count > 0) {
        bool wrap = (m_writePos + count) >= m_size;
        if (wrap) {
            int block = m_size - m_writePos;
            memcpy(m_buffer + m_writePos, pdata, block);
            increment = block;
            count -= block;
            pdata += block;
            m_writeWrap++;
            m_writePos = 0;
        } else {
            memcpy(m_buffer + m_writePos, pdata, count);
            increment = count;
            count -= count;
            pdata += count;
            m_writePos = (m_writePos + increment) % m_size;
        }
    }

    m_mutex.unlock();
    return len;
}

qint64 QAudioInterfaceBuffer::bytesAvailable()
{
    m_mutex.lock();
    int count = writeCounter() - readCounter();
    m_mutex.unlock();
    if (count >= 0)
        return count;
    return 0;
}

void QAudioInterfaceBuffer::setBufferSize(int len)
{
    if (m_buffer != nullptr)
        delete[] m_buffer;
#if QT_DEBUG_SOUNDBACKEND
    qDebug() << "buffer size" << len;
#endif
    m_buffer = new char[len];
    m_size = len;
    m_readPos = 0;
    m_writePos = 0;
    m_readWrap = 0;
    m_writeWrap = 0;
    zero();
}

void QAudioInterfaceBuffer::zero()
{
    if (m_buffer != nullptr)
        memset(m_buffer, 0, m_size);
}

int QAudioInterfaceBuffer::readCounter() const
{
    return (m_readWrap * m_size) + m_readPos;
}

int QAudioInterfaceBuffer::writeCounter() const
{
    return (m_writeWrap * m_size) + m_writePos;
}
