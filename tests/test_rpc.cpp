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
    void getTorrentsAcceptsJsonRpc2ResultObject();
    void addTorrentSendsBase64();
    void setFilePrioritySendsTorrentSet();
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

int runRpcTests(int argc, char *argv[])
{
    RpcTests suite;
    return QTest::qExec(&suite, argc, argv);
}

#include "test_rpc.moc"
