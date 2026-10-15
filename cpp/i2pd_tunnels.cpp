#include "i2pd_tunnels.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
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

bool parseConfigBool(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized == QStringLiteral("true") || normalized == QStringLiteral("yes") ||
           normalized == QStringLiteral("1") || normalized == QStringLiteral("on");
}

bool rpcCredentialAllowed(const QString &value)
{
    return !value.contains(QLatin1Char('#')) && !value.contains(QLatin1Char('\n')) &&
           !value.contains(QLatin1Char('\r'));
}

QString configKey(const QString &line)
{
    const QString probe = stripComment(line);
    const int eq = probe.indexOf(QLatin1Char('='));
    if (eq <= 0) {
        return {};
    }
    return probe.left(eq).trimmed().toLower();
}

QString configValue(const QString &line)
{
    const QString probe = stripComment(line);
    const int eq = probe.indexOf(QLatin1Char('='));
    if (eq < 0) {
        return {};
    }
    QString value = probe.mid(eq + 1).trimmed();
    if (value.size() >= 2 && value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"'))) {
        value = value.mid(1, value.size() - 2);
    }
    return value;
}

QString resolveConfigPath(const QString &baseDir, const QString &value)
{
    const QString expanded = expandUser(value);
    if (expanded.isEmpty()) {
        return {};
    }
    if (QDir::isAbsolutePath(expanded)) {
        return QDir::cleanPath(expanded);
    }
    return QDir::cleanPath(QDir(baseDir).filePath(expanded));
}

void appendConfFiles(QStringList *paths, const QString &directory)
{
    const QDir dir(directory);
    if (!dir.exists()) {
        return;
    }
    const QStringList names = dir.entryList({QStringLiteral("*.conf")}, QDir::Files, QDir::Name);
    for (const QString &name : names) {
        *paths << dir.filePath(name);
    }
}

QHash<QString, QString> readI2pdOptions(const QString &path)
{
    QHash<QString, QString> options;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return options;
    }
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        const QString key = configKey(line);
        if (key.isEmpty()) {
            continue;
        }
        options.insert(key, configValue(line));
    }
    return options;
}

QString dataDirForTunnelFile(const QString &path)
{
    const QFileInfo info(path);
    const QString parent = info.absolutePath();
    const QString folder = QFileInfo(parent).fileName();
    if (folder == QStringLiteral("tunnels.d") || folder == QStringLiteral("tunnels.conf.d")) {
        return QFileInfo(parent).absolutePath();
    }
    return parent;
}

bool tunnelFileWritable(const QString &path)
{
    const QFileInfo file(path);
    if (!file.exists() || !file.isFile()) {
        return false;
    }
    const QFileInfo directory(file.absolutePath());
    return file.isWritable() && directory.isWritable();
}

void collectTunnelFiles(const QString &dataDir, QStringList *paths, QSet<QString> *seenDirs)
{
    const QString root = QDir::cleanPath(expandUser(dataDir));
    if (root.isEmpty() || seenDirs->contains(root)) {
        return;
    }
    seenDirs->insert(root);
    const QHash<QString, QString> options = readI2pdOptions(QDir(root).filePath(QStringLiteral("i2pd.conf")));
    const QString tunconf = options.value(QStringLiteral("tunconf"), options.value(QStringLiteral("tunnelsconf")));
    if (!tunconf.isEmpty()) {
        *paths << resolveConfigPath(root, tunconf);
    }
    *paths << QDir(root).filePath(QStringLiteral("tunnels.conf"));
    const QString tunnelsDir = options.value(QStringLiteral("tunnelsdir"));
    if (!tunnelsDir.isEmpty()) {
        appendConfFiles(paths, resolveConfigPath(root, tunnelsDir));
    } else {
        appendConfFiles(paths, QDir(root).filePath(QStringLiteral("tunnels.d")));
        appendConfFiles(paths, QDir(root).filePath(QStringLiteral("tunnels.conf.d")));
    }
    const QString nested = options.value(QStringLiteral("datadir"));
    if (!nested.isEmpty()) {
        collectTunnelFiles(resolveConfigPath(root, nested), paths, seenDirs);
    }
}

