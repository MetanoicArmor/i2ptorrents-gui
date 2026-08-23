#pragma once

#include <QHash>
#include <QString>

namespace i2p {

QString normalizeLanguage(const QString &value);
void setLanguage(const QString &value);
QString language();
QString trKey(const QString &key);
QString trArgs(const QString &key, const QHash<QString, QString> &args);

} // namespace i2p
