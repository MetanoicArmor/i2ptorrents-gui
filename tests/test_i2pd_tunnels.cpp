#include "i2pd_tunnels.hpp"

#include <QDir>
#include <QSet>
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
    void parsesRpcAuthKeys();
    void writesRpcAuthIntoSection();
    void replacesExistingRpcAuthKeys();
    void rejectsUnsafeRpcCredentials();
    void reportsMissingSection();
    void readsTunconfFromI2pdConf();
    void explicitTunnelPathWins();
    void prefersWritableTunnelFile();
    void writesRpcEndpointAndDirectory();
    void defaultDataDirsFollowI2pd();
    void defaultI2pTrackersIncludeOpentrackers();
    void mergeI2pTrackersKeepsPreferredFirst();
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
    QVERIFY(!config->rpcAuth);
    QVERIFY(config->rpcUser.isEmpty());
    QVERIFY(config->rpcPass.isEmpty());
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

void I2pdTunnelsTests::parsesRpcAuthKeys()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/tunnels.conf");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"([MyTorrents]
type=torrents
rpcauth=yes
rpcuser=alice
rpcpass=s3cret
)");
    file.close();

    const auto config = i2p::parseTorrentsTunnelFile(path);
    QVERIFY(config.has_value());
    QVERIFY(config->rpcAuth);
    QCOMPARE(config->rpcUser, QStringLiteral("alice"));
    QCOMPARE(config->rpcPass, QStringLiteral("s3cret"));
}

void I2pdTunnelsTests::writesRpcAuthIntoSection()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/tunnels.conf");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"(# keep
[HTTP]
type=http
port=4444

[MyTorrents]
type=torrents
torrentsdir=/data
rpcuser=old
# comment stays

[Second]
type=torrents
rpcuser=other
rpcpass=keep
)");
    file.close();

    QString error;
    QVERIFY(i2p::writeTorrentsRpcAuth(path,
                                      QStringLiteral("MyTorrents"),
                                      true,
                                      QStringLiteral("alice"),
                                      QStringLiteral("secret"),
                                      &error));
    QVERIFY(error.isEmpty());

    QFile written(path);
    QVERIFY(written.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString text = QString::fromUtf8(written.readAll());
    QVERIFY(text.contains(QStringLiteral("# keep")));
    QVERIFY(text.contains(QStringLiteral("# comment stays")));
    QVERIFY(text.contains(QStringLiteral("rpcuser=alice")));
    QVERIFY(!text.contains(QStringLiteral("rpcuser=old")));
    QVERIFY(text.contains(QStringLiteral("rpcauth=true")));
    QVERIFY(text.contains(QStringLiteral("rpcpass=secret")));
    QVERIFY(text.contains(QStringLiteral("rpcuser=other")));
    QVERIFY(text.contains(QStringLiteral("rpcpass=keep")));
    QCOMPARE(text.count(QStringLiteral("rpcauth=")), 1);

    const auto config = i2p::parseTorrentsTunnelFile(path);
    QVERIFY(config.has_value());
    QCOMPARE(config->sectionName, QStringLiteral("MyTorrents"));
    QVERIFY(config->rpcAuth);
    QCOMPARE(config->rpcUser, QStringLiteral("alice"));
    QCOMPARE(config->rpcPass, QStringLiteral("secret"));
    QVERIFY(text.indexOf(QStringLiteral("rpcpass=secret")) < text.indexOf(QStringLiteral("[Second]")));
}

void I2pdTunnelsTests::replacesExistingRpcAuthKeys()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/tunnels.conf");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"([T]
type=torrents
rpcauth=false
rpcuser=old
rpcpass=old
)");
    file.close();

    QString error;
    QVERIFY(i2p::writeTorrentsRpcAuth(path,
                                      QStringLiteral("T"),
                                      false,
                                      QStringLiteral("new"),
                                      QStringLiteral("pass"),
                                      &error));
    QFile written(path);
    QVERIFY(written.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString text = QString::fromUtf8(written.readAll());
    QCOMPARE(text.count(QStringLiteral("rpcauth=")), 1);
    QCOMPARE(text.count(QStringLiteral("rpcuser=")), 1);
    QCOMPARE(text.count(QStringLiteral("rpcpass=")), 1);
    QVERIFY(text.contains(QStringLiteral("rpcauth=false")));
    QVERIFY(text.contains(QStringLiteral("rpcuser=new")));
    QVERIFY(text.contains(QStringLiteral("rpcpass=pass")));
}