struct TunnelKeyUpdate {
    QString key;
    QString value;
};

bool writeTunnelKeys(const QString &path,
                     const QString &section,
                     const QList<TunnelKeyUpdate> &updates,
                     bool createSection,
                     QString *error)
{
    const auto fail = [&](const QString &reason) {
        if (error != nullptr) {
            *error = reason;
        }
        return false;
    };
    for (const TunnelKeyUpdate &update : updates) {
        if (!rpcCredentialAllowed(update.value)) {
            return fail(QStringLiteral("invalid"));
        }
    }

    const QString sectionName = section.trimmed();
    const QFileInfo info(path);
    if (sectionName.isEmpty() || !info.exists() || !info.isFile()) {
        return fail(QStringLiteral("missing"));
    }
    if (!tunnelFileWritable(path)) {
        return fail(QStringLiteral("readonly"));
    }

    QFile input(path);
    if (!input.open(QIODevice::ReadOnly)) {
        return fail(input.errorString());
    }
    const QByteArray raw = input.readAll();
    input.close();

    const bool crlf = raw.contains("\r\n");
    const QString text = QString::fromUtf8(raw);
    QStringList lines = text.split(QLatin1Char('\n'));
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }

    int sectionStart = -1;
    int sectionEnd = lines.size();
    for (int i = 0; i < lines.size(); ++i) {
        const QString probe = stripComment(lines.at(i));
        if (!probe.startsWith(QLatin1Char('[')) || !probe.endsWith(QLatin1Char(']'))) {
            continue;
        }
        const QString name = probe.mid(1, probe.size() - 2).trimmed();
        if (sectionStart < 0 && name == sectionName) {
            sectionStart = i;
        } else if (sectionStart >= 0) {
            sectionEnd = i;
            break;
        }
    }

    if (sectionStart < 0) {
        if (!createSection) {
            return fail(QStringLiteral("missing"));
        }
        int insertAt = lines.size();
        if (!lines.isEmpty() && lines.last().isEmpty()) {
            insertAt = lines.size() - 1;
        }
        if (insertAt > 0 && !lines.at(insertAt - 1).trimmed().isEmpty()) {
            lines.insert(insertAt++, QString());
        }
        lines.insert(insertAt++, QStringLiteral("[%1]").arg(sectionName));
        lines.insert(insertAt++, QStringLiteral("type=torrents"));
        for (const TunnelKeyUpdate &update : updates) {
            lines.insert(insertAt++, update.key + QLatin1Char('=') + update.value);
        }
    } else {
        QHash<QString, bool> seen;
        for (int i = sectionStart + 1; i < sectionEnd; ++i) {
            const QString key = configKey(lines.at(i));
            for (const TunnelKeyUpdate &update : updates) {
                if (key == update.key) {
                    lines[i] = update.key + QLatin1Char('=') + update.value;
                    seen.insert(key, true);
                }
            }
        }
        int insertAt = sectionEnd;
        while (insertAt > sectionStart + 1 && lines.at(insertAt - 1).trimmed().isEmpty()) {
            --insertAt;
        }
        int offset = 0;
        for (const TunnelKeyUpdate &update : updates) {
            if (seen.value(update.key)) {
                continue;
            }
            lines.insert(insertAt + offset, update.key + QLatin1Char('=') + update.value);
            ++offset;
        }
    }

    const QString newline = crlf ? QStringLiteral("\r\n") : QStringLiteral("\n");
    const QByteArray encoded = lines.join(newline).toUtf8();
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly)) {
        return fail(output.errorString().isEmpty() ? QStringLiteral("readonly") : output.errorString());
    }
    if (output.write(encoded) != encoded.size()) {
        output.cancelWriting();
        return fail(output.errorString());
    }
    if (!output.commit()) {
        return fail(output.errorString());
    }
    return true;
}

} // namespace

