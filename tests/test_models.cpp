#include "models.hpp"
#include "i18n.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

class ModelsTests final : public QObject {
    Q_OBJECT

private slots:
    void torrentParsesSnakeCaseFields();
    void torrentParsesFilesWantedPriorities();
    void torrentParsesPeers();
    void syncPeerCountsUsesPeersArrayWhenAggregatesZero();
    void decodePieceBitfieldMsbFirst();
    void finishedTorrentWithoutBitfieldIsComplete();
    void progressIsClamped();
    void humanReadableUnits();
};

void ModelsTests::torrentParsesSnakeCaseFields()
{
    const QJsonObject obj{
        {QStringLiteral("id"), 7},
        {QStringLiteral("name"), QStringLiteral("Linux ISO")},
        {QStringLiteral("status"), 6},
        {QStringLiteral("is_finished"), true},
        {QStringLiteral("total_size"), 1024},
        {QStringLiteral("left_until_done"), 0},
        {QStringLiteral("rate_upload"), 512},
        {QStringLiteral("hash_string"), QStringLiteral("abc")},
        {QStringLiteral("piece_count"), 8},
        {QStringLiteral("piece_size"), 256},
        {QStringLiteral("pieces"), QString::fromLatin1(QByteArray(1, char(0x81)).toBase64())},
    };
    const std::optional<i2p::Torrent> torrent = i2p::torrentFromRpc(obj);
    QVERIFY(torrent.has_value());
    QCOMPARE(torrent->status, i2p::TorrentStatus::Seeding);
    QVERIFY(torrent->finished);
    QCOMPARE(torrent->progress(), 1.0);
    QCOMPARE(torrent->hashString, QStringLiteral("abc"));
    QVERIFY(torrent->pieces.first());
    QVERIFY(torrent->pieces.last());
}

void ModelsTests::torrentParsesFilesWantedPriorities()
{
    const QJsonObject obj{
        {QStringLiteral("id"), 2},
        {QStringLiteral("name"), QStringLiteral("Album")},
        {QStringLiteral("files"),
         QJsonArray{
             QJsonObject{{QStringLiteral("name"), QStringLiteral("disk/a.flac")},
                         {QStringLiteral("length"), 1000},
                         {QStringLiteral("bytes_completed"), 250}},
             QJsonObject{{QStringLiteral("name"), QStringLiteral("/abs/b.flac")},
                         {QStringLiteral("length"), 4000},
                         {QStringLiteral("bytesCompleted"), 0}},
         }},
        {QStringLiteral("wanted"), QJsonArray{1, 0}},
        {QStringLiteral("priorities"), QJsonArray{1, -1}},
    };
    const std::optional<i2p::Torrent> torrent = i2p::torrentFromRpc(obj);
    QVERIFY(torrent.has_value());
    QCOMPARE(torrent->files.size(), 2);
    QCOMPARE(torrent->files[0].displayName(), QStringLiteral("disk/a.flac"));
    QCOMPARE(torrent->files[0].kind(), i2p::FilePriority::High);
    QCOMPARE(torrent->files[0].progressLabel(), QStringLiteral("25%"));
    QCOMPARE(torrent->files[1].displayName(), QStringLiteral("b.flac"));
    QCOMPARE(torrent->files[1].kind(), i2p::FilePriority::Skip);
}

