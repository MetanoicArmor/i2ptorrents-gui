#include "models.hpp"

#include "i18n.hpp"

#include <QByteArray>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonValue>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace i2p {

namespace {

QJsonValue valueOf(const QJsonObject &obj, const char *camel, const char *snake)
{
    if (obj.contains(camel)) {
        return obj.value(camel);
    }
    if (obj.contains(snake)) {
        return obj.value(snake);
    }
    return {};
}

std::optional<qint64> jsonInt64(const QJsonValue &value)
{
    if (value.isUndefined() || value.isNull()) {
        return std::nullopt;
    }
    if (value.isDouble()) {
        return static_cast<qint64>(value.toInteger());
    }
    return std::nullopt;
}

std::optional<quint64> jsonUInt64(const QJsonValue &value)
{
    if (value.isUndefined() || value.isNull()) {
        return std::nullopt;
    }
    if (value.isDouble()) {
        const qint64 n = value.toInteger();
        if (n >= 0) {
            return static_cast<quint64>(n);
        }
    }
    return std::nullopt;
}

std::optional<bool> jsonTruthy(const QJsonValue &value)
{
    if (value.isUndefined() || value.isNull()) {
        return std::nullopt;
    }
    if (value.isBool()) {
        return value.toBool();
    }
    if (value.isDouble()) {
        return value.toInteger() != 0;
    }
    return std::nullopt;
}

QString jsonString(const QJsonValue &value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isUndefined() || value.isNull()) {
        return {};
    }
    return value.toVariant().toString();
}

} // namespace

TorrentStatus torrentStatusFromRpc(qint64 value)
{
    switch (value) {
    case 1:
        return TorrentStatus::QueuedVerify;
    case 2:
        return TorrentStatus::Verifying;
    case 3:
        return TorrentStatus::QueuedDownload;
    case 4:
        return TorrentStatus::Downloading;
    case 5:
        return TorrentStatus::QueuedSeed;
    case 6:
        return TorrentStatus::Seeding;
    default:
        return TorrentStatus::Stopped;
    }
}

FilePriority filePriorityFromRpc(bool wanted, qint64 priority)
{
    if (!wanted) {
        return FilePriority::Skip;
    }
    if (priority == -1) {
        return FilePriority::Low;
    }
    if (priority == 1) {
        return FilePriority::High;
    }
    return FilePriority::Normal;
}

FilePriority TorrentFile::kind() const
{
    return filePriorityFromRpc(wanted, priority);
}

QString TorrentFile::displayName() const
{
    return displayFileName(name);
}

QString TorrentFile::progressLabel() const
{
    if (length == 0) {
        return QStringLiteral("—");
    }
    const double percent =
        std::clamp(static_cast<double>(bytesCompleted) / static_cast<double>(length) * 100.0, 0.0, 100.0);
    return QStringLiteral("%1%").arg(percent, 0, 'f', 0);
}

QString Peer::displayAddress() const
{
    if (!address.isEmpty()) {
        return address;
    }
    if (!identHash.isEmpty()) {
        return identHash.left(8);
    }
    return QStringLiteral("—");
}

QString Peer::tooltipAddress() const
{
    if (!identHash.isEmpty()) {
        return identHash;
    }
    return address;
}

QString Peer::ratesDownLabel() const
{
    return formatRate(rateToClient);
}

QString Peer::ratesUpLabel() const
{
    return formatRate(rateToPeer);
}

namespace {

bool jsonBoolField(const QJsonObject &obj, std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        if (obj.contains(key)) {
            return jsonTruthy(obj.value(key)).value_or(false);
        }
    }
    return false;
}

std::optional<Peer> peerFromRpc(const QJsonValue &data)
{
    if (!data.isObject()) {
        return std::nullopt;
    }
    const QJsonObject obj = data.toObject();
    Peer peer;
    peer.address = jsonString(valueOf(obj, "address", "address")).trimmed();
    peer.identHash = jsonString(valueOf(obj, "identHash", "ident_hash")).trimmed();
    peer.clientName = jsonString(valueOf(obj, "clientName", "client_name")).trimmed();
    if (peer.clientName.isEmpty()) {
        peer.clientName = QStringLiteral("Unknown");
    }
    peer.rateToClient = jsonUInt64(valueOf(obj, "rateToClient", "rate_to_client")).value_or(0);
    peer.rateToPeer = jsonUInt64(valueOf(obj, "rateToPeer", "rate_to_peer")).value_or(0);
    peer.flagStr = jsonString(valueOf(obj, "flagStr", "flag_str")).trimmed();
    peer.isIncoming = jsonBoolField(obj, {"isIncoming", "is_incoming"});
    peer.isDownloadingFrom = jsonBoolField(obj, {"isDowloadingFrom", "isDownloadingFrom", "is_downloading_from"});
    peer.isUploadingTo = jsonBoolField(obj, {"isUploading_to", "isUploadingTo", "is_uploading_to"});
    return peer;
}

} // namespace

