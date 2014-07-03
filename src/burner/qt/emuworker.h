#ifndef EMUWORKER_H
#define EMUWORKER_H

#include <QObject>

class EmuWorker : public QObject
{
    Q_OBJECT
public:
    explicit EmuWorker(QObject *parent = 0);

signals:

public slots:
    bool init();
    void resume();
    void close();
    void pause();
    void setGame(int no);
    void run();
private:
    int m_game;
    bool m_isRunning;
};

#endif // EMUWORKER_H
