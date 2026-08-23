#pragma once

#include <QJsonObject>
#include <QString>
#include <utility>

namespace i2p {

inline constexpr int MIN_WINDOW_WIDTH = 780;
inline constexpr int MIN_WINDOW_HEIGHT = 520;
inline constexpr int DEFAULT_WINDOW_WIDTH = 1080;
inline constexpr int DEFAULT_WINDOW_HEIGHT = 700;

struct AppSettings {
    QString rpcUrl = QStringLiteral("http://127.0.0.1:9191/mytorrents");
    QString torrentsDir;
    quint32 refreshSeconds = 5;
    QString theme = QStringLiteral("night");
    QString language = QStringLiteral("en");
    QString torrentView = QStringLiteral("detailed");
    QString httpProxy = QStringLiteral("socks5://127.0.0.1:4447");
    quint32 windowWidth = DEFAULT_WINDOW_WIDTH;
    quint32 windowHeight = DEFAULT_WINDOW_HEIGHT;

    AppSettings();

    static AppSettings load();
    static AppSettings loadFrom(const QString &path);
    bool save() const;
    bool saveTo(const QString &path) const;
    std::pair<int, int> windowSize() const;
    void captureWindowSize(int width, int height);
    QString torrentsPath(QString *error = nullptr) const;
};

bool operator==(const AppSettings &lhs, const AppSettings &rhs);

QString configDirectory();
QString normalizeView(const QString &value);
QString migrateProxy(const QString &value);

} // namespace i2p
