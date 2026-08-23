#pragma once

#include "models.hpp"

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVector>
#include <functional>
#include <optional>
#include <utility>

namespace i2p {

inline const char *RPC_FIELDS[] = {
    "id",           "name",           "status",         "isFinished",      "sizeWhenDone",
    "leftUntilDone", "rateDownload",  "rateUpload",     "peersGettingFromUs", "peersSendingToUs",
    "peers",        "pieceCount",     "pieceSize",      "totalSize",       "hashString",      "pieces",
};

inline const char *RPC_FILE_FIELDS[] = {"id", "files", "wanted", "priorities"};
inline const char *RPC_PEER_FIELDS[] = {"id", "peers"};

constexpr int RPC_FIELD_COUNT = 16;
constexpr int RPC_FILE_FIELD_COUNT = 4;
constexpr int RPC_PEER_FIELD_COUNT = 2;

std::optional<QString> normalizeRpcUrl(const QString &value, QString *error = nullptr);
bool rpcMethodUnsupported(const QString &message);

class RpcClient {
public:
    using PostHandler = std::function<QByteArray(const QString &endpoint,
                                                 const QByteArray &body,
                                                 QString *error)>;

    explicit RpcClient(const QString &endpoint);
    RpcClient(QString endpoint, PostHandler handler);

    QString endpoint() const { return endpoint_; }

    QJsonValue call(const QString &method, const QJsonObject &arguments, QString *error = nullptr);
    QVector<Torrent> getTorrents(bool detailed, QString *error = nullptr);
    QVector<TorrentFile> getTorrentFiles(qint64 torrentId, QString *error = nullptr);
    QVector<Peer> getTorrentPeers(qint64 torrentId, QString *error = nullptr);
    QJsonObject addTorrentBytes(const QByteArray &content, QString *error = nullptr);
    QJsonObject addTorrentPath(const QString &path, QString *error = nullptr);
    bool removeTorrent(qint64 torrentId, bool deleteData, QString *error = nullptr);
    bool setFilePriority(qint64 torrentId,
                         qint64 index,
                         bool wanted,
                         qint64 priority,
                         QString *error = nullptr);

private:
    QString endpoint_;
    PostHandler post_;
    quint64 tag_ = 0;

    static QByteArray defaultPost(const QString &endpoint, const QByteArray &body, QString *error);
    static QJsonValue rpcArguments(const QJsonObject &payload);
    QVector<Torrent> torrentRows(const QJsonValue &result, QString *error);
};

} // namespace i2p
