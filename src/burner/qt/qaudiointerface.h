#ifndef QAUDIOINTERFACE_H
#define QAUDIOINTERFACE_H

#include <QAudioFormat>
#include <QAudioOutput>
#include <QIODevice>
#include <QThread>
#include <QMutex>
#include <QElapsedTimer>

class QAudioInterfaceBuffer : public QIODevice
{
    Q_OBJECT

public:
    QAudioInterfaceBuffer(int size = 0);
    ~QAudioInterfaceBuffer();
    void start();
    void stop();
    qint64 readData(char *data, qint64 maxlen);
    qint64 writeData(const char *data, qint64 len);
    qint64 bytesAvailable();
    void setBufferSize(int len);

private:
    QElapsedTimer m_elapsed;
    QMutex m_mutex;
    void zero();
    int readCounter() const;
    int writeCounter() const;
    int m_writeWrap;
    int m_readWrap;
    int m_size;
    char *m_buffer;
    int m_readPos;
    int m_writePos;
};

class QAudioInterface : public QThread
{
    Q_OBJECT

    QAudioInterface(QObject *parent=0);
    static QAudioInterface *m_onlyInstance;
public:
    ~QAudioInterface();

    void run();
    static QAudioInterface *get(QObject *parent=0);
    void setBuffer(QAudioInterfaceBuffer *buffer);
    void setFormat(QAudioFormat &format);
signals:
public slots:
    void play();
    void stop();
    void init();
    void writeMoreData();
private:
    QAudioOutput *m_audioOutput;
    QAudioFormat m_format;
    QAudioInterfaceBuffer *m_buffer;
};

#endif // QAUDIOINTERFACE_H
