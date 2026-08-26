#include "bencode.hpp"

#include <QMetaType>
#include <QStringList>
#include <algorithm>

namespace i2p {

namespace {

QByteArray encode(const QVariant &value);

QByteArray encodeDict(const QVariantHash &dict)
{
    QStringList keys = dict.keys();
    std::sort(keys.begin(), keys.end());
    QByteArray out;
    out += 'd';
    for (const QString &key : keys) {
        out += bencodeString(key);
        out += encode(dict.value(key));
    }
    out += 'e';
    return out;
}

QByteArray encodeList(const QVariantList &list)
{
    QByteArray out;
    out += 'l';
    for (const QVariant &item : list) {
        out += encode(item);
    }
    out += 'e';
    return out;
}

QByteArray encode(const QVariant &value)
{
    switch (value.userType()) {
    case QMetaType::QByteArray:
        return bencodeString(value.toByteArray());
    case QMetaType::QString:
        return bencodeString(value.toString());
    case QMetaType::Bool:
        return bencodeInt(value.toBool() ? 1 : 0);
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return bencodeInt(value.toLongLong());
    case QMetaType::QVariantList:
        return encodeList(value.toList());
    case QMetaType::QVariantHash:
        return encodeDict(value.toHash());
    case QMetaType::QVariantMap: {
        QVariantHash hash;
        const QVariantMap map = value.toMap();
        for (auto it = map.begin(); it != map.end(); ++it) {
            hash.insert(it.key(), it.value());
        }
        return encodeDict(hash);
    }
    default:
        if (value.canConvert<QByteArray>()) {
            return bencodeString(value.toByteArray());
        }
        if (value.canConvert<qint64>()) {
            return bencodeInt(value.toLongLong());
        }
        return bencodeString(value.toString());
    }
}

} // namespace

QByteArray bencode(const QVariant &value)
{
    return encode(value);
}

} // namespace i2p
