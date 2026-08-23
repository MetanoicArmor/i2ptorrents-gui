#include "config.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class SettingsTests final : public QObject {
    Q_OBJECT

private slots:
    void settingsRoundtrip();
    void settingsRecoverFromInvalidJson();
    void settingsKeepSocksProxy();
    void torrentViewRoundtrip();
    void torrentViewNormalizesAliases();
    void torrentsPathCreatesDirectory();
    void windowSizeRoundtrip();
    void windowSizeDefaultsWhenMissing();
    void windowSizeClampsMinimum();
};

void SettingsTests::settingsRoundtrip()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/settings.json");
    i2p::AppSettings expected;
    expected.rpcUrl = QStringLiteral("http://127.0.0.1:9999/test");
    expected.torrentsDir = QStringLiteral("/tmp/torrents");
    expected.refreshSeconds = 9;
    expected.theme = QStringLiteral("night");
    expected.language = QStringLiteral("en");
    expected.torrentView = QStringLiteral("simple");
    expected.httpProxy = QStringLiteral("socks5://127.0.0.1:4447");
    expected.windowWidth = 1080;
    expected.windowHeight = 700;
    QVERIFY(expected.saveTo(path));
    QCOMPARE(i2p::AppSettings::loadFrom(path), expected);
}

void SettingsTests::settingsRecoverFromInvalidJson()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/settings.json");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{");
    file.close();
    QVERIFY(i2p::AppSettings::loadFrom(path).rpcUrl.startsWith(QStringLiteral("http://127.0.0.1")));
}

void SettingsTests::settingsKeepSocksProxy()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/settings.json");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"http_proxy": "socks5://127.0.0.1:4447"})");
    file.close();
    QCOMPARE(i2p::AppSettings::loadFrom(path).httpProxy, QStringLiteral("socks5://127.0.0.1:4447"));
}

void SettingsTests::torrentViewRoundtrip()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/settings.json");
    i2p::AppSettings settings;
    settings.torrentView = QStringLiteral("simple");
    QVERIFY(settings.saveTo(path));
    QCOMPARE(i2p::AppSettings::loadFrom(path).torrentView, QStringLiteral("simple"));
}

void SettingsTests::torrentViewNormalizesAliases()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/settings.json");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"torrent_view": "упрощённый"})");
    file.close();
    QCOMPARE(i2p::AppSettings::loadFrom(path).torrentView, QStringLiteral("simple"));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(R"({"torrent_view": "compact"})");
    file.close();
    QCOMPARE(i2p::AppSettings::loadFrom(path).torrentView, QStringLiteral("simple"));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(R"({"torrent_view": "full"})");
    file.close();
    QCOMPARE(i2p::AppSettings::loadFrom(path).torrentView, QStringLiteral("detailed"));
}

void SettingsTests::torrentsPathCreatesDirectory()
{
    QTemporaryDir dir;
    const QString folder = dir.path() + QStringLiteral("/downloads/torrents");
    i2p::AppSettings settings;
    settings.torrentsDir = folder;
    QString error;
    QCOMPARE(settings.torrentsPath(&error), folder);
    QVERIFY(QDir(folder).exists());
}

void SettingsTests::windowSizeRoundtrip()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/settings.json");
    i2p::AppSettings settings;
    settings.windowWidth = 1440;
    settings.windowHeight = 900;
    QVERIFY(settings.saveTo(path));
    const i2p::AppSettings loaded = i2p::AppSettings::loadFrom(path);
    QCOMPARE(loaded.windowWidth, quint32(1440));
    QCOMPARE(loaded.windowHeight, quint32(900));
}

void SettingsTests::windowSizeDefaultsWhenMissing()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/settings.json");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"language": "ru"})");
    file.close();
    const i2p::AppSettings loaded = i2p::AppSettings::loadFrom(path);
    QCOMPARE(loaded.windowWidth, quint32(i2p::DEFAULT_WINDOW_WIDTH));
    QCOMPARE(loaded.windowHeight, quint32(i2p::DEFAULT_WINDOW_HEIGHT));
}

void SettingsTests::windowSizeClampsMinimum()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/settings.json");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"window_width": 100, "window_height": 50})");
    file.close();
    const i2p::AppSettings loaded = i2p::AppSettings::loadFrom(path);
    QCOMPARE(loaded.windowWidth, quint32(i2p::MIN_WINDOW_WIDTH));
    QCOMPARE(loaded.windowHeight, quint32(i2p::MIN_WINDOW_HEIGHT));
}

int runSettingsTests(int argc, char *argv[])
{
    SettingsTests suite;
    return QTest::qExec(&suite, argc, argv);
}

#include "test_settings.moc"
