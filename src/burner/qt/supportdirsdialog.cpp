#include <QtWidgets>
#include "supportdirsdialog.h"
#include "ui_supportdirsdialog.h"
#include "burner.h"

TCHAR szAppHiscorePath[MAX_PATH] = _T("support/hiscores/");
TCHAR szAppSamplesPath[MAX_PATH] = _T("support/samples/");
TCHAR szAppCheatsPath[MAX_PATH] = _T("support/cheats/");
TCHAR szAppPreviewsPath[MAX_PATH] = _T("support/previews/");
TCHAR szAppTitlesPath[MAX_PATH] = _T("support/titles/");

SupportDirsDialog::SupportDirsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SupportDirsDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("Edit support paths"));
    setFixedHeight(height());

    m_group = new QButtonGroup(this);

    m_handlers[PATH_PREVIEWS] = util::PathHandler { szAppPreviewsPath, ui->lePreviews, PATH_PREVIEWS };
    m_handlers[PATH_TITLES] = util::PathHandler { szAppTitlesPath, ui->leTitles, PATH_TITLES };
    m_handlers[PATH_HISCORES] = util::PathHandler { szAppHiscorePath, ui->leHiscores, PATH_HISCORES };
    m_handlers[PATH_SAMPLES] = util::PathHandler { szAppSamplesPath, ui->leSamples, PATH_SAMPLES };
    m_handlers[PATH_CHEATS] = util::PathHandler { szAppCheatsPath, ui->leCheats, PATH_CHEATS };

    m_group->addButton(ui->btnPreviews, PATH_PREVIEWS);
    m_group->addButton(ui->btnTitles, PATH_TITLES);
    m_group->addButton(ui->btnHiscores, PATH_HISCORES);
    m_group->addButton(ui->btnSamples, PATH_SAMPLES);
    m_group->addButton(ui->btnCheats, PATH_CHEATS);
    connect(m_group, SIGNAL(buttonClicked(int)), this, SLOT(editPath(int)));

    load();
}

SupportDirsDialog::~SupportDirsDialog()
{
    save();
    delete ui;
}

void SupportDirsDialog::editPath(int no)
{
    m_handlers[no].browse(this);
}

int SupportDirsDialog::exec()
{
    for (int i = 0; i < PATH_MAX_HANDLERS; i++)
        m_handlers[i].stringToEditor();

    if (QDialog::exec() == QDialog::Accepted) {
        for (int i = 0; i < PATH_MAX_HANDLERS; i++)
            m_handlers[i].editorToString();
    }
}

bool SupportDirsDialog::load()
{
    QSettings settings(util::appConfigName, QSettings::IniFormat);
    if (settings.status() == QSettings::AccessError)
        return false;

    settings.beginGroup("support_directories");

    loadPath(settings, "previews", PATH_PREVIEWS);
    loadPath(settings, "titles", PATH_TITLES);
    loadPath(settings, "hiscores", PATH_HISCORES);
    loadPath(settings, "samples", PATH_SAMPLES);
    loadPath(settings, "cheats", PATH_CHEATS);

    settings.endGroup();
    return true;
}

bool SupportDirsDialog::save()
{
    QSettings settings(util::appConfigName, QSettings::IniFormat);
    if (!settings.isWritable())
        return false;

    settings.beginGroup("support_directories");

    settings.setValue("previews", m_handlers[PATH_PREVIEWS].edit->text());
    settings.setValue("titles", m_handlers[PATH_TITLES].edit->text());
    settings.setValue("hiscores", m_handlers[PATH_HISCORES].edit->text());
    settings.setValue("samples", m_handlers[PATH_SAMPLES].edit->text());
    settings.setValue("cheats", m_handlers[PATH_CHEATS].edit->text());

    settings.endGroup();
    settings.sync();
}

void SupportDirsDialog::loadPath(QSettings &settings, QString name, int i)
{
    if (i >= PATH_MAX_HANDLERS)
        return;
    QString path(settings.value(name).toString());

    if (QFile(path).exists()) {
        m_handlers[i].edit->setText(path);
        m_handlers[i].editorToString();
    }
}