void ModelsTests::torrentParsesPeers()
{
    const QJsonObject obj{
        {QStringLiteral("id"), 3},
        {QStringLiteral("peers"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("address"), QStringLiteral("abcd")},
                 {QStringLiteral("identHash"), QStringLiteral("abcdEFGH1234")},
                 {QStringLiteral("clientName"), QStringLiteral("i2pd")},
                 {QStringLiteral("rateToClient"), 1024},
                 {QStringLiteral("rateToPeer"), 512},
                 {QStringLiteral("flagStr"), QStringLiteral("IDU")},
                 {QStringLiteral("isIncoming"), true},
                 {QStringLiteral("isDowloadingFrom"), true},
                 {QStringLiteral("isUploading_to"), false},
             },
             QJsonObject{
                 {QStringLiteral("address"), QStringLiteral("wxyz")},
                 {QStringLiteral("client_name"), QStringLiteral("I2PSnark")},
                 {QStringLiteral("rate_to_client"), 0},
                 {QStringLiteral("rate_to_peer"), 2048},
                 {QStringLiteral("flag_str"), QStringLiteral("U")},
                 {QStringLiteral("is_uploading_to"), true},
             },
         }},
    };
    const QVector<i2p::Peer> peers = i2p::parseTorrentPeers(obj);
    QCOMPARE(peers.size(), 2);
    QCOMPARE(peers[0].displayAddress(), QStringLiteral("abcd"));
    QCOMPARE(peers[0].tooltipAddress(), QStringLiteral("abcdEFGH1234"));
    QCOMPARE(peers[0].clientName, QStringLiteral("i2pd"));
    QCOMPARE(peers[0].flagStr, QStringLiteral("IDU"));
    QVERIFY(peers[0].isIncoming);
    QVERIFY(peers[0].isDownloadingFrom);
    QVERIFY(!peers[0].isUploadingTo);
    QCOMPARE(peers[1].displayAddress(), QStringLiteral("wxyz"));
    QCOMPARE(peers[1].tooltipAddress(), QStringLiteral("wxyz"));
    QVERIFY(peers[1].isUploadingTo);
}

void ModelsTests::syncPeerCountsUsesPeersArrayWhenAggregatesZero()
{
    QJsonObject obj{
        {QStringLiteral("id"), 5},
        {QStringLiteral("peersSendingToUs"), 0},
        {QStringLiteral("peersGettingFromUs"), 0},
        {QStringLiteral("peers"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("address"), QStringLiteral("abcd")},
                 {QStringLiteral("isDowloadingFrom"), true},
             },
             QJsonObject{
                 {QStringLiteral("address"), QStringLiteral("wxyz")},
                 {QStringLiteral("isUploading_to"), true},
             },
         }},
    };
    const std::optional<i2p::Torrent> torrent = i2p::torrentFromRpc(obj);
    QVERIFY(torrent.has_value());
    QCOMPARE(torrent->peersSendingToUs, quint64(1));
    QCOMPARE(torrent->peersGettingFromUs, quint64(1));
}

void ModelsTests::decodePieceBitfieldMsbFirst()
{
    const QJsonValue raw = QString::fromLatin1(QByteArray(1, char(0x81)).toBase64());
    const QVector<bool> bits = i2p::decodePieceBitfield(raw, 8, false);
    QCOMPARE(bits,
             (QVector<bool>{true, false, false, false, false, false, false, true}));
}

void ModelsTests::finishedTorrentWithoutBitfieldIsComplete()
{
    const QJsonValue raw = QStringLiteral("");
    const QVector<bool> bits = i2p::decodePieceBitfield(raw, 4, true);
    QCOMPARE(bits, (QVector<bool>{true, true, true, true}));
}

void ModelsTests::progressIsClamped()
{
    const QJsonObject obj{
        {QStringLiteral("id"), 1},
        {QStringLiteral("totalSize"), 10},
        {QStringLiteral("leftUntilDone"), 20},
    };
    const std::optional<i2p::Torrent> torrent = i2p::torrentFromRpc(obj);
    QVERIFY(torrent.has_value());
    QCOMPARE(torrent->progress(), 0.0);
}

void ModelsTests::humanReadableUnits()
{
    i2p::setLanguage(QStringLiteral("en"));
    QCOMPARE(i2p::formatBytes(1024), QStringLiteral("1.0 KB"));
    QCOMPARE(i2p::formatRate(1024), QStringLiteral("1.0 KB/s"));
    i2p::setLanguage(QStringLiteral("ru"));
    QCOMPARE(i2p::formatBytes(1024), QStringLiteral("1.0 КБ"));
    QCOMPARE(i2p::formatRate(1024), QStringLiteral("1.0 КБ/с"));
    i2p::setLanguage(QStringLiteral("en"));
}

int runModelsTests(int argc, char *argv[])
{
    ModelsTests suite;
    return QTest::qExec(&suite, argc, argv);
}

#include "test_models.moc"
