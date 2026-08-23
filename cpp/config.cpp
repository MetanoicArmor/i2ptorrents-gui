#include "config.hpp"

#include "i18n.hpp"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>

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

QString jsonString(const QJsonValue &value, const QString &fallback)
{
    if (value.isString()) {
        return value.toString();
    }
    if (!value.isUndefined() && !value.isNull()) {
        return value.toVariant().toString();
    }
    return fallback;
}

quint32 jsonUInt(const QJsonValue &value, quint32 fallback)
{
    if (value.isDouble()) {
        return static_cast<quint32>(value.toInt());
    }
    if (value.isString()) {
        bool ok = false;
        const int parsed = value.toString().toInt(&ok);
        if (ok) {
            return static_cast<quint32>(parsed);
        }
    }
    return fallback;
}

} // namespace

AppSettings::AppSettings()
{
    torrentsDir = QDir(homeDirectory()).filePath(QStringLiteral("torrents"));
}

QString configDirectory()
{
#if defined(Q_OS_WIN)
    const QString base =
        qEnvironmentVariableIsSet("APPDATA")
            ? QString::fromLocal8Bit(qgetenv("APPDATA"))
            : homeDirectory();
    return QDir(base).filePath(QStringLiteral("i2ptorrents-gui"));
#elif defined(Q_OS_MACOS)
    return QDir(homeDirectory()).filePath(QStringLiteral("Library/Application Support/i2ptorrents-gui"));
#else
    QString base = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (base.isEmpty()) {
        base = QDir(homeDirectory()).filePath(QStringLiteral(".config"));
    }
    return QDir(base).filePath(QStringLiteral("i2ptorrents-gui"));
#endif
}

QString normalizeView(const QString &value)
{
    const QString lowered = value.trimmed().toLower();
    if (lowered == QStringLiteral("simple") || lowered == QStringLiteral("compact") ||
        lowered == QStringLiteral("упрощённый") || lowered == QStringLiteral("упрощенный")) {
        return QStringLiteral("simple");
    }
    return QStringLiteral("detailed");
}

QString migrateProxy(const QString &value)
{
    const QString raw = value.trimmed();
    return raw.isEmpty() ? QStringLiteral("socks5://127.0.0.1:4447") : raw;
}

AppSettings AppSettings::loadFrom(const QString &path)
{
    AppSettings defaults;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return defaults;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return defaults;
    }
    const QJsonObject obj = doc.object();
    AppSettings settings;
    settings.rpcUrl = jsonString(obj.value(QStringLiteral("rpc_url")), defaults.rpcUrl);
    settings.torrentsDir =
        jsonString(obj.value(QStringLiteral("torrents_dir")), defaults.torrentsDir);
    settings.refreshSeconds = std::clamp(jsonUInt(obj.value(QStringLiteral("refresh_seconds")), 5),
                                         quint32(2),
                                         quint32(60));
    settings.theme = jsonString(obj.value(QStringLiteral("theme")), defaults.theme) ==
                             QStringLiteral("night")
                         ? QStringLiteral("night")
                         : QStringLiteral("light");
    settings.language = normalizeLanguage(jsonString(obj.value(QStringLiteral("language")), defaults.language));
    settings.torrentView =
        normalizeView(jsonString(obj.value(QStringLiteral("torrent_view")), defaults.torrentView));
    settings.httpProxy =
        migrateProxy(jsonString(obj.value(QStringLiteral("http_proxy")), defaults.httpProxy));
    settings.windowWidth =
        std::max(jsonUInt(obj.value(QStringLiteral("window_width")), DEFAULT_WINDOW_WIDTH),
                 quint32(MIN_WINDOW_WIDTH));
    settings.windowHeight =
        std::max(jsonUInt(obj.value(QStringLiteral("window_height")), DEFAULT_WINDOW_HEIGHT),
                 quint32(MIN_WINDOW_HEIGHT));
    return settings;
}

AppSettings AppSettings::load()
{
    return loadFrom(QDir(configDirectory()).filePath(QStringLiteral("settings.json")));
}

bool AppSettings::saveTo(const QString &path) const
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QJsonObject obj;
    obj.insert(QStringLiteral("rpc_url"), rpcUrl);
    obj.insert(QStringLiteral("torrents_dir"), torrentsDir);
    obj.insert(QStringLiteral("refresh_seconds"), static_cast<int>(refreshSeconds));
    obj.insert(QStringLiteral("theme"), theme);
    obj.insert(QStringLiteral("language"), language);
    obj.insert(QStringLiteral("torrent_view"), torrentView);
    obj.insert(QStringLiteral("http_proxy"), httpProxy);
    obj.insert(QStringLiteral("window_width"), static_cast<int>(windowWidth));
    obj.insert(QStringLiteral("window_height"), static_cast<int>(windowHeight));

    const QString temporary = path + QStringLiteral(".tmp");
    QFile file(temporary);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    file.close();
    if (QFile::exists(path) && !QFile::remove(path)) {
        return false;
    }
    return QFile::rename(temporary, path);
}

bool AppSettings::save() const
{
    return saveTo(QDir(configDirectory()).filePath(QStringLiteral("settings.json")));
}

std::pair<int, int> AppSettings::windowSize() const
{
    return {static_cast<int>(std::max(windowWidth, quint32(MIN_WINDOW_WIDTH))),
            static_cast<int>(std::max(windowHeight, quint32(MIN_WINDOW_HEIGHT)))};
}

void AppSettings::captureWindowSize(int width, int height)
{
    windowWidth = static_cast<quint32>(std::max(width, 0));
    windowHeight = static_cast<quint32>(std::max(height, 0));
    windowWidth = std::max(windowWidth, quint32(MIN_WINDOW_WIDTH));
    windowHeight = std::max(windowHeight, quint32(MIN_WINDOW_HEIGHT));
}

QString AppSettings::torrentsPath(QString *error) const
{
    QString path = expandUser(torrentsDir);
    if (path.isEmpty()) {
        path = QDir(homeDirectory()).filePath(QStringLiteral("torrents"));
    }
    if (!QDir().mkpath(path)) {
        if (error) {
            *error = QStringLiteral("mkdir failed");
        }
        return {};
    }
    return path;
}

bool operator==(const AppSettings &lhs, const AppSettings &rhs)
{
    return lhs.rpcUrl == rhs.rpcUrl && lhs.torrentsDir == rhs.torrentsDir
           && lhs.refreshSeconds == rhs.refreshSeconds && lhs.theme == rhs.theme
           && lhs.language == rhs.language && lhs.torrentView == rhs.torrentView
           && lhs.httpProxy == rhs.httpProxy && lhs.windowWidth == rhs.windowWidth
           && lhs.windowHeight == rhs.windowHeight;
}

} // namespace i2p