QVector<Peer> parseTorrentPeers(const QJsonObject &obj)
{
    const QJsonValue peersValue = valueOf(obj, "peers", "peers");
    if (!peersValue.isArray()) {
        return {};
    }
    QVector<Peer> peers;
    for (const QJsonValue &item : peersValue.toArray()) {
        if (const std::optional<Peer> peer = peerFromRpc(item)) {
            peers.push_back(*peer);
        }
    }
    return peers;
}

void syncPeerCounts(Torrent &torrent, const QJsonObject &obj)
{
    const QVector<Peer> peers = parseTorrentPeers(obj);
    if (peers.isEmpty()) {
        return;
    }
    if (torrent.peersSendingToUs == 0) {
        quint64 count = 0;
        for (const Peer &peer : peers) {
            if (peer.isDownloadingFrom) {
                ++count;
            }
        }
        if (count > 0) {
            torrent.peersSendingToUs = count;
        }
    }
    if (torrent.peersGettingFromUs == 0) {
        quint64 count = 0;
        for (const Peer &peer : peers) {
            if (peer.isUploadingTo) {
                ++count;
            }
        }
        if (count > 0) {
            torrent.peersGettingFromUs = count;
        }
    }
}

quint64 Torrent::completed() const
{
    return totalSize >= leftUntilDone ? totalSize - leftUntilDone : 0;
}

double Torrent::progress() const
{
    if (totalSize == 0) {
        return finished ? 1.0 : 0.0;
    }
    return std::clamp(static_cast<double>(completed()) / static_cast<double>(totalSize), 0.0, 1.0);
}

QString Torrent::shortHash() const
{
    const QString digest = hashString.trimmed();
    if (digest.size() <= 16) {
        return digest;
    }
    return digest.left(10) + QChar(0x2026) + digest.right(6);
}

QString Torrent::statusLabel() const
{
    switch (status) {
    case TorrentStatus::Stopped:
        return trKey(QStringLiteral("status_stopped"));
    case TorrentStatus::QueuedVerify:
        return trKey(QStringLiteral("status_queued_verify"));
    case TorrentStatus::Verifying:
        return trKey(QStringLiteral("status_verifying"));
    case TorrentStatus::QueuedDownload:
        return trKey(QStringLiteral("status_queued_download"));
    case TorrentStatus::Downloading:
        return trKey(QStringLiteral("status_downloading"));
    case TorrentStatus::QueuedSeed:
        return trKey(QStringLiteral("status_queued_seed"));
    case TorrentStatus::Seeding:
        return trKey(QStringLiteral("status_seeding"));
    }
    return trKey(QStringLiteral("status_stopped"));
}

QVector<TorrentFile> parseTorrentFiles(const QJsonObject &obj)
{
    const QJsonValue filesValue = valueOf(obj, "files", "files");
    if (!filesValue.isArray() || filesValue.toArray().isEmpty()) {
        return {};
    }
    const QJsonArray rows = filesValue.toArray();
    QJsonArray wanted;
    QJsonArray priorities;
    const QJsonValue wantedValue = valueOf(obj, "wanted", "wanted");
    if (wantedValue.isArray()) {
        wanted = wantedValue.toArray();
    }
    const QJsonValue prioValue = valueOf(obj, "priorities", "priorities");
    if (prioValue.isArray()) {
        priorities = prioValue.toArray();
    }

    QVector<TorrentFile> files;
    files.reserve(rows.size());
    for (int index = 0; index < rows.size(); ++index) {
        const QJsonObject file = rows.at(index).toObject();
        TorrentFile row;
        row.index = index;
        row.name = jsonString(valueOf(file, "name", "name"));
        row.length = jsonUInt64(valueOf(file, "length", "length")).value_or(0);
        row.bytesCompleted =
            jsonUInt64(valueOf(file, "bytesCompleted", "bytes_completed")).value_or(0);
        row.wanted = wanted.size() > index ? jsonTruthy(wanted.at(index)).value_or(true) : true;
        row.priority =
            priorities.size() > index ? jsonInt64(priorities.at(index)).value_or(0) : 0;
        files.push_back(row);
    }
    return files;
}

QString displayFileName(const QString &name)
{
    const QFileInfo path(name);
    if (path.isAbsolute() || name.startsWith(QLatin1Char('/')) || name.startsWith(QLatin1Char('\\'))) {
        const QString base = path.fileName();
        return base.isEmpty() ? name : base;
    }
    return QString(name).replace(QLatin1Char('\\'), QLatin1Char('/'));
}

