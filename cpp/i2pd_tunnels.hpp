#pragma once

#include <QString>
#include <QStringList>
#include <optional>

namespace i2p {

struct TorrentsTunnelConfig {
    QString torrentsDir;
    quint16 rpcPort = 9191;
    QString rpcPath = QStringLiteral("mytorrents");
    QStringList trackers;
    QString sectionName;
    QString confPath;

    QString rpcUrl() const;
};

std::optional<TorrentsTunnelConfig> parseTorrentsTunnelFile(const QString &path);
std::optional<TorrentsTunnelConfig> detectTorrentsTunnel();

} // namespace i2p
