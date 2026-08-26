#include "torrent_create.hpp"

#include "bencode.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QVector>
#include <functional>

namespace i2p {

namespace {

constexpr quint32 kMinPiece = 16u * 1024u;
constexpr quint32 kMaxPiece = 16u * 1024u * 1024u;

struct TorrentFileEntry {
    QString absolutePath;
    QStringList pathParts; // relative UTF-8 path segments for multi-file
    qint64 length = 0;
};

QStringList splitTrackers(const QStringList &raw)
{
    QStringList out;
    for (const QString &line : raw) {
        for (const QString &part : line.split(QRegularExpression(QStringLiteral("[,\\s]+")),
                                              Qt::SkipEmptyParts)) {
            const QString trimmed = part.trimmed();
            if (!trimmed.isEmpty() && !out.contains(trimmed)) {
                out << trimmed;
            }
        }
    }
    return out;
}

bool collectFiles(const QString &source,
                  QVector<TorrentFileEntry> *files,
                  QString *name,
                  bool *multi,
                  QString *error)
{
    const QFileInfo info(source);
    if (!info.exists()) {
        if (error) {
            *error = QStringLiteral("Source path does not exist");
        }
        return false;
    }

    if (info.isFile()) {
        *multi = false;
        *name = info.fileName();
        TorrentFileEntry entry;
        entry.absolutePath = info.absoluteFilePath();
        entry.length = info.size();
        if (entry.length < 0) {
            if (error) {
                *error = QStringLiteral("Invalid file size");
            }
            return false;
        }
        files->push_back(entry);
        return true;
    }

    if (!info.isDir()) {
        if (error) {
            *error = QStringLiteral("Source must be a file or directory");
        }
        return false;
    }

    *multi = true;
    *name = info.fileName();
    const QDir root(info.absoluteFilePath());

    std::function<void(const QString &)> walk = [&](const QString &rel) {
        const QDir dir = rel.isEmpty() ? root : QDir(root.filePath(rel));
        const QFileInfoList entries =
            dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &entryInfo : entries) {
            const QString childRel =
                rel.isEmpty() ? entryInfo.fileName() : (rel + QLatin1Char('/') + entryInfo.fileName());
            if (entryInfo.isDir()) {
                walk(childRel);
                continue;
            }
            if (!entryInfo.isFile()) {
                continue;
            }
            TorrentFileEntry entry;
            entry.absolutePath = entryInfo.absoluteFilePath();
            entry.length = entryInfo.size();
            entry.pathParts = childRel.split(QLatin1Char('/'), Qt::SkipEmptyParts);
            files->push_back(entry);
        }
    };
    walk(QString());

    if (files->isEmpty()) {
        if (error) {
            *error = QStringLiteral("Directory has no files");
        }
        return false;
    }
    return true;
}

} // namespace

quint32 defaultPieceSize(quint64 totalSize)
{
    quint32 size = kMinPiece;
    while (size < kMaxPiece && totalSize / size > 2000ull) {
        size *= 2u;
    }
    return size;
}

bool isLegalPieceSize(quint32 pieceSize)
{
    if (pieceSize < kMinPiece || pieceSize > kMaxPiece) {
        return false;
    }
    return (pieceSize & (pieceSize - 1u)) == 0u;
}