QStringList defaultI2pTrackers()
{
    return {
        QStringLiteral("http://tracker2.postman.i2p/announce.php"),
        QStringLiteral("http://opentracker.dg2.i2p/a"),
        QStringLiteral("http://opentracker.r4sas.i2p/announce"),
        QStringLiteral("http://opentracker.simp.i2p/a"),
        QStringLiteral("http://tracker.eeptorrent.i2p/a"),
        QStringLiteral("http://opentracker.fattydove.i2p/a"),
        QStringLiteral("http://opentracker-actix.i2p/announce"),
        QStringLiteral("http://opentracker.bt.i2p/a"),
        QStringLiteral("http://vern.i2p:6969/announce"),
        QStringLiteral("http://atia.i2p/announce"),
        QStringLiteral("http://sigmatracker.i2p/a"),
        QStringLiteral("http://opentracker.skank.i2p/a"),
        QStringLiteral("http://omitracker.i2p/announce"),
        QStringLiteral("http://psa.i2p/a"),
        QStringLiteral("http://tracker.nyaa2p.i2p/announce"),
    };
}

QString defaultI2pTrackersConf()
{
    return defaultI2pTrackers().join(QLatin1Char(','));
}

QStringList mergeI2pTrackers(const QStringList &preferred)
{
    QStringList out;
    const auto append = [&out](const QString &url) {
        const QString trimmed = url.trimmed();
        if (trimmed.isEmpty()) {
            return;
        }
        for (const QString &existing : out) {
            if (existing.compare(trimmed, Qt::CaseInsensitive) == 0) {
                return;
            }
        }
        out << trimmed;
    };
    for (const QString &url : preferred) {
        append(url);
    }
    for (const QString &url : defaultI2pTrackers()) {
        append(url);
    }
    return out;
}

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
    QString rpcUser;
    QString rpcPass;
    quint16 rpcPort = 9191;
    bool havePort = false;
    bool rpcAuth = false;

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
        config.rpcAuth = rpcAuth;
        config.rpcUser = rpcUser;
        config.rpcPass = rpcPass;
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
            rpcUser.clear();
            rpcPass.clear();
            rpcPort = 9191;
            havePort = false;
            rpcAuth = false;
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
        } else if (key == QStringLiteral("rpcauth")) {
            rpcAuth = parseConfigBool(value);
        } else if (key == QStringLiteral("rpcuser")) {
            rpcUser = value;
        } else if (key == QStringLiteral("rpcpass")) {
            rpcPass = value;
        }
    }
    return finishSection();
}

std::optional<TorrentsTunnelConfig> detectTorrentsTunnel(const QString &i2pdDir, const QString &tunnelsConf)
{
    const auto choose = [](const QStringList &paths) -> std::optional<TorrentsTunnelConfig> {
        std::optional<TorrentsTunnelConfig> fallback;
        QSet<QString> seen;
        for (const QString &raw : paths) {
            if (raw.trimmed().isEmpty() || !QFileInfo::exists(raw)) {
                continue;
            }
            const QString path = QFileInfo(raw).absoluteFilePath();
            if (seen.contains(path)) {
                continue;
            }
            seen.insert(path);
            auto config = parseTorrentsTunnelFile(path);
            if (!config.has_value()) {
                continue;
            }
            config->dataDir = dataDirForTunnelFile(path);
            if (!fallback.has_value()) {
                fallback = config;
            }
            if (tunnelFileWritable(path)) {
                return config;
            }
        }
        return fallback;
    };

    const QString explicitConf = tunnelsConf.trimmed();
    if (!explicitConf.isEmpty()) {
        const QString path = expandUser(explicitConf);
        if (!QFileInfo::exists(path)) {
            return std::nullopt;
        }
        auto config = parseTorrentsTunnelFile(path);
        if (config.has_value()) {
            config->dataDir = dataDirForTunnelFile(path);
        }
        return config;
    }

    QStringList paths;
    QSet<QString> seenDirs;
    const QString explicitDir = i2pdDir.trimmed();
    if (!explicitDir.isEmpty()) {
        collectTunnelFiles(explicitDir, &paths, &seenDirs);
    } else {
        for (const QString &dir : defaultI2pdDataDirs()) {
            collectTunnelFiles(dir, &paths, &seenDirs);
        }
    }
    paths.removeDuplicates();
    return choose(paths);
}

