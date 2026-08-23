#include "resources.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>

namespace i2p {

QString loadTextResource(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QStringList resourceSearchRoots()
{
    QStringList roots;
    const QString exe = QCoreApplication::applicationDirPath();
    if (!exe.isEmpty()) {
        roots << QDir(exe).absolutePath();
        roots << QDir(exe).filePath(QStringLiteral("../Resources"));
        const QFileInfo parent(exe);
        if (parent.dir().exists()) {
            roots << parent.dir().absolutePath();
        }
    }
    roots << QDir::currentPath();
    roots << QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral(".."));

    QStringList unique;
    QSet<QString> seen;
    for (const QString &root : roots) {
        const QString canonical = QDir(root).canonicalPath();
        if (canonical.isEmpty() || seen.contains(canonical)) {
            continue;
        }
        seen.insert(canonical);
        unique << canonical;
    }
    return unique;
}

QString resourcePath(const QString &name)
{
    for (const QString &root : resourceSearchRoots()) {
        const QString direct = QDir(root).filePath(name);
        if (QFileInfo::exists(direct)) {
            return direct;
        }
        const QString nested = QDir(root).filePath(QStringLiteral("assets/%1").arg(name));
        if (QFileInfo::exists(nested)) {
            return nested;
        }
    }
    return {};
}

QString resourceFontsDir()
{
    for (const QString &root : resourceSearchRoots()) {
        for (const QString &sub : {QStringLiteral("fonts"), QStringLiteral("assets/fonts")}) {
            const QDir dir(QDir(root).filePath(sub));
            if (!dir.exists()) {
                continue;
            }
            const QStringList fonts =
                dir.entryList({QStringLiteral("*.otf"), QStringLiteral("*.ttf")}, QDir::Files);
            if (!fonts.isEmpty()) {
                return dir.absolutePath();
            }
        }
    }
    return {};
}

QString applicationIconPath()
{
    for (const QString &name :
         {QStringLiteral("icon.png"), QStringLiteral("I2PTorrents.ico"), QStringLiteral("image.png")}) {
        const QString path = resourcePath(name);
        if (!path.isEmpty()) {
            return path;
        }
    }
    return {};
}

} // namespace i2p
