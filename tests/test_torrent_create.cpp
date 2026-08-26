#include "bencode.hpp"
#include "torrent_create.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class TorrentCreateTests final : public QObject {
    Q_OBJECT

private slots:
    void bencodesStringAndInt();
    void defaultPieceSizeIsPowerOfTwo();
    void createsSingleFileTorrent();
    void createsMultiFileTorrent();
    void cancelStopsHashing();
};

void TorrentCreateTests::bencodesStringAndInt()
{
    QCOMPARE(i2p::bencodeString(QByteArray("spam")), QByteArray("4:spam"));
    QCOMPARE(i2p::bencodeInt(42), QByteArray("i42e"));
    QVariantHash dict;
    dict.insert(QStringLiteral("b"), 2);
    dict.insert(QStringLiteral("a"), 1);
    QCOMPARE(i2p::bencode(dict), QByteArray("d1:ai1e1:bi2ee"));
}

void TorrentCreateTests::defaultPieceSizeIsPowerOfTwo()
{
    const quint32 size = i2p::defaultPieceSize(100ull * 1024ull * 1024ull);
    QVERIFY(i2p::isLegalPieceSize(size));
    QVERIFY(size >= 16u * 1024u);
}

void TorrentCreateTests::createsSingleFileTorrent()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/payload.bin");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray payload(50 * 1024, 'x');
    QCOMPARE(file.write(payload), qint64(payload.size()));
    file.close();

    i2p::CreateTorrentRequest req;
    req.sourcePath = path;
    req.trackers = {QStringLiteral("http://tracker2.postman.i2p/announce.php")};
    req.pieceSize = 16u * 1024u;
    req.createdBy = QStringLiteral("test");

    QString error;
    const auto meta = i2p::createTorrentMetainfo(req, &error);
    QVERIFY2(meta.has_value(), qPrintable(error));
    QVERIFY(meta->contains("4:info"));
    QVERIFY(meta->contains("8:announce"));
    const quint64 pieces = (static_cast<quint64>(payload.size()) + req.pieceSize - 1) / req.pieceSize;
    const QByteArray needle =
        QByteArray("6:pieces") + QByteArray::number(static_cast<qint64>(pieces * 20)) + ':';
    QVERIFY2(meta->contains(needle), qPrintable(QString::fromLatin1(needle)));
}

void TorrentCreateTests::createsMultiFileTorrent()
{
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("bundle/a")));
    QFile f1(dir.path() + QStringLiteral("/bundle/a/one.txt"));
    QVERIFY(f1.open(QIODevice::WriteOnly));
    f1.write("hello");
    f1.close();
    QFile f2(dir.path() + QStringLiteral("/bundle/two.txt"));
    QVERIFY(f2.open(QIODevice::WriteOnly));
    f2.write("world!");
    f2.close();

    i2p::CreateTorrentRequest req;
    req.sourcePath = dir.path() + QStringLiteral("/bundle");
    req.trackers = {QStringLiteral("http://t.example/announce")};
    req.pieceSize = 16u * 1024u;

    QString error;
    const auto meta = i2p::createTorrentMetainfo(req, &error);
    QVERIFY2(meta.has_value(), qPrintable(error));
    QVERIFY(meta->contains("5:files"));
    QVERIFY(meta->contains("7:one.txt"));
}

void TorrentCreateTests::cancelStopsHashing()
{
    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/big.bin");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(QByteArray(512 * 1024, 'z')), qint64(512 * 1024));
    file.close();

    i2p::CreateTorrentRequest req;
    req.sourcePath = path;
    req.pieceSize = 16u * 1024u;
    std::atomic_bool cancel{true};
    QString error;
    const auto meta = i2p::createTorrentMetainfo(req, &error, &cancel);
    QVERIFY(!meta.has_value());
    QCOMPARE(error, QStringLiteral("Cancelled"));
}

int runTorrentCreateTests(int argc, char *argv[])
{
    TorrentCreateTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "test_torrent_create.moc"