QStringList defaultI2pdDataDirs()
{
    QStringList dirs;
#if defined(Q_OS_WIN)
    if (qEnvironmentVariableIsSet("APPDATA")) {
        dirs << QDir(QString::fromLocal8Bit(qgetenv("APPDATA"))).filePath(QStringLiteral("i2pd"));
    }
    const QString programData = qEnvironmentVariable("ProgramData");
    if (!programData.isEmpty()) {
        dirs << QDir(programData).filePath(QStringLiteral("i2pd"));
    }
#elif defined(Q_OS_MACOS)
    dirs << QDir(homeDirectory()).filePath(QStringLiteral("Library/Application Support/i2pd"));
    dirs << QDir(homeDirectory()).filePath(QStringLiteral(".i2pd"));
#else
    dirs << QDir(homeDirectory()).filePath(QStringLiteral(".i2pd"));
    dirs << QStringLiteral("/var/lib/i2pd");
    dirs << QStringLiteral("/etc/i2pd");
#endif
    dirs.removeDuplicates();
    return dirs;
}

QStringList tunnelFilesForDataDir(const QString &dataDir)
{
    QStringList paths;
    QSet<QString> seenDirs;
    collectTunnelFiles(dataDir, &paths, &seenDirs);
    paths.removeDuplicates();
    return paths;
}

bool writeTorrentsRpcAuth(const QString &path,
                          const QString &section,
                          bool auth,
                          const QString &user,
                          const QString &pass,
                          QString *error)
{
    const QList<TunnelKeyUpdate> updates = {
        {QStringLiteral("rpcauth"), auth ? QStringLiteral("true") : QStringLiteral("false")},
        {QStringLiteral("rpcuser"), user.trimmed()},
        {QStringLiteral("rpcpass"), pass.trimmed()},
    };
    return writeTunnelKeys(path, section, updates, false, error);
}

bool writeTorrentsSection(const QString &path,
                          const QString &section,
                          const TorrentsTunnelConfig &values,
                          bool createSection,
                          QString *error)
{
    QString rpcPath = values.rpcPath.trimmed();
    while (rpcPath.startsWith(QLatin1Char('/'))) {
        rpcPath = rpcPath.mid(1);
    }
    while (rpcPath.endsWith(QLatin1Char('/'))) {
        rpcPath.chop(1);
    }
    if (rpcPath.isEmpty()) {
        rpcPath = QStringLiteral("mytorrents");
    }
    QStringList trackers = values.trackers;
    if (trackers.isEmpty()) {
        if (const auto parsed = parseTorrentsTunnelFile(path)) {
            if (parsed->sectionName == section.trimmed()) {
                trackers = parsed->trackers;
            }
        }
    }
    const QList<TunnelKeyUpdate> updates = {
        {QStringLiteral("torrentsdir"), values.torrentsDir.trimmed()},
        {QStringLiteral("rpcport"), QString::number(values.rpcPort)},
        {QStringLiteral("rpcpath"), rpcPath},
        {QStringLiteral("rpcauth"), values.rpcAuth ? QStringLiteral("true") : QStringLiteral("false")},
        {QStringLiteral("rpcuser"), values.rpcUser.trimmed()},
        {QStringLiteral("rpcpass"), values.rpcPass.trimmed()},
        {QStringLiteral("trackers"), mergeI2pTrackers(trackers).join(QLatin1Char(','))},
    };
    return writeTunnelKeys(path, section, updates, createSection, error);
}

} // namespace i2p
