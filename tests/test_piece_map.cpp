#include "piece_map.hpp"

#include <QTest>

class PieceMapTests final : public QObject {
    Q_OBJECT

private slots:
    void emptyHaveIsEmptyColumns();
    void allHaveFillsEveryColumn();
    void mixedBitsProducePartialColumns();
};

void PieceMapTests::emptyHaveIsEmptyColumns()
{
    const QVector<i2p::PieceFill> fills = i2p::pieceColumnFill({}, 4);
    QCOMPARE(fills.size(), 4);
    for (i2p::PieceFill fill : fills) {
        QCOMPARE(fill, i2p::PieceFill::Empty);
    }
}

void PieceMapTests::allHaveFillsEveryColumn()
{
    const QVector<bool> have(4, true);
    const QVector<i2p::PieceFill> fills = i2p::pieceColumnFill(have, 8);
    for (i2p::PieceFill fill : fills) {
        QCOMPARE(fill, i2p::PieceFill::Have);
    }
}

void PieceMapTests::mixedBitsProducePartialColumns()
{
    const QVector<bool> have{true, false};
    const QVector<i2p::PieceFill> fills = i2p::pieceColumnFill(have, 1);
    QCOMPARE(fills, (QVector<i2p::PieceFill>{i2p::PieceFill::Partial}));
}

int runPieceMapTests(int argc, char *argv[])
{
    PieceMapTests suite;
    return QTest::qExec(&suite, argc, argv);
}

#include "test_piece_map.moc"
