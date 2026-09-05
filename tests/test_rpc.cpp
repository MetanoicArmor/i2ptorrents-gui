#include "models.hpp"
#include "rpc.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

class RpcTests final : public QObject {
    Q_OBJECT

private slots:
    void normalizeRpcUrlCompletesPath();
    void remoteRpcIsRejected();
    void getTorrentsAcceptsSnakeCase();
    void getTorrentsOmitsPiecesInSimpleView();
    void getTorrentFilesRequestsOneTorrent();
    void getTorrentPeersRequestsOneTorrent();
    void getTorrentPeersParsesNestedJsonRpc2();
    void getTorrentPeersParsesI2pdScientificNotation();
    void getTorrentTrackersRequestsOneTorrent();
    void getTorrentsRequestsPercentDoneAndEta();
    void getTorrentsAcceptsJsonRpc2ResultObject();
    void addTorrentSendsBase64();
    void addTorrentMagnetSendsFilename();
    void normalizeMagnetLinkAcceptsHexAndBase32();
    void setFilePrioritySendsTorrentSet();
    void startStopTorrentSendsIds();
};

void RpcTests::normalizeRpcUrlCompletesPath()
{
    struct Case {
        const char *input;
        const char *expected;
    };
    const Case cases[] = {
        {"127.0.0.1:9191/mytorrents", "http://127.0.0.1:9191/mytorrents/rpc/"},
        {"http://localhost:9191/mytorrents/", "http://localhost:9191/mytorrents/rpc/"},
        {"http://localhost:9191/mytorrents/rpc", "http://localhost:9191/mytorrents/rpc/"},
        {"http://localhost:9191", "http://localhost:9191/rpc/"},
    };
    for (const Case &item : cases) {
        const std::optional<QString> url = i2p::normalizeRpcUrl(QString::fromUtf8(item.input));
        QVERIFY(url.has_value());
        QCOMPARE(*url, QString::fromUtf8(item.expected));
    }
}

void RpcTests::remoteRpcIsRejected()
{
    QString error;
    const std::optional<QString> url =
        i2p::normalizeRpcUrl(QStringLiteral("http://192.0.2.10:9191/mytorrents"), &error);
    QVERIFY(!url.has_value());
    QVERIFY(error.contains(QStringLiteral("local")) || error.contains(QStringLiteral("локальн")));
}

void RpcTests::getTorrentsAcceptsSnakeCase()
{
    i2p::RpcClient client(
        QStringLiteral("http://localhost:9191/mytorrents"),
        [](const QString &, const QByteArray &, QString *) {
            return QJsonDocument(QJsonObject{
                                      {QStringLiteral("result"), QStringLiteral("success")},
                                      {QStringLiteral("arguments"),
                                       QJsonObject{{QStringLiteral("torrents"),
                                                    QJsonArray{QJsonObject{
                                                        {QStringLiteral("id"), 4},
                                                        {QStringLiteral("name"), QStringLiteral("Example")},
                                                        {QStringLiteral("status"), 4},
                                                        {QStringLiteral("total_size"), 100},
                                                        {QStringLiteral("left_until_done"), 25},
                                                        {QStringLiteral("rate_download"), 10},
                                                    }}}}}})
                .toJson();
        });
    QString error;
    const QVector<i2p::Torrent> torrents = client.getTorrents(true, &error);
    QCOMPARE(error, QString());
    QCOMPARE(torrents.size(), 1);
    QCOMPARE(torrents[0].name, QStringLiteral("Example"));
    QVERIFY(qAbs(torrents[0].progress() - 0.75) < 1e-9);
}

