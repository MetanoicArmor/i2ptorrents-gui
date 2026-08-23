#include "i18n.hpp"

#include "resources.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QThreadStorage>

namespace i2p {

namespace {

QHash<QString, QHash<QString, QString>> loadCatalog()
{
    QHash<QString, QHash<QString, QString>> catalog;
    for (const QString &lang : {QStringLiteral("en"), QStringLiteral("ru")}) {
        const QString raw = loadTextResource(QStringLiteral(":/i18n/%1.json").arg(lang));
        const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
        const QJsonObject obj = doc.object();
        QHash<QString, QString> strings;
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            strings.insert(it.key(), it.value().toString());
        }
        catalog.insert(lang, strings);
    }
    return catalog;
}

const QHash<QString, QHash<QString, QString>> &catalog()
{
    static const QHash<QString, QHash<QString, QString>> data = loadCatalog();
    return data;
}

QThreadStorage<QString> &threadLanguage()
{
    static QThreadStorage<QString> storage;
    if (!storage.hasLocalData()) {
        storage.setLocalData(QStringLiteral("en"));
    }
    return storage;
}

QString lookup(const QString &lang, const QString &key)
{
    const auto &cat = catalog();
    if (cat.contains(lang) && cat[lang].contains(key)) {
        return cat[lang][key];
    }
    if (cat.contains(QStringLiteral("en")) && cat[QStringLiteral("en")].contains(key)) {
        return cat[QStringLiteral("en")][key];
    }
    return key;
}

} // namespace

QString normalizeLanguage(const QString &value)
{
    const QString raw = value.trimmed().toLower();
    if (raw == QStringLiteral("ru") || raw == QStringLiteral("russian") || raw.contains(QStringLiteral("рус"))) {
        return QStringLiteral("ru");
    }
    return QStringLiteral("en");
}

void setLanguage(const QString &value)
{
    threadLanguage().setLocalData(normalizeLanguage(value));
}

QString language()
{
    return threadLanguage().localData();
}

QString trKey(const QString &key)
{
    return lookup(language(), key);
}

QString trArgs(const QString &key, const QHash<QString, QString> &args)
{
    QString text = trKey(key);
    for (auto it = args.begin(); it != args.end(); ++it) {
        text.replace(QStringLiteral("{%1}").arg(it.key()), it.value());
        const QRegularExpression pattern(
            QStringLiteral("\\{%1:[^}]*\\}").arg(QRegularExpression::escape(it.key())));
        text.replace(pattern, it.value());
    }
    return text;
}

} // namespace i2p
