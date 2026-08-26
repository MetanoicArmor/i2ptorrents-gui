#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <optional>

namespace i2p {

enum class TorrentStatus {
    Stopped = 0,
    QueuedVerify = 1,
    Verifying = 2,
    QueuedDownload = 3,
    Downloading = 4,
    QueuedSeed = 5,
    Seeding = 6,
};

enum class FilePriority { Skip, Low, Normal, High };

struct TorrentFile {
    qint64 index = 0;
    QString name;
    quint64 length = 0;
    quint64 bytesCompleted = 0;
    bool wanted = true;
    qint64 priority = 0;

    FilePriority kind() const;
    QString displayName() const;
    QString progressLabel() const;
};

struct Peer {
    QString address;
    QString identHash;
    QString clientName;
    quint64 rateToClient = 0;
    quint64 rateToPeer = 0;
    QString flagStr;
    bool isIncoming = false;
    bool isDownloadingFrom = false;
    bool isUploadingTo = false;
    std::optional<double> progress;

    QString displayAddress() const;
    QString tooltipAddress() const;
    QString clipboardText() const;
    QString ratesDownLabel() const;
    QString ratesUpLabel() const;
    QString progressLabel() const;
};

struct Tracker {
    QString id;
    QString announce;
    int tier = 0;
};

struct Torrent {
    qint64 id = 0;
    QString name;
    TorrentStatus status = TorrentStatus::Stopped;
    quint64 totalSize = 0;
    quint64 leftUntilDone = 0;
    quint64 rateDownload = 0;
    quint64 rateUpload = 0;
    quint64 peersSendingToUs = 0;
    quint64 peersGettingFromUs = 0;
    quint64 pieceCount = 0;
    quint64 pieceSize = 0;
    QString hashString;
    bool finished = false;
    std::optional<double> percentDone;
    qint64 eta = -1;
    QVector<bool> pieces;
    QVector<TorrentFile> files;
    QVector<Tracker> trackers;

    quint64 completed() const;
    double progress() const;
    QString shortHash() const;
    QString statusLabel() const;
};

TorrentStatus torrentStatusFromRpc(qint64 value);
FilePriority filePriorityFromRpc(bool wanted, qint64 priority);
QVector<TorrentFile> parseTorrentFiles(const QJsonObject &obj);
QVector<Peer> parseTorrentPeers(const QJsonObject &obj);
QVector<Tracker> parseTorrentTrackers(const QJsonObject &obj);
void syncPeerCounts(Torrent &torrent, const QJsonObject &obj);
QString displayFileName(const QString &name);
QVector<bool> decodePieceBitfield(const QJsonValue &raw, quint64 pieceCount, bool finished);
QString formatBytes(quint64 value);
QString formatRate(quint64 value);
QString formatEta(qint64 seconds);
QString progressText(const Torrent &torrent);
std::optional<Torrent> torrentFromRpc(const QJsonValue &data);

} // namespace i2p
