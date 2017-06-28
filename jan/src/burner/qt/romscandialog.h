#ifndef ROMSCANDIALOG_H
#define ROMSCANDIALOG_H

#include <QDialog>
#include <QThread>
#include <QByteArray>

namespace Ui {
class RomScanDialog;
}

class RomScanDialog;

class RomAnalyzer : public QThread {
    Q_OBJECT
    RomScanDialog *m_scanDlg;
public:
    RomAnalyzer(RomScanDialog *parent);
signals:
    void setRange(int, int);
    void setValue(int);
    void done();
private:
    void run();
};

class RomScanDialog : public QDialog
{
    friend class RomAnalyzer;
    Q_OBJECT

public:
    explicit RomScanDialog(QWidget *parent = 0);
    ~RomScanDialog();
    int status(int drvNo);
    void setStatus(int drvNo, char stat);
public slots:
    void cancel();
    bool load();
    bool save();
protected:
    virtual void showEvent(QShowEvent *event);
private:
    Ui::RomScanDialog *ui;
    QByteArray m_status;
    RomAnalyzer m_analyzer;
    const QString m_settingsName;
};

#endif // ROMSCANDIALOG_H