void RpcTests::getTorrentsOmitsPiecesInSimpleView()
{
    QString method;
    QJsonObject arguments;
    i2p::RpcClient client(
        QStringLiteral("http://localhost:9191/mytorrents"),
        [&](const QString &, const QByteArray &body, QString *) {
            const QJsonObject payload = QJsonDocument::fromJson(body).object();
            method = payload.value(QStringLiteral("method")).toString();
            arguments = payload.value(QStringLiteral("arguments")).toObject();
            return QJsonDocument(QJsonObject{{QStringLiteral("result"), QStringLiteral("success")},
                                               {QStringLiteral("arguments"),
                                                QJsonObject{{QStringLiteral("torrents"), QJsonArray{}}}}})
                .toJson();
        });
    QString error;
    client.getTorrents(false, &error);
    QCOMPARE(method, QStringLiteral("torrent-get"));
    const QJsonArray fields = arguments.value(QStringLiteral("fields")).toArray();
    QStringList names;
    for (const QJsonValue &field : fields) {
        names << field.toString();
    }
    QVERIFY(!names.contains(QStringLiteral("pieces")));
    QVERIFY(names.contains(QStringLiteral("hashString")));
    QVERIFY(names.contains(QStringLiteral("peers")));
    QVERIFY(names.contains(QStringLiteral("percentDone")));
    QVERIFY(names.contains(QStringLiteral("eta")));
    QVERIFY(!names.contains(QStringLiteral("files")));
}

void RpcTests::getTorrentFilesRequestsOneTorrent()
{
    QString capturedMethod;
    int capturedId = 0;
    i2p::RpcClient client(
        QStringLiteral("http://localhost:9191/mytorrents"),
        [&](const QString &, const QByteArray &body, QString *) {
            const QJsonObject payload = QJsonDocument::fromJson(body).object();
            const QJsonObject arguments = payload.value(QStringLiteral("arguments")).toObject();
            capturedMethod = payload.value(QStringLiteral("method")).toString();
            capturedId = arguments.value(QStringLiteral("ids")).toArray().first().toInt();
            return QJsonDocument(QJsonObject{
                                      {QStringLiteral("result"), QStringLiteral("success")},
                                      {QStringLiteral("arguments"),
                                       QJsonObject{{QStringLiteral("torrents"),
                                                    QJsonArray{QJsonObject{
                                                        {QStringLiteral("id"), 7},
                                                        {QStringLiteral("files"),
                                                         QJsonArray{QJsonObject{
                                                             {QStringLiteral("name"), QStringLiteral("a.bin")},
                                                             {QStringLiteral("length"), 10},
                                                             {QStringLiteral("bytesCompleted"), 3},
                                                         }}},
                                                        {QStringLiteral("wanted"), QJsonArray{1}},
                                                        {QStringLiteral("priorities"), QJsonArray{0}},
                                                    }}}}}})
                .toJson();
        });
    QString error;
    const QVector<i2p::TorrentFile> files = client.getTorrentFiles(7, &error);
    QCOMPARE(capturedMethod, QStringLiteral("torrent-get"));
    QCOMPARE(capturedId, 7);
    QCOMPARE(files.size(), 1);
    QCOMPARE(files[0].displayName(), QStringLiteral("a.bin"));
}