QVector<bool> decodePieceBitfield(const QJsonValue &raw, quint64 pieceCount, bool finished)
{
    const int count = static_cast<int>(pieceCount);
    if (count <= 0) {
        return {};
    }
    QByteArray data;
    if (raw.isString()) {
        const QString text = raw.toString().trimmed();
        if (text.isEmpty()) {
            return finished ? QVector<bool>(count, true) : QVector<bool>();
        }
        data = QByteArray::fromBase64(text.toUtf8());
    } else if (raw.isArray()) {
        for (const QJsonValue &item : raw.toArray()) {
            data.append(static_cast<char>(item.toInt(0)));
        }
    } else {
        return finished ? QVector<bool>(count, true) : QVector<bool>();
    }

    QVector<bool> bits;
    bits.reserve(count);
    for (int index = 0; index < count; ++index) {
        const int offset = index / 8;
        const int bit = 0x80 >> (index % 8);
        bits.push_back(offset < data.size() && (static_cast<unsigned char>(data[offset]) & bit) != 0);
    }
    return bits;
}

QString formatBytes(quint64 value)
{
    double size = static_cast<double>(value);
    const QStringList keys = {QStringLiteral("unit_b"),
                              QStringLiteral("unit_kb"),
                              QStringLiteral("unit_mb"),
                              QStringLiteral("unit_gb"),
                              QStringLiteral("unit_tb")};
    for (int index = 0; index < keys.size(); ++index) {
        if (size < 1024.0 || index == keys.size() - 1) {
            const QString label = trKey(keys[index]);
            if (keys[index] == QStringLiteral("unit_b")) {
                return QStringLiteral("%1 %2").arg(size, 0, 'f', 0).arg(label);
            }
            return QStringLiteral("%1 %2").arg(size, 0, 'f', 1).arg(label);
        }
        size /= 1024.0;
    }
    return QStringLiteral("%1 %2").arg(size, 0, 'f', 1).arg(trKey(QStringLiteral("unit_tb")));
}

QString formatRate(quint64 value)
{
    return formatBytes(value) + trKey(QStringLiteral("per_second"));
}

QString progressText(const Torrent &torrent)
{
    QHash<QString, QString> args;
    args.insert(QStringLiteral("percent"), QString::number(torrent.progress() * 100.0, 'f', 1));
    args.insert(QStringLiteral("done"), formatBytes(torrent.completed()));
    args.insert(QStringLiteral("total"), formatBytes(torrent.totalSize));
    return trArgs(QStringLiteral("progress_of"), args);
}

std::optional<Torrent> torrentFromRpc(const QJsonValue &data)
{
    if (!data.isObject()) {
        return std::nullopt;
    }
    const QJsonObject obj = data.toObject();
    const std::optional<qint64> id = jsonInt64(valueOf(obj, "id", "id"));
    if (!id.has_value()) {
        return std::nullopt;
    }

    Torrent torrent;
    torrent.id = *id;
    torrent.status = torrentStatusFromRpc(jsonInt64(valueOf(obj, "status", "status")).value_or(0));
    if (const std::optional<quint64> total = jsonUInt64(valueOf(obj, "totalSize", "total_size"))) {
        torrent.totalSize = *total;
    } else if (const std::optional<quint64> sizeWhenDone =
                   jsonUInt64(valueOf(obj, "sizeWhenDone", "size_when_done"))) {
        torrent.totalSize = *sizeWhenDone;
    }
    torrent.leftUntilDone =
        jsonUInt64(valueOf(obj, "leftUntilDone", "left_until_done")).value_or(0);
    torrent.rateDownload = jsonUInt64(valueOf(obj, "rateDownload", "rate_download")).value_or(0);
    torrent.rateUpload = jsonUInt64(valueOf(obj, "rateUpload", "rate_upload")).value_or(0);
    torrent.peersSendingToUs =
        jsonUInt64(valueOf(obj, "peersSendingToUs", "peers_sending_to_us")).value_or(0);
    torrent.peersGettingFromUs =
        jsonUInt64(valueOf(obj, "peersGettingFromUs", "peers_getting_from_us")).value_or(0);
    torrent.pieceCount = jsonUInt64(valueOf(obj, "pieceCount", "piece_count")).value_or(0);
    torrent.pieceSize = jsonUInt64(valueOf(obj, "pieceSize", "piece_size")).value_or(0);
    torrent.hashString = jsonString(valueOf(obj, "hashString", "hash_string")).trimmed();
    torrent.finished = jsonTruthy(valueOf(obj, "isFinished", "is_finished")).value_or(false);
    const QString name = jsonString(obj.value(QStringLiteral("name")));
    torrent.name = name.isEmpty() ? trKey(QStringLiteral("untitled")) : name;
    torrent.pieces = decodePieceBitfield(
        obj.contains(QStringLiteral("pieces")) ? obj.value(QStringLiteral("pieces")) : QJsonValue(),
        torrent.pieceCount,
        torrent.finished);
    torrent.files = parseTorrentFiles(obj);
    syncPeerCounts(torrent, obj);
    return torrent;
}

} // namespace i2p
