#include "postman_catalog.hpp"

#include <QTest>

class PostmanCatalogTests final : public QObject {
    Q_OBJECT

private slots:
    void readsEnterToken();
    void parsesCatalogRows();
};

namespace {

QString htmlFixture(const char *text)
{
    QString html = QString::fromUtf8(text);
    html.replace(QLatin1Char('\''), QLatin1Char('"'));
    return html;
}

const char *kGateHtml = R"(
<title>PaTracker 1.7.5 | Site Under Heavy Load</title>
<form action='/?action=Enter' method='POST'>
<input type='hidden' name='formtoken' value='396a286253ee692ecbed01f58f11ff7a6c4d1700'>
<input type='submit' value='Proceed'>
</form>
)";

const char *kCatalogHtml = R"(
<title>Torrents [Page 1] - Postman BitTorrent Tracker</title>
<table>
<tr>
<th>Category</th><th>Torrent</th><th>Size</th><th>Seed/Leech</th><th>Added</th><th>Download</th>
</tr>
<tr>
<td class='category'><a href='index.php?view=Main&category=1' title='Movies'><span class='cat Movies'>Movies</span></a></td>
<td class='torrentname'>
<a href='index.php?view=TorrentDetail&id=104486' title='Tom &amp; Jerry'>Tom &amp; Jerry</a>
<span>Classic cartoon shorts. <a href='https://www.imdb.com/title/tt0000001'>IMDb</a></span>
</td>
<td class='filesize'><span class='size'>1.1G <span class='filecount'>1</span></span></td>
<td class='swarmsize'><span class='swarm'>2 / 0<span class='badge'> 0</span></span></td>
<td class='added'>2026-10-02</td>
<td class='download'>
<a href='magnet:?xt=urn:btih:d8d00c98e3a08929e9a0276ae43453cb2a391b4b&amp;dn=Tom+%26+Jerry&amp;tr=http://tracker2.postman.i2p/announce.php'></a>
<a href='index.php?action=Download&amp;id=104486'></a>
</td>
</tr>
<tr>
<td class='category'><a href='index.php?view=Main&category=2' title='Music'><span class='cat Music'>Music</span></a></td>
<td class='torrentname'><a href='index.php?view=TorrentDetail&id=7' title='Demo'>Demo</a></td>
<td class='filesize'><span class='size'>4M <span class='filecount'>2</span></span></td>
<td class='swarmsize'><span class='swarm'>1 / 3</span></td>
<td class='added'>2026-09-01</td>
<td class='download'>
<a href='magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567&amp;tr=http://tracker2.postman.i2p/announce.php'></a>
<a href='index.php?action=Download&amp;id=7'></a>
</td>
</tr>
</table>
<a href='index.php?view=Main&start=40&limit=40'>2</a>
)";

} // namespace

void PostmanCatalogTests::readsEnterToken()
{
    const QString html = htmlFixture(kGateHtml);
    QVERIFY(i2p::postmanNeedsEnter(html));
    QCOMPARE(i2p::postmanEnterToken(html), QStringLiteral("396a286253ee692ecbed01f58f11ff7a6c4d1700"));
    QVERIFY(!i2p::postmanNeedsEnter(htmlFixture(kCatalogHtml)));
    QVERIFY(i2p::postmanEnterToken(htmlFixture(kCatalogHtml)).isEmpty());
}

void PostmanCatalogTests::parsesCatalogRows()
{
    i2p::PostmanQuery query;
    query.limit = 40;
    const i2p::PostmanPage page = i2p::parsePostmanPage(htmlFixture(kCatalogHtml), query);
    QCOMPARE(page.rows.size(), 2);
    QCOMPARE(page.nextStart, 40);
    QCOMPARE(page.prevStart, -1);

    const i2p::PostmanTorrent &first = page.rows.at(0);
    QCOMPARE(first.name, QStringLiteral("Tom & Jerry"));
    QCOMPARE(first.category, QStringLiteral("Movies"));
    QCOMPARE(first.sizeText, QStringLiteral("1.1G"));
    QCOMPARE(first.seeders, 2);
    QCOMPARE(first.leechers, 0);
    QCOMPARE(first.added, QStringLiteral("2026-10-02"));
    QCOMPARE(first.id, qint64(104486));
    QCOMPARE(first.magnet,
             QStringLiteral("magnet:?xt=urn:btih:d8d00c98e3a08929e9a0276ae43453cb2a391b4b&dn=Tom+%26+Jerry&tr=http://tracker2.postman.i2p/announce.php"));
    QCOMPARE(first.summary, QStringLiteral("Classic cartoon shorts. IMDb"));

    QCOMPARE(page.rows.at(1).id, qint64(7));
    QCOMPARE(page.rows.at(1).seeders, 1);
    QCOMPARE(page.rows.at(1).leechers, 3);
    QCOMPARE(page.rows.at(1).category, QStringLiteral("Music"));
    QVERIFY(page.rows.at(1).summary.isEmpty());
}

int runPostmanCatalogTests(int argc, char *argv[])
{
    PostmanCatalogTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "test_postman_catalog.moc"
