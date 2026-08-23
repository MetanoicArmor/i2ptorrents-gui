#include "rpc.hpp"

#include "i18n.hpp"

#include <QEventLoop>
#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace i2p {

namespace {

constexpr int MAX_RESPONSE_BYTES = 8 * 1024 * 1024;
constexpr int RPC_TIMEOUT_MS = 5000;

QHash<QString, QString> makeArgs(const QString &key, const QString &value)
{
    QHash<QString, QString> args;
    args.insert(key, value);
    return args;
}

} // namespace

std::optional<QString> normalizeRpcUrl(const QString &value, QString *error)
{
    QString raw = value.trimmed();
    if (raw.isEmpty()) {
        if (error) {
            *error = trKey(QStringLiteral("rpc_url_required"));
        }
        return std::nullopt;
    }
    if (!raw.contains(QStringLiteral("://"))) {
        raw = QStringLiteral("http://") + raw;
    }
    QUrl parsed(raw);
    if (!parsed.isValid() || (parsed.scheme() != QStringLiteral("http") &&
                              parsed.scheme() != QStringLiteral("https"))) {
        if (error) {
            *error = trKey(QStringLiteral("rpc_url_invalid"));
        }
        return std::nullopt;
    }
    QString host = parsed.host(QUrl::FullyEncoded).trimmed();
    while (host.endsWith(QLatin1Char('.'))) {
        host.chop(1);
    }
    host = host.toLower();
    if (host != QStringLiteral("localhost")) {
        QHostAddress address;
        if (!address.setAddress(host)) {
            if (error) {
                *error = trKey(QStringLiteral("rpc_url_local_only"));
            }
            return std::nullopt;
        }
        if (!address.isLoopback()) {
            if (error) {
                *error = trKey(QStringLiteral("rpc_url_local_only"));
            }
            return std::nullopt;
        }
    }

    QString path = parsed.path();
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    if (!path.endsWith(QStringLiteral("/rpc"))) {
        path += QStringLiteral("/rpc");
    }
    path += QLatin1Char('/');
    parsed.setPath(path);
    return parsed.toString(QUrl::FullyEncoded);
}

bool rpcMethodUnsupported(const QString &message)
{
    const QString lower = message.toLower();
    return lower.contains(QStringLiteral("method not found")) || lower.contains(QStringLiteral("-32601"));
}

RpcClient::RpcClient(const QString &endpoint)
    : endpoint_(endpoint)
    , post_(defaultPost)
{
    if (const std::optional<QString> normalized = normalizeRpcUrl(endpoint)) {
        endpoint_ = *normalized;
    } else {
        endpoint_ = endpoint;
    }
}

RpcClient::RpcClient(QString endpoint, PostHandler handler)
    : endpoint_(std::move(endpoint))
    , post_(std::move(handler))
{
}

QByteArray RpcClient::defaultPost(const QString &endpoint, const QByteArray &body, QString *error)
{
    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(endpoint)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "i2ptorrents-gui/0.1");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);

    QNetworkReply *reply = manager.post(request, body);
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(RPC_TIMEOUT_MS);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start();
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        if (error) {
            *error = trArgs(QStringLiteral("rpc_no_connection"),
                            makeArgs(QStringLiteral("reason"), QStringLiteral("timeout")));
        }
        reply->deleteLater();
        return {};
    }

    if (reply->error() != QNetworkReply::NoError) {
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString detail = QString::fromUtf8(reply->readAll()).trimmed();
        if (error) {
            if (code > 0) {
                QHash<QString, QString> args;
                args.insert(QStringLiteral("code"), QString::number(code));
                args.insert(QStringLiteral("detail"), detail);
                *error = trArgs(QStringLiteral("rpc_http"), args);
            } else {
                *error = trArgs(QStringLiteral("rpc_no_connection"),
                                makeArgs(QStringLiteral("reason"), reply->errorString()));
            }
        }
        reply->deleteLater();
        return {};
    }

    QByteArray raw = reply->readAll();
    reply->deleteLater();
    if (raw.size() > MAX_RESPONSE_BYTES) {
        if (error) {
            *error = trKey(QStringLiteral("rpc_too_large"));
        }
        return {};
    }
    return raw;
}

QJsonValue RpcClient::rpcArguments(const QJsonObject &payload)
{
    const QJsonValue arguments = payload.value(QStringLiteral("arguments"));
    if (arguments.isObject()) {
        return arguments;
    }
    const QJsonValue result = payload.value(QStringLiteral("result"));
    if (result.isObject()) {
        return result;
    }
    return QJsonObject{};
}

QJsonValue RpcClient::call(const QString &method, const QJsonObject &arguments, QString *error)
{
    QJsonObject body;
    body.insert(QStringLiteral("method"), method);
    body.insert(QStringLiteral("arguments"), arguments);
    body.insert(QStringLiteral("tag"), static_cast<qint64>(++tag_));

    const QByteArray encoded = QJsonDocument(body).toJson(QJsonDocument::Compact);
    const QByteArray raw = post_(endpoint_, encoded, error);
    if (raw.isEmpty()) {
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) {
            *error = trKey(QStringLiteral("rpc_bad_response"));
        }
        return {};
    }
    const QJsonObject payload = doc.object();
    if (payload.contains(QStringLiteral("error"))) {
        const QJsonValue err = payload.value(QStringLiteral("error"));
        QString message;
        if (err.isObject()) {
            message = err.toObject().value(QStringLiteral("message")).toString(trKey(QStringLiteral("rpc_unknown_error")));
        } else {
            message = err.toVariant().toString();
        }
        if (error) {
            *error = trArgs(QStringLiteral("rpc_error"), makeArgs(QStringLiteral("message"), message));
        }
        return {};
    }
    if (payload.contains(QStringLiteral("result"))) {
        const QJsonValue result = payload.value(QStringLiteral("result"));
        if (!result.isNull() && !result.isObject() && result.toString() != QStringLiteral("success")) {
            if (error) {
                *error = trArgs(QStringLiteral("rpc_error"),
                                makeArgs(QStringLiteral("message"), result.toVariant().toString()));
            }
            return {};
        }
    }
    return rpcArguments(payload);
}

