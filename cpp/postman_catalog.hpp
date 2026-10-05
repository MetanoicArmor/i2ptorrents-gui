#pragma once

#include <QList>
#include <QMutex>
#include <QNetworkCookie>
#include <QString>
#include <QVector>

namespace i2p {

inline constexpr int POSTMAN_PROXY_PORT = 4444;
inline constexpr char POSTMAN_PROXY_HOST[] = "127.0.0.1";
inline constexpr char POSTMAN_ORIGIN[] = "http://tracker2.postman.i2p/";

struct PostmanQuery {
    QString search;
    int category = -1;
    int orderBy = -1;
    int start = 0;
    int limit = 40;
};

struct PostmanTorrent {
    qint64 id = 0;
    QString name;
    QString category;
    QString sizeText;
    int seeders = -1;
    int leechers = -1;
    QString added;
    QString magnet;
    QString summary;
};

struct PostmanPage {
    QVector<PostmanTorrent> rows;
    int start = 0;
    int limit = 40;
    int nextStart = -1;
    int prevStart = -1;
};

struct PostmanCategory {
    int id = -1;
    const char *labelKey = nullptr;
    const char *label = nullptr;
};

struct PostmanSort {
    int id = -1;
    const char *labelKey = nullptr;
};

const QVector<PostmanCategory> &postmanCategories();
const QVector<PostmanSort> &postmanSorts();

bool postmanNeedsEnter(const QString &html);
QString postmanEnterToken(const QString &html);
PostmanPage parsePostmanPage(const QString &html, const PostmanQuery &query);

struct PostmanProxy {
    bool socks = false;
    int port = POSTMAN_PROXY_PORT;
};

class PostmanCatalog {
public:
    PostmanPage fetch(const PostmanQuery &query,
                      const PostmanProxy &proxy,
                      QString *error = nullptr);
    QByteArray downloadTorrent(qint64 id, const PostmanProxy &proxy, QString *error = nullptr);

private:
    QMutex mutex_;
    QList<QNetworkCookie> cookies_;
};

} // namespace i2p
