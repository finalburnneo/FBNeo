#ifndef ROMSCANDIALOG_H
#define ROMSCANDIALOG_H

#include <QDialog>
#include <QThread>
#include <QVector>

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
protected:
    virtual void showEvent(QShowEvent *event);
private:
    bool load();
    bool save();
    Ui::RomScanDialog *ui;
    QVector<char> m_status;
    RomAnalyzer m_analyzer;
};

#endif // ROMSCANDIALOG_H
