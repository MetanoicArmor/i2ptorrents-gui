#include "i2pd_tunnels.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>

namespace i2p {

namespace {

QString homeDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}

QString expandUser(const QString &value)
{
    const QString raw = value.trimmed();
    if (raw.startsWith(QStringLiteral("~/"))) {
        return QDir(homeDirectory()).filePath(raw.mid(2));
    }
    if (raw == QStringLiteral("~")) {
        return homeDirectory();
    }
    return raw;
}

QString stripComment(QString line)
{
    const int hash = line.indexOf(QLatin1Char('#'));
    if (hash >= 0) {
        line = line.left(hash);
    }
    return line.trimmed();
}

QStringList candidateTunnelFiles()
{
    QStringList paths;
#if defined(Q_OS_WIN)
    if (qEnvironmentVariableIsSet("APPDATA")) {
        paths << QDir(QString::fromLocal8Bit(qgetenv("APPDATA"))).filePath(QStringLiteral("i2pd/tunnels.conf"));
    }
    if (qEnvironmentVariableIsSet("LOCALAPPDATA")) {
        paths << QDir(QString::fromLocal8Bit(qgetenv("LOCALAPPDATA")))
                     .filePath(QStringLiteral("i2pd/tunnels.conf"));
    }
#elif defined(Q_OS_MACOS)
    paths << QDir(homeDirectory()).filePath(QStringLiteral("Library/Application Support/i2pd/tunnels.conf"));
    paths << QDir(homeDirectory()).filePath(QStringLiteral(".i2pd/tunnels.conf"));
#else
    paths << QStringLiteral("/var/lib/i2pd/tunnels.conf");
    paths << QDir(homeDirectory()).filePath(QStringLiteral(".i2pd/tunnels.conf"));
    paths << QStringLiteral("/etc/i2pd/tunnels.conf");
    const QDir tunnelsDir(QStringLiteral("/etc/i2pd/tunnels.d"));
    if (tunnelsDir.exists()) {
        const QStringList confFiles =
            tunnelsDir.entryList({QStringLiteral("*.conf")}, QDir::Files, QDir::Name);
        for (const QString &name : confFiles) {
            paths << tunnelsDir.filePath(name);
        }
    }
#endif
    paths.removeDuplicates();
    return paths;
}

} // namespace

QString TorrentsTunnelConfig::rpcUrl() const
{
    QString path = rpcPath.trimmed();
    while (path.startsWith(QLatin1Char('/'))) {
        path = path.mid(1);
    }
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    if (path.isEmpty()) {
        path = QStringLiteral("mytorrents");
    }
    return QStringLiteral("http://127.0.0.1:%1/%2").arg(rpcPort).arg(path);
}

std::optional<TorrentsTunnelConfig> parseTorrentsTunnelFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }

    QString section;
    QString type;
    QString torrentsDir;
    QString rpcPath;
    QString trackersRaw;
    quint16 rpcPort = 9191;
    bool havePort = false;

    auto parseTrackers = [](const QString &raw) {
        QStringList out;
        for (const QString &part : raw.split(QRegularExpression(QStringLiteral("[,\\s]+")),
                                             Qt::SkipEmptyParts)) {
            const QString trimmed = part.trimmed();
            if (!trimmed.isEmpty()) {
                out << trimmed;
            }
        }
        return out;
    };

    auto finishSection = [&]() -> std::optional<TorrentsTunnelConfig> {
        if (type.compare(QStringLiteral("torrents"), Qt::CaseInsensitive) != 0) {
            return std::nullopt;
        }
        TorrentsTunnelConfig config;
        config.sectionName = section;
        config.confPath = QFileInfo(path).absoluteFilePath();
        config.torrentsDir = expandUser(torrentsDir);
        config.rpcPort = havePort ? rpcPort : quint16(9191);
        config.rpcPath = rpcPath.trimmed().isEmpty() ? QStringLiteral("mytorrents") : rpcPath.trimmed();
        config.trackers = parseTrackers(trackersRaw);
        return config;
    };

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        QString line = stripComment(stream.readLine());
        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            if (const auto found = finishSection()) {
                return found;
            }
            section = line.mid(1, line.size() - 2).trimmed();
            type.clear();
            torrentsDir.clear();
            rpcPath.clear();
            trackersRaw.clear();
            rpcPort = 9191;
            havePort = false;
            continue;
        }
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0) {
            continue;
        }
        const QString key = line.left(eq).trimmed().toLower();
        const QString value = line.mid(eq + 1).trimmed();
        if (key == QStringLiteral("type")) {
            type = value;
        } else if (key == QStringLiteral("torrentsdir")) {
            torrentsDir = value;
        } else if (key == QStringLiteral("rpcpath")) {
            rpcPath = value;
        } else if (key == QStringLiteral("trackers")) {
            trackersRaw = value;
        } else if (key == QStringLiteral("rpcport")) {
            bool ok = false;
            const int parsed = value.toInt(&ok);
            if (ok && parsed > 0 && parsed <= 65535) {
                rpcPort = static_cast<quint16>(parsed);
                havePort = true;
            }
        }
    }
    return finishSection();
}

std::optional<TorrentsTunnelConfig> detectTorrentsTunnel()
{
    for (const QString &path : candidateTunnelFiles()) {
        if (!QFileInfo::exists(path)) {
            continue;
        }
        if (const auto config = parseTorrentsTunnelFile(path)) {
            return config;
        }
    }
    return std::nullopt;
}

} // namespace i2p