void RpcTests::getTorrentPeersRequestsOneTorrent()
{
    QString capturedMethod;
    int capturedId = 0;
    QStringList capturedFields;
    i2p::RpcClient client(
        QStringLiteral("http://localhost:9191/mytorrents"),
        [&](const QString &, const QByteArray &body, QString *) {
            const QJsonObject payload = QJsonDocument::fromJson(body).object();
            const QJsonObject arguments = payload.value(QStringLiteral("arguments")).toObject();
            capturedMethod = payload.value(QStringLiteral("method")).toString();
            capturedId = arguments.value(QStringLiteral("ids")).toArray().first().toInt();
            for (const QJsonValue &field : arguments.value(QStringLiteral("fields")).toArray()) {
                capturedFields << field.toString();
            }
            return QJsonDocument(QJsonObject{
                                      {QStringLiteral("result"), QStringLiteral("success")},
                                      {QStringLiteral("arguments"),
                                       QJsonObject{{QStringLiteral("torrents"),
                                                    QJsonArray{QJsonObject{
                                                        {QStringLiteral("id"), 7},
                                                        {QStringLiteral("peers"),
                                                         QJsonArray{QJsonObject{
                                                             {QStringLiteral("address"), QStringLiteral("abcd")},
                                                             {QStringLiteral("clientName"), QStringLiteral("i2pd")},
                                                             {QStringLiteral("rateToClient"), 100},
                                                             {QStringLiteral("rateToPeer"), 50},
                                                             {QStringLiteral("flagStr"), QStringLiteral("ID")},
                                                         }}},
                                                    }}}}}})
                .toJson();
        });
    QString error;
    const QVector<i2p::Peer> peers = client.getTorrentPeers(7, &error);
    QCOMPARE(capturedMethod, QStringLiteral("torrent-get"));
    QCOMPARE(capturedId, 7);
    QVERIFY(capturedFields.contains(QStringLiteral("peers")));
    QCOMPARE(peers.size(), 1);
    QCOMPARE(peers[0].displayAddress(), QStringLiteral("abcd"));
    QCOMPARE(peers[0].clientName, QStringLiteral("i2pd"));
    QCOMPARE(peers[0].flagStr, QStringLiteral("ID"));
}

