#include "theme.hpp"

#include <QTest>

class ThemeTests final : public QObject {
    Q_OBJECT

private slots:
    void tooltipPaletteFollowsTheme();
};

void ThemeTests::tooltipPaletteFollowsTheme()
{
    const auto night = i2p::tooltipPaletteColors(QStringLiteral("night"));
    QCOMPARE(night.first, QStringLiteral("#2c2c2e"));
    QCOMPARE(night.second, QStringLiteral("#f5f5f7"));
    const auto light = i2p::tooltipPaletteColors(QStringLiteral("light"));
    QCOMPARE(light.first, QStringLiteral("#f2f2f7"));
    QCOMPARE(light.second, QStringLiteral("#1d1d1f"));
    const auto other = i2p::tooltipPaletteColors(QStringLiteral("other"));
    QCOMPARE(other.first, QStringLiteral("#f2f2f7"));
    QCOMPARE(other.second, QStringLiteral("#1d1d1f"));
}

int runThemeTests(int argc, char *argv[])
{
    ThemeTests suite;
    return QTest::qExec(&suite, argc, argv);
}

#include "test_theme.moc"