void I2pdTunnelsTests::rejectsUnsafeRpcCredentials()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/tunnels.conf");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray original = R"([T]
type=torrents
rpcuser=old
)";
    file.write(original);
    file.close();

    QString error;
    QVERIFY(!i2p::writeTorrentsRpcAuth(path,
                                       QStringLiteral("T"),
                                       true,
                                       QStringLiteral("al#ice"),
                                       QStringLiteral("secret"),
                                       &error));
    QCOMPARE(error, QStringLiteral("invalid"));
    error.clear();
    QVERIFY(!i2p::writeTorrentsRpcAuth(path,
                                       QStringLiteral("T"),
                                       true,
                                       QStringLiteral("alice"),
                                       QStringLiteral("sec\nret"),
                                       &error));
    QCOMPARE(error, QStringLiteral("invalid"));

    QFile written(path);
    QVERIFY(written.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(written.readAll(), original);
}

void I2pdTunnelsTests::reportsMissingSection()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/tunnels.conf");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"([HTTP]
type=http
)");
    file.close();

    QString error;
    QVERIFY(!i2p::writeTorrentsRpcAuth(path,
                                       QStringLiteral("MyTorrents"),
                                       true,
                                       QStringLiteral("alice"),
                                       QStringLiteral("secret"),
                                       &error));
    QCOMPARE(error, QStringLiteral("missing"));
    QVERIFY(!i2p::writeTorrentsRpcAuth(dir.path() + QStringLiteral("/absent.conf"),
                                       QStringLiteral("MyTorrents"),
                                       false,
                                       QString(),
                                       QString(),
                                       &error));
    QCOMPARE(error, QStringLiteral("missing"));
}

void I2pdTunnelsTests::readsTunconfFromI2pdConf()
{
    QTemporaryDir dir;
    const QString root = dir.path();
    const QString custom = root + QStringLiteral("/custom.conf");
    const QString extraDir = root + QStringLiteral("/extra");
    QVERIFY(QDir().mkpath(extraDir));
    QFile conf(root + QStringLiteral("/i2pd.conf"));
    QVERIFY(conf.open(QIODevice::WriteOnly | QIODevice::Text));
    conf.write(QStringLiteral("tunconf=%1\ntunnelsdir=%2\n").arg(custom, extraDir).toUtf8());
    conf.close();
    QFile tunnel(custom);
    QVERIFY(tunnel.open(QIODevice::WriteOnly | QIODevice::Text));
    tunnel.write(R"([MyTorrents]
type=torrents
rpcport=9393
rpcpath=fromconf
)");
    tunnel.close();
    QFile extra(extraDir + QStringLiteral("/peer.conf"));
    QVERIFY(extra.open(QIODevice::WriteOnly | QIODevice::Text));
    extra.write(R"([Other]
type=client
)");
    extra.close();

    const QStringList files = i2p::tunnelFilesForDataDir(root);
    QVERIFY(files.contains(custom));
    QVERIFY(files.contains(extraDir + QStringLiteral("/peer.conf")));
    const auto config = i2p::detectTorrentsTunnel(root, QString());
    QVERIFY(config.has_value());
    QCOMPARE(config->rpcPort, quint16(9393));
    QCOMPARE(config->rpcPath, QStringLiteral("fromconf"));
}

