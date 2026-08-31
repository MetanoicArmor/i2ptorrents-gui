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
    void peerIdentHashDisplayAndClipboard();
    void syncPeerCountsUsesPeersArrayWhenAggregatesZero();
    void decodePieceBitfieldMsbFirst();
    void finishedTorrentWithoutBitfieldIsComplete();
    void progressIsClamped();
    void percentDonePreferredOverLeftTotal();
    void parsesEtaAndTrackers();
    void parsesTrackerStats();
    void peerProgressLabel();
    void formatEtaHumanReadable();
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
    const QString fullIdentHash =
        QStringLiteral("HUK-AbCd1234~EfGh5678-IjKl9012~MnOp3456-Q=");
    const QJsonObject obj{
        {QStringLiteral("id"), 3},
        {QStringLiteral("peers"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("address"), QStringLiteral("HUK-")},
                 {QStringLiteral("identHash"), fullIdentHash},
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
    QCOMPARE(peers[0].displayAddress(), fullIdentHash);
    QCOMPARE(peers[0].clipboardText(), fullIdentHash);
    QCOMPARE(peers[0].tooltipAddress(), fullIdentHash);
    QCOMPARE(peers[0].clientName, QStringLiteral("i2pd"));
    QCOMPARE(peers[0].flagStr, QStringLiteral("IDU"));
    QVERIFY(peers[0].isIncoming);
    QVERIFY(peers[0].isDownloadingFrom);
    QVERIFY(!peers[0].isUploadingTo);
    QCOMPARE(peers[1].displayAddress(), QStringLiteral("wxyz"));
    QCOMPARE(peers[1].clipboardText(), QStringLiteral("wxyz"));
    QVERIFY(peers[1].isUploadingTo);
}

void ModelsTests::peerIdentHashDisplayAndClipboard()
{
    const QString fullIdentHash =
        QStringLiteral("HUK-AbCd1234~EfGh5678-IjKl9012~MnOp3456-Q=");
    i2p::Peer peer;
    peer.identHash = fullIdentHash;
    QCOMPARE(peer.displayAddress(), fullIdentHash);
    QCOMPARE(peer.clipboardText(), fullIdentHash);

    i2p::Peer withAddress;
    withAddress.address = QStringLiteral("HUK-");
    withAddress.identHash = fullIdentHash;
    QCOMPARE(withAddress.displayAddress(), fullIdentHash);
    QCOMPARE(withAddress.clipboardText(), fullIdentHash);
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

void ModelsTests::percentDonePreferredOverLeftTotal()
{
    const QJsonObject obj{
        {QStringLiteral("id"), 1},
        {QStringLiteral("totalSize"), 100},
        {QStringLiteral("leftUntilDone"), 90},
        {QStringLiteral("percentDone"), 0.42},
    };
    const std::optional<i2p::Torrent> torrent = i2p::torrentFromRpc(obj);
    QVERIFY(torrent.has_value());
    QVERIFY(qAbs(torrent->progress() - 0.42) < 1e-9);
}

void ModelsTests::parsesEtaAndTrackers()
{
    const QJsonObject obj{
        {QStringLiteral("id"), 9},
        {QStringLiteral("eta"), 3720},
        {QStringLiteral("trackers"),
         QJsonArray{
             QJsonObject{{QStringLiteral("id"), QStringLiteral("0")},
                         {QStringLiteral("announce"), QStringLiteral("http://tracker2.postman.i2p/announce.php")},
                         {QStringLiteral("tier"), 0}},
             QJsonObject{{QStringLiteral("id"), QStringLiteral("1")},
                         {QStringLiteral("announce"), QStringLiteral("http://tracker.i2p/a")},
                         {QStringLiteral("tier"), 1}},
         }},
    };
    const std::optional<i2p::Torrent> torrent = i2p::torrentFromRpc(obj);
    QVERIFY(torrent.has_value());
    QCOMPARE(torrent->eta, qint64(3720));
    QCOMPARE(torrent->trackers.size(), 2);
    QCOMPARE(torrent->trackers[0].announce, QStringLiteral("http://tracker2.postman.i2p/announce.php"));
    QCOMPARE(torrent->trackers[1].tier, 1);

    const QVector<i2p::Tracker> only =
        i2p::parseTorrentTrackers(QJsonObject{{QStringLiteral("trackers"), obj.value(QStringLiteral("trackers"))}});
    QCOMPARE(only.size(), 2);
}

void ModelsTests::parsesTrackerStats()
{
    const QJsonObject obj{
        {QStringLiteral("trackerStats"),
         QJsonArray{QJsonObject{{QStringLiteral("id"), 0},
                                {QStringLiteral("announce"), QStringLiteral("http://tracker2.postman.i2p/announce.php")},
                                {QStringLiteral("seederCount"), 12},
                                {QStringLiteral("leecherCount"), 3},
                                {QStringLiteral("lastAnnounceTime"), 1700000000}}}},
        {QStringLiteral("trackers"),
         QJsonArray{QJsonObject{{QStringLiteral("id"), QStringLiteral("0")},
                                {QStringLiteral("announce"), QStringLiteral("http://ignored.i2p/a")},
                                {QStringLiteral("tier"), 0}}}},
    };
    const QVector<i2p::Tracker> trackers = i2p::parseTorrentTrackers(obj);
    QCOMPARE(trackers.size(), 1);
    QCOMPARE(trackers[0].announce, QStringLiteral("http://tracker2.postman.i2p/announce.php"));
    QCOMPARE(trackers[0].seederCount, qint64(12));
    QCOMPARE(trackers[0].leecherCount, qint64(3));
    QCOMPARE(trackers[0].lastAnnounceTime, qint64(1700000000));
    QCOMPARE(trackers[0].seederLabel(), QStringLiteral("12"));
}

void ModelsTests::peerProgressLabel()
{
    const QJsonObject obj{
        {QStringLiteral("peers"),
         QJsonArray{QJsonObject{{QStringLiteral("address"), QStringLiteral("abcd")},
                                {QStringLiteral("progress"), 0.5}}}},
    };
    const QVector<i2p::Peer> peers = i2p::parseTorrentPeers(obj);
    QCOMPARE(peers.size(), 1);
    QVERIFY(peers[0].progress.has_value());
    QVERIFY(qAbs(*peers[0].progress - 0.5) < 1e-9);
    QCOMPARE(peers[0].progressLabel(), QStringLiteral("50%"));
}

void ModelsTests::formatEtaHumanReadable()
{
    i2p::setLanguage(QStringLiteral("en"));
    QCOMPARE(i2p::formatEta(-2), QString());
    QCOMPARE(i2p::formatEta(45), QStringLiteral("45s"));
    QCOMPARE(i2p::formatEta(125), QStringLiteral("2m"));
    QCOMPARE(i2p::formatEta(3600), QStringLiteral("1h"));
    QCOMPARE(i2p::formatEta(3720), QStringLiteral("1h 2m"));
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
