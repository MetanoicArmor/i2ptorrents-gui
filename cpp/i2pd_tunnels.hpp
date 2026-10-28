#pragma once

#include <QString>
#include <QStringList>
#include <optional>

namespace i2p {

struct TorrentsTunnelConfig {
    QString torrentsDir;
    quint16 rpcPort = 9191;
    QString rpcPath = QStringLiteral("mytorrents");
    bool rpcAuth = false;
    QString rpcUser;
    QString rpcPass;
    QStringList trackers;
    bool dht = true;
    bool haveDhtFile = false;
    QString dhtFile;
    QString sectionName;
    QString confPath;
    QString dataDir;

    QString rpcUrl() const;
};

QStringList defaultI2pTrackers();
QString defaultI2pTrackersConf();
QStringList mergeI2pTrackers(const QStringList &preferred);

std::optional<TorrentsTunnelConfig> parseTorrentsTunnelFile(const QString &path);
QStringList defaultI2pdDataDirs();
QStringList tunnelFilesForDataDir(const QString &dataDir);
std::optional<TorrentsTunnelConfig> detectTorrentsTunnel(const QString &i2pdDir = {},
                                                         const QString &tunnelsConf = {});
bool writeTorrentsRpcAuth(const QString &path,
                          const QString &section,
                          bool auth,
                          const QString &user,
                          const QString &pass,
                          QString *error = nullptr);
bool writeTorrentsSection(const QString &path,
                          const QString &section,
                          const TorrentsTunnelConfig &values,
                          bool createSection,
                          QString *error = nullptr);

} // namespace i2p
