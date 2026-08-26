#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <atomic>
#include <functional>
#include <optional>

namespace i2p {

struct CreateTorrentRequest {
    QString sourcePath;
    QStringList trackers;
    quint32 pieceSize = 0; // 0 = auto
    bool isPrivate = false;
    QString comment;
    QString createdBy;
};

struct CreateTorrentProgress {
    quint64 pieceIndex = 0;
    quint64 pieceCount = 0;
};

using CreateTorrentProgressFn = std::function<void(const CreateTorrentProgress &)>;

quint32 defaultPieceSize(quint64 totalSize);
bool isLegalPieceSize(quint32 pieceSize);

std::optional<QByteArray> createTorrentMetainfo(const CreateTorrentRequest &request,
                                               QString *error,
                                               std::atomic_bool *cancel = nullptr,
                                               const CreateTorrentProgressFn &onProgress = {});

bool saveTorrentMetainfo(const QByteArray &metainfo, const QString &path, QString *error);

} // namespace i2p
