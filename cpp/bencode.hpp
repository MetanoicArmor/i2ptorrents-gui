#pragma once

#include <QByteArray>
#include <QString>
#include <QVariant>
#include <QVariantHash>
#include <QVariantList>

namespace i2p {

QByteArray bencode(const QVariant &value);

inline QByteArray bencodeString(const QByteArray &value)
{
    return QByteArray::number(value.size()) + ':' + value;
}

inline QByteArray bencodeString(const QString &value)
{
    return bencodeString(value.toUtf8());
}

inline QByteArray bencodeInt(qint64 value)
{
    return 'i' + QByteArray::number(value) + 'e';
}

} // namespace i2p