std::optional<QByteArray> createTorrentMetainfo(const CreateTorrentRequest &request,
                                               QString *error,
                                               std::atomic_bool *cancel,
                                               const CreateTorrentProgressFn &onProgress)
{
    QVector<TorrentFileEntry> files;
    QString name;
    bool multi = false;
    if (!collectFiles(request.sourcePath.trimmed(), &files, &name, &multi, error)) {
        return std::nullopt;
    }

    quint64 total = 0;
    for (const TorrentFileEntry &file : files) {
        total += static_cast<quint64>(file.length);
    }
    if (total == 0) {
        if (error) {
            *error = QStringLiteral("Total size is zero");
        }
        return std::nullopt;
    }

    quint32 pieceSize = request.pieceSize == 0 ? defaultPieceSize(total) : request.pieceSize;
    if (!isLegalPieceSize(pieceSize)) {
        if (error) {
            *error = QStringLiteral("Piece size must be a power of two and at least 16 KiB");
        }
        return std::nullopt;
    }

    const quint64 pieceCount = (total + pieceSize - 1ull) / pieceSize;
    QByteArray pieces;
    pieces.reserve(static_cast<int>(pieceCount * 20));

    QCryptographicHash hasher(QCryptographicHash::Sha1);
    quint32 inPiece = 0;
    quint64 pieceIndex = 0;

    auto flushPiece = [&]() {
        pieces += hasher.result();
        hasher.reset();
        inPiece = 0;
        ++pieceIndex;
        if (onProgress) {
            onProgress(CreateTorrentProgress{pieceIndex, pieceCount});
        }
    };

    if (onProgress) {
        onProgress(CreateTorrentProgress{0, pieceCount});
    }

    for (const TorrentFileEntry &entry : files) {
        if (cancel && cancel->load()) {
            if (error) {
                *error = QStringLiteral("Cancelled");
            }
            return std::nullopt;
        }
        QFile file(entry.absolutePath);
        if (!file.open(QIODevice::ReadOnly)) {
            if (error) {
                *error = file.errorString();
            }
            return std::nullopt;
        }
        while (!file.atEnd()) {
            if (cancel && cancel->load()) {
                if (error) {
                    *error = QStringLiteral("Cancelled");
                }
                return std::nullopt;
            }
            const quint32 want = pieceSize - inPiece;
            const QByteArray chunk = file.read(want);
            if (chunk.isEmpty() && !file.atEnd()) {
                if (error) {
                    *error = file.errorString();
                }
                return std::nullopt;
            }
            if (chunk.isEmpty()) {
                break;
            }
            hasher.addData(chunk);
            inPiece += static_cast<quint32>(chunk.size());
            if (inPiece == pieceSize) {
                flushPiece();
            }
        }
    }
    if (inPiece > 0) {
        flushPiece();
    }

    QVariantHash info;
    info.insert(QStringLiteral("name"), name);
    info.insert(QStringLiteral("piece length"), static_cast<qint64>(pieceSize));
    info.insert(QStringLiteral("pieces"), pieces);
    if (request.isPrivate) {
        info.insert(QStringLiteral("private"), 1);
    }
    if (multi) {
        QVariantList fileList;
        for (const TorrentFileEntry &entry : files) {
            QVariantHash item;
            item.insert(QStringLiteral("length"), entry.length);
            QVariantList path;
            for (const QString &part : entry.pathParts) {
                path << part;
            }
            item.insert(QStringLiteral("path"), path);
            fileList << item;
        }
        info.insert(QStringLiteral("files"), fileList);
    } else {
        info.insert(QStringLiteral("length"), files.first().length);
    }

    const QStringList trackers = splitTrackers(request.trackers);
    QVariantHash root;
    if (!trackers.isEmpty()) {
        root.insert(QStringLiteral("announce"), trackers.first());
        if (trackers.size() > 1) {
            QVariantList announceList;
            for (const QString &url : trackers) {
                announceList << QVariantList{url};
            }
            root.insert(QStringLiteral("announce-list"), announceList);
        }
    }
    if (!request.comment.trimmed().isEmpty()) {
        root.insert(QStringLiteral("comment"), request.comment.trimmed());
    }
    const QString createdBy =
        request.createdBy.trimmed().isEmpty() ? QStringLiteral("I2P Torrents") : request.createdBy.trimmed();
    root.insert(QStringLiteral("created by"), createdBy);
    root.insert(QStringLiteral("creation date"),
                static_cast<qint64>(QDateTime::currentSecsSinceEpoch()));
    root.insert(QStringLiteral("info"), info);

    return bencode(root);
}

bool saveTorrentMetainfo(const QByteArray &metainfo, const QString &path, QString *error)
{
    if (metainfo.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Empty metainfo");
        }
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    if (file.write(metainfo) != metainfo.size()) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    return true;
}

} // namespace i2p