void I2pdTunnelsTests::explicitTunnelPathWins()
{
    QTemporaryDir dir;
    const QString chosen = dir.path() + QStringLiteral("/chosen.conf");
    const QString other = dir.path() + QStringLiteral("/tunnels.conf");
    QFile chosenFile(chosen);
    QVERIFY(chosenFile.open(QIODevice::WriteOnly | QIODevice::Text));
    chosenFile.write(R"([Mine]
type=torrents
rpcport=9400
rpcpath=chosen
)");
    chosenFile.close();
    QFile otherFile(other);
    QVERIFY(otherFile.open(QIODevice::WriteOnly | QIODevice::Text));
    otherFile.write(R"([Other]
type=torrents
rpcport=9500
rpcpath=other
)");
    otherFile.close();

    const auto config = i2p::detectTorrentsTunnel(dir.path(), chosen);
    QVERIFY(config.has_value());
    QCOMPARE(config->sectionName, QStringLiteral("Mine"));
    QCOMPARE(config->rpcPort, quint16(9400));
}

void I2pdTunnelsTests::prefersWritableTunnelFile()
{
    QTemporaryDir dir;
    const QString root = dir.path();
    const QString locked = root + QStringLiteral("/locked.conf");
    QFile lockedFile(locked);
    QVERIFY(lockedFile.open(QIODevice::WriteOnly | QIODevice::Text));
    lockedFile.write(R"([Locked]
type=torrents
rpcport=9101
rpcpath=locked
)");
    lockedFile.close();
    QVERIFY(QFile::setPermissions(locked, QFileDevice::ReadOwner | QFileDevice::ReadUser));
    if (QFile probe(locked); probe.open(QIODevice::Append)) {
        QSKIP("read-only permissions are not enforced for this user (e.g. root)");
    }

    QFile openFile(root + QStringLiteral("/tunnels.conf"));
    QVERIFY(openFile.open(QIODevice::WriteOnly | QIODevice::Text));
    openFile.write(R"([Open]
type=torrents
rpcport=9102
rpcpath=open
)");
    openFile.close();

    QFile conf(root + QStringLiteral("/i2pd.conf"));
    QVERIFY(conf.open(QIODevice::WriteOnly | QIODevice::Text));
    conf.write(QStringLiteral("tunconf=%1\n").arg(locked).toUtf8());
    conf.close();

    const auto config = i2p::detectTorrentsTunnel(root, QString());
    QVERIFY(config.has_value());
    QCOMPARE(config->sectionName, QStringLiteral("Open"));
    QCOMPARE(config->rpcPort, quint16(9102));
    QFile::setPermissions(locked, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

void I2pdTunnelsTests::writesRpcEndpointAndDirectory()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/tunnels.conf");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"([HTTP]
type=http
port=4444
)");
    file.close();

    i2p::TorrentsTunnelConfig values;
    values.rpcPort = 9292;
    values.rpcPath = QStringLiteral("customrpc");
    values.torrentsDir = QStringLiteral("/data/torrents");
    values.rpcAuth = true;
    values.rpcUser = QStringLiteral("alice");
    values.rpcPass = QStringLiteral("secret");
    QString error;
    QVERIFY(i2p::writeTorrentsSection(path, QStringLiteral("MyTorrents"), values, true, &error));
    QVERIFY(error.isEmpty());

    const auto config = i2p::parseTorrentsTunnelFile(path);
    QVERIFY(config.has_value());
    QCOMPARE(config->sectionName, QStringLiteral("MyTorrents"));
    QCOMPARE(config->rpcPort, quint16(9292));
    QCOMPARE(config->rpcPath, QStringLiteral("customrpc"));
    QCOMPARE(config->torrentsDir, QStringLiteral("/data/torrents"));
    QVERIFY(config->rpcAuth);
    QCOMPARE(config->rpcUser, QStringLiteral("alice"));
    QFile written(path);
    QVERIFY(written.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString text = QString::fromUtf8(written.readAll());
    QVERIFY(text.contains(QStringLiteral("type=http")));
    QVERIFY(text.contains(QStringLiteral("[MyTorrents]")));
    QVERIFY(text.contains(QStringLiteral("http://opentracker.dg2.i2p/a")));
    QVERIFY(text.contains(QStringLiteral("http://tracker.nyaa2p.i2p/announce")));
    QVERIFY(config->trackers.contains(QStringLiteral("http://opentracker.r4sas.i2p/announce")));
}

void I2pdTunnelsTests::defaultDataDirsFollowI2pd()
{
    const QStringList dirs = i2p::defaultI2pdDataDirs();
    QVERIFY(!dirs.isEmpty());
#if defined(Q_OS_MACOS)
    QVERIFY(dirs.first().endsWith(QStringLiteral("Library/Application Support/i2pd")));
    QVERIFY(dirs.contains(QDir::homePath() + QStringLiteral("/.i2pd")));
#elif defined(Q_OS_WIN)
    QVERIFY(dirs.first().endsWith(QStringLiteral("i2pd")));
    for (const QString &dir : dirs) {
        QVERIFY(!dir.contains(QStringLiteral("Local\\i2pd")) && !dir.contains(QStringLiteral("Local/i2pd")));
    }
#else
    QVERIFY(dirs.contains(QDir::homePath() + QStringLiteral("/.i2pd")));
    QVERIFY(dirs.contains(QStringLiteral("/var/lib/i2pd")));
    QVERIFY(dirs.contains(QStringLiteral("/etc/i2pd")));
#endif
}

void I2pdTunnelsTests::defaultI2pTrackersIncludeOpentrackers()
{
    const QStringList trackers = i2p::defaultI2pTrackers();
    QCOMPARE(trackers.first(), QStringLiteral("http://tracker2.postman.i2p/announce.php"));
    QVERIFY(trackers.contains(QStringLiteral("http://opentracker.dg2.i2p/a")));
    QVERIFY(trackers.contains(QStringLiteral("http://opentracker.r4sas.i2p/announce")));
    QVERIFY(trackers.contains(QStringLiteral("http://opentracker.simp.i2p/a")));
    QVERIFY(trackers.contains(QStringLiteral("http://tracker.eeptorrent.i2p/a")));
    QVERIFY(trackers.contains(QStringLiteral("http://opentracker.fattydove.i2p/a")));
    QVERIFY(trackers.contains(QStringLiteral("http://opentracker-actix.i2p/announce")));
    QVERIFY(trackers.contains(QStringLiteral("http://opentracker.bt.i2p/a")));
    QVERIFY(trackers.contains(QStringLiteral("http://vern.i2p:6969/announce")));
    QVERIFY(trackers.contains(QStringLiteral("http://atia.i2p/announce")));
    QVERIFY(trackers.contains(QStringLiteral("http://sigmatracker.i2p/a")));
    QVERIFY(trackers.contains(QStringLiteral("http://opentracker.skank.i2p/a")));
    QVERIFY(trackers.contains(QStringLiteral("http://omitracker.i2p/announce")));
    QVERIFY(trackers.contains(QStringLiteral("http://psa.i2p/a")));
    QVERIFY(trackers.contains(QStringLiteral("http://tracker.nyaa2p.i2p/announce")));
    QCOMPARE(trackers.size(), 15);
    QCOMPARE(QSet<QString>(trackers.begin(), trackers.end()).size(), 15);
    QCOMPARE(i2p::defaultI2pTrackersConf().count(QLatin1Char(',')), 14);
}

void I2pdTunnelsTests::mergeI2pTrackersKeepsPreferredFirst()
{
    const QString custom = QStringLiteral("http://custom.i2p/announce");
    const QStringList merged = i2p::mergeI2pTrackers({custom, QStringLiteral("http://tracker2.postman.i2p/announce.php")});
    QCOMPARE(merged.first(), custom);
    QCOMPARE(merged.at(1), QStringLiteral("http://tracker2.postman.i2p/announce.php"));
    QVERIFY(merged.contains(QStringLiteral("http://opentracker.skank.i2p/a")));
    QCOMPARE(merged.size(), i2p::defaultI2pTrackers().size() + 1);
}

int runI2pdTunnelsTests(int argc, char *argv[])
{
    I2pdTunnelsTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "test_i2pd_tunnels.moc"
