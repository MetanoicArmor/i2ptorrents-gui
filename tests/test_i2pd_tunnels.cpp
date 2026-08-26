#include "i2pd_tunnels.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class I2pdTunnelsTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesTorrentsSection();
    void usesDefaultsWhenRpcKeysMissing();
    void expandsTildeInTorrentsDir();
    void ignoresNonTorrentsSections();
    void returnsFirstTorrentsSection();
    void buildsRpcUrl();
};

void I2pdTunnelsTests::parsesTorrentsSection()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/tunnels.conf");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"([MyTorrents]
type=torrents
trackers=http://tracker2.postman.i2p/announce.php
torrentsdir=/home/i2pd/torrents
rpcport=9292
rpcpath=customrpc
)");
    file.close();

    const auto config = i2p::parseTorrentsTunnelFile(path);
    QVERIFY(config.has_value());
    QCOMPARE(config->sectionName, QStringLiteral("MyTorrents"));
    QCOMPARE(config->torrentsDir, QStringLiteral("/home/i2pd/torrents"));
    QCOMPARE(config->rpcPort, quint16(9292));
    QCOMPARE(config->rpcPath, QStringLiteral("customrpc"));
    QCOMPARE(config->rpcUrl(), QStringLiteral("http://127.0.0.1:9292/customrpc"));
    QCOMPARE(config->trackers,
             QStringList{QStringLiteral("http://tracker2.postman.i2p/announce.php")});
    QVERIFY(config->confPath.endsWith(QStringLiteral("tunnels.conf")));
}

void I2pdTunnelsTests::usesDefaultsWhenRpcKeysMissing()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/tunnels.conf");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"([T]
type=torrents
torrentsdir=/data/torrents
)");
    file.close();

    const auto config = i2p::parseTorrentsTunnelFile(path);
    QVERIFY(config.has_value());
    QCOMPARE(config->rpcPort, quint16(9191));
    QCOMPARE(config->rpcPath, QStringLiteral("mytorrents"));
    QCOMPARE(config->rpcUrl(), QStringLiteral("http://127.0.0.1:9191/mytorrents"));
}

void I2pdTunnelsTests::expandsTildeInTorrentsDir()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/tunnels.conf");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"([T]
type=torrents
torrentsdir=~/Downloads/torrents
)");
    file.close();

    const auto config = i2p::parseTorrentsTunnelFile(path);
    QVERIFY(config.has_value());
    QVERIFY(config->torrentsDir.endsWith(QStringLiteral("Downloads/torrents")) ||
            config->torrentsDir.endsWith(QStringLiteral("Downloads\\torrents")));
    QVERIFY(!config->torrentsDir.startsWith(QLatin1Char('~')));
}

void I2pdTunnelsTests::ignoresNonTorrentsSections()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/tunnels.conf");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"([HTTP]
type=http
host=127.0.0.1
port=4444

[IRC]
type=client
port=6668
)");
    file.close();
    QVERIFY(!i2p::parseTorrentsTunnelFile(path).has_value());
}

void I2pdTunnelsTests::returnsFirstTorrentsSection()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/tunnels.conf");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"([First]
type=torrents
torrentsdir=/first
rpcport=9191

[Second]
type=torrents
torrentsdir=/second
rpcport=9292
)");
    file.close();

    const auto config = i2p::parseTorrentsTunnelFile(path);
    QVERIFY(config.has_value());
    QCOMPARE(config->sectionName, QStringLiteral("First"));
    QCOMPARE(config->torrentsDir, QStringLiteral("/first"));
}

void I2pdTunnelsTests::buildsRpcUrl()
{
    i2p::TorrentsTunnelConfig config;
    config.rpcPort = 9191;
    config.rpcPath = QStringLiteral("/mytorrents/");
    QCOMPARE(config.rpcUrl(), QStringLiteral("http://127.0.0.1:9191/mytorrents"));
}

int runI2pdTunnelsTests(int argc, char *argv[])
{
    I2pdTunnelsTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "test_i2pd_tunnels.moc"
