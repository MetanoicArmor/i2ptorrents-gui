#include "postman_catalog.hpp"

#include "i18n.hpp"

#include <QEventLoop>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QThread>
#include <QTimer>
#include <QUrlQuery>
#include <algorithm>

namespace i2p {

namespace {

constexpr int POSTMAN_TIMEOUT_MS = 60000;

class PostmanCookieJar final : public QNetworkCookieJar {
public:
    void replace(const QList<QNetworkCookie> &cookies) { setAllCookies(cookies); }
    QList<QNetworkCookie> snapshot() const { return allCookies(); }
};

QString decodeHtml(QString text)
{
    for (int pass = 0; pass < 2; ++pass) {
        text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
        text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
        text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
        text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
        text.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
        text.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
        text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
        text.replace(QStringLiteral("&hellip;"), QStringLiteral("…"));
    }
    const QRegularExpression numeric(QStringLiteral("&#(x[0-9A-Fa-f]+|[0-9]+);"));
    for (int guard = 0; guard < 32; ++guard) {
        const QRegularExpressionMatch match = numeric.match(text);
        if (!match.hasMatch()) {
            break;
        }
        const QString raw = match.captured(1);
        bool ok = false;
        const uint code = raw.startsWith(QLatin1Char('x'), Qt::CaseInsensitive) ? raw.mid(1).toUInt(&ok, 16)
                                                                                 : raw.toUInt(&ok, 10);
        text.replace(match.capturedStart(), match.capturedLength(), ok ? QString(QChar(code)) : QString());
    }
    return text;
}

QString plainText(QString html)
{
    html.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    return decodeHtml(html).simplified();
}

QString tableCell(const QString &row, const QString &className)
{
    const QRegularExpression re(
        QStringLiteral("<td\\b[^>]*\\bclass\\s*=\\s*\"[^\"]*\\b%1\\b[^\"]*\"[^>]*>(.*?)</td>")
            .arg(QRegularExpression::escape(className)),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch match = re.match(row);
    return match.hasMatch() ? match.captured(1) : QString();
}

QString attrValue(const QString &html, const QString &tagPattern)
{
    const QRegularExpression re(tagPattern,
                                QRegularExpression::CaseInsensitiveOption |
                                    QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch match = re.match(html);
    return match.hasMatch() ? decodeHtml(match.captured(1)) : QString();
}

QUrl originUrl()
{
    return QUrl(QString::fromLatin1(POSTMAN_ORIGIN));
}

QUrl catalogUrl(const PostmanQuery &query)
{
    QUrl url = originUrl();
    QUrlQuery params;
    params.addQueryItem(QStringLiteral("view"), QStringLiteral("Main"));
    params.addQueryItem(QStringLiteral("search"), query.search);
    params.addQueryItem(QStringLiteral("category"), QString::number(query.category));
    params.addQueryItem(QStringLiteral("orderby"), QString::number(query.orderBy));
    params.addQueryItem(QStringLiteral("start"), QString::number(std::max(query.start, 0)));
    params.addQueryItem(QStringLiteral("limit"), QString::number(std::clamp(query.limit, 1, 100)));
    url.setQuery(params);
    return url;
}

struct HttpBody {
    QByteArray bytes;
    bool ok = false;
    bool throttled = false;
};

struct ProxyEndpoint {
    QNetworkProxy::ProxyType type = QNetworkProxy::HttpProxy;
    int port = POSTMAN_PROXY_PORT;
};

ProxyEndpoint proxyEndpoint(const PostmanProxy &proxy)
{
    ProxyEndpoint endpoint;
    endpoint.type = proxy.socks ? QNetworkProxy::Socks5Proxy : QNetworkProxy::HttpProxy;
    endpoint.port = proxy.port >= 1 && proxy.port <= 65535 ? proxy.port : POSTMAN_PROXY_PORT;
    return endpoint;
}

QString proxyKind(const ProxyEndpoint &proxy)
{
    return proxy.type == QNetworkProxy::Socks5Proxy ? QStringLiteral("SOCKS5") : QStringLiteral("HTTP");
}

QString proxyFailure(const ProxyEndpoint &proxy, const QString &reason)
{
    return trArgs(QStringLiteral("postman_proxy_error"),
                  {{QStringLiteral("kind"), proxyKind(proxy)},
                   {QStringLiteral("port"), QString::number(proxy.port)},
                   {QStringLiteral("error"), reason}});
}

QString throttleMessage(const ProxyEndpoint &proxy)
{
    return trArgs(QStringLiteral("postman_throttled"),
                  {{QStringLiteral("kind"), proxyKind(proxy)},
                   {QStringLiteral("port"), QString::number(proxy.port)}});
}

bool looksThrottled(const QNetworkReply *reply, const QByteArray &body)
{
    const QString phrase = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
    const QString blob = phrase + QLatin1Char('\n') + QString::fromUtf8(body.left(512)) + QLatin1Char('\n') +
                         reply->errorString();
    return blob.contains(QLatin1String("throttl"), Qt::CaseInsensitive);
}

HttpBody exchange(QNetworkAccessManager &manager,
                  const QByteArray &method,
                  const QUrl &url,
                  const QByteArray &body,
                  const ProxyEndpoint &proxy,
                  QString *error)
{
    QNetworkRequest request{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, false);
    request.setTransferTimeout(POSTMAN_TIMEOUT_MS);
    request.setRawHeader("User-Agent", "i2ptorrents-gui");
    request.setRawHeader("Accept", "text/html,application/xhtml+xml,*/*");
    request.setRawHeader("Connection", "close");
    if (!body.isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          QStringLiteral("application/x-www-form-urlencoded"));
    }

    QNetworkReply *reply = manager.sendCustomRequest(request, method, body);
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(POSTMAN_TIMEOUT_MS);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start();
    loop.exec();

    HttpBody result;
    if (!reply->isFinished()) {
        reply->abort();
        if (error) {
            *error = proxyFailure(proxy, trKey(QStringLiteral("postman_timeout")));
        }
        reply->deleteLater();
        return result;
    }
    result.bytes = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        result.throttled = looksThrottled(reply, result.bytes);
        if (error) {
            if (result.throttled) {
                *error = throttleMessage(proxy);
            } else {
                const QString reason = reply->errorString().trimmed().isEmpty()
                                           ? trKey(QStringLiteral("postman_timeout"))
                                           : reply->errorString().trimmed();
                *error = proxyFailure(proxy, reason);
            }
        }
        reply->deleteLater();
        return result;
    }
    result.ok = true;
    reply->deleteLater();
    return result;
}

HttpBody request(QNetworkAccessManager &manager,
                 const QByteArray &method,
                 const QUrl &url,
                 const QByteArray &body,
                 const ProxyEndpoint &proxy,
                 QString *error)
{
    HttpBody result = exchange(manager, method, url, body, proxy, error);
    if (result.ok || !result.throttled) {
        return result;
    }
    // i2pd and Java I2P answer 503 throttling! when a stream cannot be opened yet.
    QThread::msleep(2000);
    if (error) {
        error->clear();
    }
    return exchange(manager, method, url, body, proxy, error);
}

class Session {
public:
    Session(QList<QNetworkCookie> *cookies, const ProxyEndpoint &proxy)
        : cookies_(cookies)
        , proxy_(proxy)
    {
        jar_ = new PostmanCookieJar;
        jar_->replace(*cookies_);
        manager_.setCookieJar(jar_);
        manager_.setProxy(QNetworkProxy(proxy_.type, QString::fromLatin1(POSTMAN_PROXY_HOST), proxy_.port));
    }