void RpcTests::getTorrentPeersParsesNestedJsonRpc2()
{
    i2p::RpcClient client(
        QStringLiteral("http://localhost:9191/mytorrents"),
        [](const QString &, const QByteArray &, QString *) {
            return QJsonDocument(QJsonObject{
                                      {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                                      {QStringLiteral("id"), 2},
                                      {QStringLiteral("result"),
                                       QJsonObject{
                                           {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                                           {QStringLiteral("torrents"),
                                            QJsonArray{QJsonObject{
                                                {QStringLiteral("id"), 5},
                                                {QStringLiteral("peersSendingToUs"), 1},
                                                {QStringLiteral("peers"),
                                                 QJsonArray{
                                                     QJsonObject{
                                                         {QStringLiteral("address"), QStringLiteral("HUK-")},
                                                         {QStringLiteral("clientName"), QStringLiteral("I2PSnark")},
                                                         {QStringLiteral("isDowloadingFrom"), true},
                                                         {QStringLiteral("rateToClient"), 12676},
                                                         {QStringLiteral("rateToPeer"), 0},
                                                         {QStringLiteral("flagStr"), QString()},
                                                     },
                                                     QJsonObject{
                                                         {QStringLiteral("address"), QStringLiteral("~zXQ")},
                                                         {QStringLiteral("clientName"), QStringLiteral("I2PSnark")},
                                                         {QStringLiteral("isUploading_to"), true},
                                                         {QStringLiteral("rateToClient"), 0},
                                                         {QStringLiteral("rateToPeer"), 0},
                                                         {QStringLiteral("flagStr"), QString()},
                                                     },
                                                 }},
                                            }}}},
                                       }})
                .toJson();
        });
    QString error;
    const QVector<i2p::Peer> peers = client.getTorrentPeers(5, &error);
    QVERIFY(error.isEmpty());
    QCOMPARE(peers.size(), 2);
    QCOMPARE(peers[0].displayAddress(), QStringLiteral("HUK-"));
    QCOMPARE(peers[1].displayAddress(), QStringLiteral("~zXQ"));
}

void RpcTests::getTorrentPeersParsesI2pdScientificNotation()
{
    const QByteArray raw =
        R"({"jsonrpc":"2.0","result":{"jsonrpc":"2.0","torrents":[{"id":5,"peers":[{"address":"HUK-","clientName":"I2PSnark","progress":5E-1},{"address":"~zXQ","clientName":"I2PSnark","progress":5E-1}]}]},"id":2})";
    i2p::RpcClient client(QStringLiteral("http://localhost:9191/mytorrents"),
                          [raw](const QString &, const QByteArray &, QString *) { return raw; });
    QString error;
    const QVector<i2p::Peer> peers = client.getTorrentPeers(5, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(peers.size(), 2);
    QVERIFY(peers[0].progress.has_value());
    QVERIFY(qAbs(*peers[0].progress - 0.5) < 1e-9);
    QCOMPARE(peers[0].progressLabel(), QStringLiteral("50%"));
}

void RpcTests::getTorrentTrackersRequestsOneTorrent()
{
    QString capturedMethod;
    int capturedId = 0;
    QStringList capturedFields;
    i2p::RpcClient client(
        QStringLiteral("http://localhost:9191/mytorrents"),
        [&](const QString &, const QByteArray &body, QString *) {
            const QJsonObject payload = QJsonDocument::fromJson(body).object();
            const QJsonObject arguments = payload.value(QStringLiteral("arguments")).toObject();
            capturedMethod = payload.value(QStringLiteral("method")).toString();
            capturedId = arguments.value(QStringLiteral("ids")).toArray().first().toInt();
            for (const QJsonValue &field : arguments.value(QStringLiteral("fields")).toArray()) {
                capturedFields << field.toString();
            }
            return QJsonDocument(QJsonObject{
                                      {QStringLiteral("result"), QStringLiteral("success")},
                                      {QStringLiteral("arguments"),
                                       QJsonObject{{QStringLiteral("torrents"),
                                                    QJsonArray{QJsonObject{
                                                        {QStringLiteral("id"), 3},
                                                        {QStringLiteral("trackers"),
                                                         QJsonArray{QJsonObject{
                                                             {QStringLiteral("id"), QStringLiteral("0")},
                                                             {QStringLiteral("announce"),
                                                              QStringLiteral("http://tracker2.postman.i2p/announce.php")},
                                                             {QStringLiteral("tier"), 0},
                                                         }}},
                                                    }}}}}})
                .toJson();
        });
    QString error;
    const QVector<i2p::Tracker> trackers = client.getTorrentTrackers(3, &error);
    QCOMPARE(capturedMethod, QStringLiteral("torrent-get"));
    QCOMPARE(capturedId, 3);
    QVERIFY(capturedFields.contains(QStringLiteral("trackers")));
    QVERIFY(capturedFields.contains(QStringLiteral("trackerStats")));
    QCOMPARE(trackers.size(), 1);
    QCOMPARE(trackers[0].announce, QStringLiteral("http://tracker2.postman.i2p/announce.php"));
}

void RpcTests::getTorrentsRequestsPercentDoneAndEta()
{
    i2p::RpcClient client(
        QStringLiteral("http://localhost:9191/mytorrents"),
        [](const QString &, const QByteArray &, QString *) {
            return QJsonDocument(QJsonObject{
                                      {QStringLiteral("result"), QStringLiteral("success")},
                                      {QStringLiteral("arguments"),
                                       QJsonObject{{QStringLiteral("torrents"),
                                                    QJsonArray{QJsonObject{
                                                        {QStringLiteral("id"), 8},
                                                        {QStringLiteral("name"), QStringLiteral("EtaTorrent")},
                                                        {QStringLiteral("status"), 4},
                                                        {QStringLiteral("totalSize"), 1000},
                                                        {QStringLiteral("leftUntilDone"), 900},
                                                        {QStringLiteral("percentDone"), 0.33},
                                                        {QStringLiteral("eta"), 120},
                                                    }}}}}})
                .toJson();
        });
    QString error;
    const QVector<i2p::Torrent> torrents = client.getTorrents(true, &error);
    QCOMPARE(torrents.size(), 1);
    QVERIFY(qAbs(torrents[0].progress() - 0.33) < 1e-9);
    QCOMPARE(torrents[0].eta, qint64(120));
}

void RpcTests::getTorrentsAcceptsJsonRpc2ResultObject()
{
    i2p::RpcClient client(
        QStringLiteral("http://localhost:9191/mytorrents"),
        [](const QString &, const QByteArray &, QString *) {
            return QJsonDocument(QJsonObject{
                                      {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                                      {QStringLiteral("id"), 1},
                                      {QStringLiteral("result"),
                                       QJsonObject{{QStringLiteral("torrents"),
                                                    QJsonArray{QJsonObject{
                                                        {QStringLiteral("id"), 2},
                                                        {QStringLiteral("name"), QStringLiteral("JsonRpc")},
                                                        {QStringLiteral("status"), 6},
                                                        {QStringLiteral("totalSize"), 50},
                                                        {QStringLiteral("leftUntilDone"), 0},
                                                    }}}}}})
                .toJson();
        });
    QString error;
    const QVector<i2p::Torrent> torrents = client.getTorrents(false, &error);
    QCOMPARE(torrents.size(), 1);
    QCOMPARE(torrents[0].name, QStringLiteral("JsonRpc"));
    QVERIFY(torrents[0].finished || torrents[0].progress() >= 1.0);
}

void RpcTests::addTorrentSendsBase64()
{
    QString capturedMethod;
    QString capturedMetainfo;
    i2p::RpcClient client(
        QStringLiteral("http://localhost:9191"),
        [&](const QString &, const QByteArray &body, QString *) {
            const QJsonObject payload = QJsonDocument::fromJson(body).object();
            capturedMethod = payload.value(QStringLiteral("method")).toString();
            capturedMetainfo =
                payload.value(QStringLiteral("arguments")).toObject().value(QStringLiteral("metainfo")).toString();
            return QJsonDocument(QJsonObject{
                                      {QStringLiteral("result"), QStringLiteral("success")},
                                      {QStringLiteral("arguments"),
                                       QJsonObject{{QStringLiteral("torrent-added"),
                                                    QJsonObject{{QStringLiteral("id"), 1}}}}}})
                .toJson();
        });
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/test.torrent");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("d4:infode");
    file.close();
    QString error;
    const QJsonObject added = client.addTorrentPath(path, &error);
    QCOMPARE(capturedMethod, QStringLiteral("torrent-add"));
    QCOMPARE(QByteArray::fromBase64(capturedMetainfo.toUtf8()), QByteArray("d4:infode"));
    QCOMPARE(added.value(QStringLiteral("id")).toInt(), 1);
}

void RpcTests::addTorrentMagnetSendsFilename()
{
    QString method;
    QJsonObject arguments;
    i2p::RpcClient client(
        QStringLiteral("http://localhost:9191"),
        [&](const QString &, const QByteArray &body, QString *) {
            const QJsonObject payload = QJsonDocument::fromJson(body).object();
            method = payload.value(QStringLiteral("method")).toString();
            arguments = payload.value(QStringLiteral("arguments")).toObject();
            return QJsonDocument(QJsonObject{
                                      {QStringLiteral("result"), QStringLiteral("success")},
                                      {QStringLiteral("arguments"),
                                       QJsonObject{{QStringLiteral("torrent-added"),
                                                    QJsonObject{{QStringLiteral("id"), 7}}}}}})
                .toJson();
        });
    QString error;
    const QJsonObject added = client.addTorrentMagnet(
        QStringLiteral("magnet:?xt=urn:btih:0123456789abcdef0123456789ABCDEF01234567"), &error);
    QCOMPARE(method, QStringLiteral("torrent-add"));
    QCOMPARE(arguments.value(QStringLiteral("filename")).toString(),
             QStringLiteral("magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567"));
    QVERIFY(!arguments.contains(QStringLiteral("metainfo")));
    QCOMPARE(added.value(QStringLiteral("id")).toInt(), 7);
}

void RpcTests::normalizeMagnetLinkAcceptsHexAndBase32()
{
    QString error;
    const std::optional<QString> fromHex = i2p::normalizeMagnetLink(
        QStringLiteral("  MAGNET:?XT=urn:btih:0123456789ABCDEF0123456789abcdef01234567&dn=Demo  "),
        &error);
    QVERIFY(fromHex.has_value());
    QCOMPARE(*fromHex, QStringLiteral("magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567"));

    const std::optional<QString> fromRaw =
        i2p::normalizeMagnetLink(QStringLiteral("0123456789ABCDEF0123456789abcdef01234567"), &error);
    QVERIFY(fromRaw.has_value());
    QCOMPARE(*fromRaw, QStringLiteral("magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567"));

    const QByteArray bytes = QByteArray::fromHex("0123456789abcdef0123456789abcdef01234567");
    QString base32;
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    quint64 acc = 0;
    int bits = 0;
    for (unsigned char byte : bytes) {
        acc = (acc << 8) | byte;
        bits += 8;
        while (bits >= 5) {
            bits -= 5;
            base32.append(QLatin1Char(alphabet[(acc >> bits) & 31]));
        }
    }
    if (bits > 0) {
        base32.append(QLatin1Char(alphabet[(acc << (5 - bits)) & 31]));
    }
    QCOMPARE(base32.size(), 32);
    const std::optional<QString> fromBase32 = i2p::normalizeMagnetLink(base32, &error);
    QVERIFY(fromBase32.has_value());
    QCOMPARE(*fromBase32, QStringLiteral("magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567"));

    QVERIFY(!i2p::normalizeMagnetLink(QStringLiteral("not-a-magnet"), &error).has_value());
    QVERIFY(!error.isEmpty());
}

void RpcTests::setFilePrioritySendsTorrentSet()
{
    QString method;
    QJsonObject arguments;
    i2p::RpcClient client(
        QStringLiteral("http://localhost:9191"),
        [&](const QString &, const QByteArray &body, QString *) {
            const QJsonObject payload = QJsonDocument::fromJson(body).object();
            method = payload.value(QStringLiteral("method")).toString();
            arguments = payload.value(QStringLiteral("arguments")).toObject();
            return QJsonDocument(QJsonObject{{QStringLiteral("result"), QStringLiteral("success")},
                                               {QStringLiteral("arguments"), QJsonObject{}}})
                .toJson();
        });
    QString error;
    QVERIFY(client.setFilePriority(3, 1, true, 1, &error));
    QCOMPARE(method, QStringLiteral("torrent-set"));
    QCOMPARE(arguments.value(QStringLiteral("ids")).toArray().first().toInt(), 3);
    QCOMPARE(arguments.value(QStringLiteral("files-wanted")).toArray().first().toInt(), 1);
    QCOMPARE(arguments.value(QStringLiteral("priority-high")).toArray().first().toInt(), 1);

    QVERIFY(client.setFilePriority(3, 2, false, 0, &error));
    QCOMPARE(method, QStringLiteral("torrent-set"));
    QCOMPARE(arguments.value(QStringLiteral("files-unwanted")).toArray().first().toInt(), 2);
}

void RpcTests::startStopTorrentSendsIds()
{
    QString method;
    QJsonObject arguments;
    i2p::RpcClient client(
        QStringLiteral("http://localhost:9191"),
        [&](const QString &, const QByteArray &body, QString *) {
            const QJsonObject payload = QJsonDocument::fromJson(body).object();
            method = payload.value(QStringLiteral("method")).toString();
            arguments = payload.value(QStringLiteral("arguments")).toObject();
            return QJsonDocument(QJsonObject{{QStringLiteral("result"), QStringLiteral("success")},
                                               {QStringLiteral("arguments"), QJsonObject{}}})
                .toJson();
        });
    QString error;
    QVERIFY(client.startTorrent(4, &error));
    QCOMPARE(method, QStringLiteral("torrent-start"));
    QCOMPARE(arguments.value(QStringLiteral("ids")).toArray().first().toInt(), 4);

    QVERIFY(client.stopTorrent(4, &error));
    QCOMPARE(method, QStringLiteral("torrent-stop"));
    QCOMPARE(arguments.value(QStringLiteral("ids")).toArray().first().toInt(), 4);
}

int runRpcTests(int argc, char *argv[])
{
    RpcTests suite;
    return QTest::qExec(&suite, argc, argv);
}

#include "test_rpc.moc"
