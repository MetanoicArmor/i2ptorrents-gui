#pragma once

#include <QDir>
#include <QString>
#include <QStringList>

namespace i2p {

QString loadTextResource(const QString &path);
QStringList resourceSearchRoots();
QString resourcePath(const QString &name);
QString resourceFontsDir();
QString applicationIconPath();

} // namespace i2p