QVector<Torrent> RpcClient::torrentRows(const QJsonValue &result, QString *error)
{
    if (!result.isObject()) {
        if (error) {
            *error = trKey(QStringLiteral("rpc_bad_list"));
        }
        return {};
    }
    const QJsonArray rows = result.toObject().value(QStringLiteral("torrents")).toArray();
    QVector<Torrent> torrents;
    torrents.reserve(rows.size());
    for (const QJsonValue &row : rows) {
        if (const std::optional<Torrent> torrent = torrentFromRpc(row)) {
            torrents.push_back(*torrent);
        }
    }
    return torrents;
}

QVector<Torrent> RpcClient::getTorrents(bool detailed, QString *error)
{
    QJsonArray fields;
    for (int index = 0; index < RPC_FIELD_COUNT; ++index) {
        const char *field = RPC_FIELDS[index];
        if (!detailed && qstrcmp(field, "pieces") == 0) {
            continue;
        }
        fields.append(QString::fromUtf8(field));
    }
    const QJsonValue result = call(QStringLiteral("torrent-get"), {{QStringLiteral("fields"), fields}}, error);
    return torrentRows(result, error);
}

QVector<TorrentFile> RpcClient::getTorrentFiles(qint64 torrentId, QString *error)
{
    QJsonArray fields;
    for (int index = 0; index < RPC_FILE_FIELD_COUNT; ++index) {
        fields.append(QString::fromUtf8(RPC_FILE_FIELDS[index]));
    }
    QJsonObject arguments;
    arguments.insert(QStringLiteral("ids"), QJsonArray{torrentId});
    arguments.insert(QStringLiteral("fields"), fields);
    const QJsonValue result = call(QStringLiteral("torrent-get"), arguments, error);
    const QVector<Torrent> torrents = torrentRows(result, error);
    if (torrents.isEmpty()) {
        return {};
    }
    return torrents.first().files;
}

QVector<Peer> RpcClient::getTorrentPeers(qint64 torrentId, QString *error)
{
    QJsonArray fields;
    for (int index = 0; index < RPC_PEER_FIELD_COUNT; ++index) {
        fields.append(QString::fromUtf8(RPC_PEER_FIELDS[index]));
    }
    QJsonObject arguments;
    arguments.insert(QStringLiteral("ids"), QJsonArray{torrentId});
    arguments.insert(QStringLiteral("fields"), fields);
    const QJsonValue result = call(QStringLiteral("torrent-get"), arguments, error);
    if (!result.isObject()) {
        return {};
    }
    const QJsonArray rows = result.toObject().value(QStringLiteral("torrents")).toArray();
    if (rows.isEmpty() || !rows.first().isObject()) {
        return {};
    }
    return parseTorrentPeers(rows.first().toObject());
}

QJsonObject RpcClient::addTorrentBytes(const QByteArray &content, QString *error)
{
    if (content.isEmpty()) {
        if (error) {
            *error = trKey(QStringLiteral("rpc_empty_file"));
        }
        return {};
    }
    const QString metainfo = QString::fromLatin1(content.toBase64());
    const QJsonValue result =
        call(QStringLiteral("torrent-add"), {{QStringLiteral("metainfo"), metainfo}}, error);
    if (!result.isObject()) {
        return {};
    }
    const QJsonObject obj = result.toObject();
    if (obj.contains(QStringLiteral("torrent-added"))) {
        return obj.value(QStringLiteral("torrent-added")).toObject();
    }
    if (obj.contains(QStringLiteral("torrent-duplicate"))) {
        return obj.value(QStringLiteral("torrent-duplicate")).toObject();
    }
    return {};
}

QJsonObject RpcClient::addTorrentPath(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            QHash<QString, QString> args;
            args.insert(QStringLiteral("error"), file.errorString());
            *error = trArgs(QStringLiteral("rpc_read_failed"), args);
        }
        return {};
    }
    return addTorrentBytes(file.readAll(), error);
}

bool RpcClient::removeTorrent(qint64 torrentId, bool deleteData, QString *error)
{
    QJsonObject arguments;
    arguments.insert(QStringLiteral("ids"), QJsonArray{torrentId});
    arguments.insert(QStringLiteral("delete-local-data"), deleteData);
    call(QStringLiteral("torrent-remove"), arguments, error);
    return error == nullptr || error->isEmpty();
}

bool RpcClient::setFilePriority(qint64 torrentId,
                                qint64 index,
                                bool wanted,
                                qint64 priority,
                                QString *error)
{
    QJsonObject arguments;
    arguments.insert(QStringLiteral("ids"), QJsonArray{torrentId});
    if (wanted) {
        arguments.insert(QStringLiteral("files-wanted"), QJsonArray{index});
        const char *key = priority == -1   ? "priority-low"
                          : priority == 1 ? "priority-high"
                                          : "priority-normal";
        arguments.insert(QString::fromUtf8(key), QJsonArray{index});
    } else {
        arguments.insert(QStringLiteral("files-unwanted"), QJsonArray{index});
    }
    call(QStringLiteral("torrent-set"), arguments, error);
    return error == nullptr || error->isEmpty();
}

} // namespace i2p
