#include "i18n.hpp"

#include <QTest>

class I18nTests final : public QObject {
    Q_OBJECT

private slots:
    void defaultLanguageIsEnglish();
    void russianLanguage();
    void normalizeLanguageAliases();
    void aboutStrings();
    void translationCatalogsHaveSameKeys();
};

void I18nTests::defaultLanguageIsEnglish()
{
    i2p::setLanguage(QStringLiteral("en"));
    QCOMPARE(i2p::trKey(QStringLiteral("settings")), QStringLiteral("Settings"));
    QCOMPARE(i2p::trKey(QStringLiteral("add_torrent")), QStringLiteral("Add torrent"));
    QVERIFY(i2p::trKey(QStringLiteral("add_torrent_tip")).contains(QStringLiteral("{shortcut}")));
}

void I18nTests::russianLanguage()
{
    i2p::setLanguage(QStringLiteral("ru"));
    QCOMPARE(i2p::trKey(QStringLiteral("settings")), QStringLiteral("Настройки"));
    QCOMPARE(i2p::trKey(QStringLiteral("add_torrent")), QStringLiteral("Добавить торрент"));
    i2p::setLanguage(QStringLiteral("en"));
}

void I18nTests::normalizeLanguageAliases()
{
    QCOMPARE(i2p::normalizeLanguage(QStringLiteral("ru")), QStringLiteral("ru"));
    QCOMPARE(i2p::normalizeLanguage(QStringLiteral("Русский")), QStringLiteral("ru"));
    QCOMPARE(i2p::normalizeLanguage(QStringLiteral("en")), QStringLiteral("en"));
    QCOMPARE(i2p::normalizeLanguage(QStringLiteral("de")), QStringLiteral("en"));
}

void I18nTests::aboutStrings()
{
    i2p::setLanguage(QStringLiteral("en"));
    QCOMPARE(i2p::trKey(QStringLiteral("about")), QStringLiteral("About"));
    const QString body = i2p::trArgs(QStringLiteral("about_body"),
                                     {{QStringLiteral("version"), QStringLiteral("0.1.0")},
                                      {QStringLiteral("author"), QStringLiteral("Vade")},
                                      {QStringLiteral("license"), QStringLiteral("BSD-3-Clause")}});
    QVERIFY(body.contains(QStringLiteral("BSD-3-Clause")));
    i2p::setLanguage(QStringLiteral("ru"));
    QCOMPARE(i2p::trKey(QStringLiteral("about")), QStringLiteral("О программе"));
    i2p::setLanguage(QStringLiteral("en"));
}

void I18nTests::translationCatalogsHaveSameKeys()
{
    i2p::setLanguage(QStringLiteral("en"));
    const QString enSettings = i2p::trKey(QStringLiteral("settings"));
    i2p::setLanguage(QStringLiteral("ru"));
    const QString ruSettings = i2p::trKey(QStringLiteral("settings"));
    QVERIFY(!enSettings.isEmpty());
    QVERIFY(!ruSettings.isEmpty());
}

int runI18nTests(int argc, char *argv[])
{
    I18nTests suite;
    return QTest::qExec(&suite, argc, argv);
}

#include "test_i18n.moc"