    ~Session()
    {
        if (jar_ != nullptr && cookies_ != nullptr) {
            *cookies_ = jar_->snapshot();
        }
    }

    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    QNetworkAccessManager &manager() { return manager_; }
    const ProxyEndpoint &proxy() const { return proxy_; }

private:
    QList<QNetworkCookie> *cookies_ = nullptr;
    ProxyEndpoint proxy_;
    PostmanCookieJar *jar_ = nullptr;
    QNetworkAccessManager manager_;
};

bool submitEnter(QNetworkAccessManager &manager,
                 const QString &html,
                 const ProxyEndpoint &proxy,
                 QString *error)
{
    if (!postmanNeedsEnter(html)) {
        return true;
    }
    const QString token = postmanEnterToken(html);
    if (token.isEmpty()) {
        if (error) {
            *error = trKey(QStringLiteral("postman_heavy_load"));
        }
        return false;
    }
    QUrl enter = originUrl();
    enter.setQuery(QStringLiteral("action=Enter"));
    const QByteArray form = QByteArrayLiteral("formtoken=") + QUrl::toPercentEncoding(token);
    const HttpBody posted = request(manager, QByteArrayLiteral("POST"), enter, form, proxy, error);
    if (!posted.ok) {
        return false;
    }
    if (postmanNeedsEnter(QString::fromUtf8(posted.bytes))) {
        if (error) {
            *error = trKey(QStringLiteral("postman_heavy_load"));
        }
        return false;
    }
    if (error) {
        error->clear();
    }
    return true;
}

} // namespace

const QVector<PostmanCategory> &postmanCategories()
{
    static const QVector<PostmanCategory> rows = {
        {-1, "postman_all_categories", nullptr},
        {1, nullptr, "Movies"},
        {2, nullptr, "Music"},
        {3, nullptr, "TV"},
        {4, nullptr, "Games"},
        {5, nullptr, "Apps"},
        {6, nullptr, "Misc."},
        {8, nullptr, "Pictures"},
        {9, nullptr, "Anime"},
        {10, nullptr, "Comics"},
        {25, nullptr, "Social Media"},
        {24, nullptr, "Podcasts"},
        {11, nullptr, "Books"},
        {17, nullptr, "Audio Books"},
        {20, nullptr, "E-Books"},
        {21, nullptr, "Course/Lesson"},
        {22, nullptr, "Essay/Op-Ed"},
        {23, nullptr, "Cad/3D Printing"},
        {13, nullptr, "Music Vid."},
        {14, nullptr, "Pr0n"},
        {15, nullptr, "Documentary"},
        {16, nullptr, "Leaked Documents"},
        {18, nullptr, "Conspiracy"},
        {19, nullptr, "Religious Content"},
    };
    return rows;
}

const QVector<PostmanSort> &postmanSorts()
{
    static const QVector<PostmanSort> rows = {
        {-1, "postman_sort_default"},
        {1, "postman_sort_added"},
        {2, "postman_sort_downloads"},
        {3, "postman_sort_hits"},
        {4, "postman_sort_comments"},
        {5, "postman_sort_swarm"},
        {6, "postman_sort_rating"},
        {7, "postman_sort_size"},
    };
    return rows;
}

bool postmanNeedsEnter(const QString &html)
{
    return html.contains(QStringLiteral("formtoken"), Qt::CaseInsensitive) &&
           html.contains(QStringLiteral("action=Enter"), Qt::CaseInsensitive);
}

QString postmanEnterToken(const QString &html)
{
    const QRegularExpression namedFirst(
        QStringLiteral("name\\s*=\\s*[\"']formtoken[\"'][^>]*value\\s*=\\s*[\"']([0-9a-fA-F]+)[\"']"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch first = namedFirst.match(html);
    if (first.hasMatch()) {
        return first.captured(1);
    }
    const QRegularExpression valueFirst(
        QStringLiteral("value\\s*=\\s*[\"']([0-9a-fA-F]+)[\"'][^>]*name\\s*=\\s*[\"']formtoken[\"']"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch second = valueFirst.match(html);
    return second.hasMatch() ? second.captured(1) : QString();
}

PostmanPage parsePostmanPage(const QString &html, const PostmanQuery &query)
{
    PostmanPage page;
    page.start = std::max(query.start, 0);
    page.limit = std::max(query.limit, 1);

    const QRegularExpression rowRe(QStringLiteral("<tr\\b[^>]*>(.*?)</tr>"),
                                   QRegularExpression::CaseInsensitiveOption |
                                       QRegularExpression::DotMatchesEverythingOption);
    auto rows = rowRe.globalMatch(html);
    while (rows.hasNext()) {
        const QString row = rows.next().captured(1);
        const QString nameCell = tableCell(row, QStringLiteral("torrentname"));
        if (nameCell.isEmpty()) {
            continue;
        }
        PostmanTorrent item;
        item.name = attrValue(nameCell, QStringLiteral("<a\\b[^>]*\\btitle\\s*=\\s*\"([^\"]*)\""));
        if (item.name.isEmpty()) {
            item.name = plainText(nameCell);
        }
        QString blurbs;
        const QRegularExpression spanRe(QStringLiteral("<span\\b[^>]*>(.*?)</span>"),
                                         QRegularExpression::CaseInsensitiveOption |
                                             QRegularExpression::DotMatchesEverythingOption);
        auto spans = spanRe.globalMatch(nameCell);
        while (spans.hasNext()) {
            blurbs += spans.next().captured(1);
            blurbs += QLatin1Char(' ');
        }
        item.summary = plainText(blurbs);
        if (item.summary == item.name) {
            item.summary.clear();
        }
        constexpr int kSummaryLimit = 500;
        if (item.summary.size() > kSummaryLimit) {
            item.summary.truncate(kSummaryLimit);
            item.summary += QStringLiteral("…");
        }
        const QString categoryCell = tableCell(row, QStringLiteral("category"));
        item.category = attrValue(categoryCell, QStringLiteral("\\btitle\\s*=\\s*\"([^\"]*)\""));
        if (item.category.isEmpty()) {
            item.category = plainText(categoryCell);
        }
        QString sizeCell = tableCell(row, QStringLiteral("filesize"));
        sizeCell.remove(QRegularExpression(QStringLiteral("<span\\b[^>]*\\bfilecount\\b[^>]*>.*?</span>"),
                                            QRegularExpression::CaseInsensitiveOption |
                                                QRegularExpression::DotMatchesEverythingOption));
        item.sizeText = plainText(sizeCell);

        const QString swarmCell = tableCell(row, QStringLiteral("swarmsize"));
        const QRegularExpression swarmRe(QStringLiteral("(\\d+)\\s*/\\s*(\\d+)"));
        const QRegularExpressionMatch swarm = swarmRe.match(swarmCell);
        if (swarm.hasMatch()) {
            item.seeders = swarm.captured(1).toInt();
            item.leechers = swarm.captured(2).toInt();
        }
        item.added = plainText(tableCell(row, QStringLiteral("added")));

        const QString downloadCell = tableCell(row, QStringLiteral("download"));
        item.magnet = attrValue(downloadCell, QStringLiteral("href\\s*=\\s*\"(magnet:\\?[^\"]*)\""));
        const QString idSource = downloadCell + nameCell;
        const QRegularExpression idRe(QStringLiteral("(?:action=Download|view=TorrentDetail)(?:&amp;|&)id=(\\d+)"),
                                       QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch idMatch = idRe.match(idSource);
        if (idMatch.hasMatch()) {
            item.id = idMatch.captured(1).toLongLong();
        }
        if (item.name.isEmpty() || (item.magnet.isEmpty() && item.id <= 0)) {
            continue;
        }
        page.rows.push_back(item);
    }

    const QRegularExpression startRe(QStringLiteral("[?&]start=(\\d+)"));
    auto starts = startRe.globalMatch(html);
    while (starts.hasNext()) {
        const int start = starts.next().captured(1).toInt();
        if (start > page.start && (page.nextStart < 0 || start < page.nextStart)) {
            page.nextStart = start;
        }
        if (start < page.start && start > page.prevStart) {
            page.prevStart = start;
        }
    }
    if (page.nextStart < 0 && page.rows.size() >= page.limit) {
        page.nextStart = page.start + page.limit;
    }
    if (page.start > 0 && page.prevStart < 0) {
        page.prevStart = std::max(0, page.start - page.limit);
    }
    return page;
}

PostmanPage PostmanCatalog::fetch(const PostmanQuery &query, const PostmanProxy &proxy, QString *error)
{
    QMutexLocker lock(&mutex_);
    if (error) {
        error->clear();
    }
    const ProxyEndpoint endpoint = proxyEndpoint(proxy);
    Session session(&cookies_, endpoint);
    const QUrl url = catalogUrl(query);
    HttpBody body = request(session.manager(), QByteArrayLiteral("GET"), url, {}, session.proxy(), error);
    if (!body.ok) {
        return {};
    }
    QString html = QString::fromUtf8(body.bytes);
    if (postmanNeedsEnter(html)) {
        if (!submitEnter(session.manager(), html, session.proxy(), error)) {
            return {};
        }
        body = request(session.manager(), QByteArrayLiteral("GET"), url, {}, session.proxy(), error);
        if (!body.ok) {
            return {};
        }
        html = QString::fromUtf8(body.bytes);
        if (postmanNeedsEnter(html)) {
            if (error) {
                *error = trKey(QStringLiteral("postman_heavy_load"));
            }
            return {};
        }
    }
    if (error) {
        error->clear();
    }
    return parsePostmanPage(html, query);
}

QByteArray PostmanCatalog::downloadTorrent(qint64 id, const PostmanProxy &proxy, QString *error)
{
    QMutexLocker lock(&mutex_);
    if (error) {
        error->clear();
    }
    if (id <= 0) {
        if (error) {
            *error = trKey(QStringLiteral("postman_download_failed"));
        }
        return {};
    }
    const ProxyEndpoint endpoint = proxyEndpoint(proxy);
    Session session(&cookies_, endpoint);
    QUrl url = originUrl();
    url.setPath(QStringLiteral("/index.php"));
    url.setQuery(QStringLiteral("action=Download&id=%1").arg(id));
    HttpBody body = request(session.manager(), QByteArrayLiteral("GET"), url, {}, session.proxy(), error);
    if (!body.ok) {
        return {};
    }
    if (postmanNeedsEnter(QString::fromUtf8(body.bytes.left(4096)))) {
        if (!submitEnter(session.manager(), QString::fromUtf8(body.bytes), session.proxy(), error)) {
            return {};
        }
        body = request(session.manager(), QByteArrayLiteral("GET"), url, {}, session.proxy(), error);
        if (!body.ok) {
            return {};
        }
    }
    const bool torrent = !body.bytes.isEmpty() && body.bytes.at(0) == 'd' &&
                         !postmanNeedsEnter(QString::fromUtf8(body.bytes.left(4096)));
    if (!torrent) {
        if (error) {
            *error = trKey(QStringLiteral("postman_download_failed"));
        }
        return {};
    }
    if (error) {
        error->clear();
    }
    return body.bytes;
}

} // namespace i2p
