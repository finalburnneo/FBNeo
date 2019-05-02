#include <QFileDialog>
#include <QDebug>
#include "qutil.h"

namespace util {

const QString appConfigName("config/fbaqt.ini");

void fixPath(QString &path)
{
    if (!path.isEmpty()) {
        QChar lastChar = path.at(path.size() - 1);
        if (lastChar != QChar('\\') && lastChar != QChar('/'))
            path.append('/');
    }
}

void PathHandler::editorToString()
{
    if (str == nullptr || edit == nullptr)
        return;
    QByteArray bytes;
    QString path = edit->text();
    fixPath(path);
    bytes = path.toLocal8Bit();
    _sntprintf(str, MAX_PATH, bytes.data());
}

void PathHandler::stringToEditor()
{
    if (edit == nullptr)
        return;
    edit->setText(QString(str));
}

void PathHandler::browse(QWidget *parent)
{
    QString path = QFileDialog::getExistingDirectory(parent);
    if (path.isEmpty())
        return;
    if (edit == nullptr)
        return;
    edit->setText(path);
}

QString loadText(const QString &fileName)
{
    QFile file(fileName);
    if (file.exists()) {
        file.open(QFile::ReadOnly | QFile::Text);
        return QString(file.readAll());
    }
    return QString();
}

struct FlagDescription {
    unsigned flags;
    QString name;
};

static FlagDescription genreDescription[] = {
    { GBF_HORSHOOT,         QT_TR_NOOP("Shooter / Horizontal / Sh'mup") },
    { GBF_VERSHOOT,         QT_TR_NOOP("Shooter / Vertical / Sh'mup") },
    { GBF_SCRFIGHT,         QT_TR_NOOP("Fighting / Beat 'em Up") },
    { GBF_VSFIGHT,          QT_TR_NOOP("Fighting / Versus") },
    { GBF_BIOS,             QT_TR_NOOP("BIOS") },
    { GBF_BREAKOUT,         QT_TR_NOOP("Breakout") },
    { GBF_CASINO,           QT_TR_NOOP("Casino") },
    { GBF_BALLPADDLE,       QT_TR_NOOP("Ball & Paddle") },
    { GBF_MAZE,             QT_TR_NOOP("Maze") },
    { GBF_MINIGAMES,        QT_TR_NOOP("Mini-Games") },
    { GBF_PINBALL,          QT_TR_NOOP("Pinball") },
    { GBF_PLATFORM,         QT_TR_NOOP("Platformer") },
    { GBF_PUZZLE,           QT_TR_NOOP("Puzzle") },
    { GBF_QUIZ,             QT_TR_NOOP("Quiz") },
    { GBF_SPORTSFOOTBALL,   QT_TR_NOOP("Sports / Football") },
    { GBF_SPORTSMISC,       QT_TR_NOOP("Sports") },
    { GBF_MISC,             QT_TR_NOOP("Misc") },
    { GBF_MAHJONG,          QT_TR_NOOP("Mahjong") },
    { GBF_RACING,           QT_TR_NOOP("Racing") },
    { GBF_SHOOT,            QT_TR_NOOP("Shooter / Run 'n Gun") },
};
static const int genreDescriptionSize = sizeof(genreDescription) / sizeof(FlagDescription);

static FlagDescription familyDescription[] = {
    { FBF_MSLUG,        QT_TR_NOOP("Metal Slug") },
    { FBF_19XX,         QT_TR_NOOP("19XX") },
    { FBF_DSTLK,        QT_TR_NOOP("Darkstalkers") },
    { FBF_FATFURY,      QT_TR_NOOP("Fatal Fury") },
    { FBF_KOF,          QT_TR_NOOP("King of Fighters") },
    { FBF_PWRINST,      QT_TR_NOOP("Power Instinct") },
    { FBF_SAMSHO,       QT_TR_NOOP("Samurai Shodown") },
    { FBF_SF,           QT_TR_NOOP("Street Fighter") },
    { FBF_SONICWI,      QT_TR_NOOP("Aero Fighters") },
	{ FBF_SONIC,   		QT_TR_NOOP("Sonic the Hedgehog") },
};
static const int familyDescriptionSize = sizeof(familyDescription) / sizeof(FlagDescription);


static FlagDescription boardDescription[] = {
    { BDF_PROTOTYPE,    QT_TR_NOOP("Prototype") },
    { BDF_BOOTLEG,      QT_TR_NOOP("Bootleg") },
    { BDF_HACK,         QT_TR_NOOP("Hack") },
    { BDF_DEMO,         QT_TR_NOOP("Demo") },
    { BDF_HOMEBREW,     QT_TR_NOOP("Homebrew") },

};
static const int boardDescriptionSize = sizeof(boardDescription) / sizeof(FlagDescription);

QString decorateGenre()
{
    unsigned genre = BurnDrvGetGenreFlags();
    unsigned family = BurnDrvGetFamilyFlags();

    QStringList decorated;

    for (int i = 0; i < genreDescriptionSize; i++) {
        if (genreDescription[i].flags & genre)
            decorated << genreDescription[i].name;
    }

    for (int i = 0; i < familyDescriptionSize; i++) {
        if (familyDescription[i].flags & family)
            decorated << familyDescription[i].name;
    }

    return decorated.join(", ");
}

QString decorateRomInfo()
{
    unsigned flags = BurnDrvGetFlags();
    QStringList decorated;

    for (int i = 0; i < boardDescriptionSize; i++) {
        if (boardDescription[i].flags & flags)
            decorated << boardDescription[i].name;
    }

    decorated << QObject::tr("%0 players max").arg(BurnDrvGetMaxPlayers());

    QString board(BurnDrvGetText(DRV_BOARDROM));
    if (!board.isEmpty())
        decorated << QObject::tr("uses board-ROMs from %0").arg(board);

    return decorated.join(", ");
}

}
