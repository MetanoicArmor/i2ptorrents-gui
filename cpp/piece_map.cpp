#include "piece_map.hpp"

namespace i2p {

QVector<PieceFill> pieceColumnFill(const QVector<bool> &have, int width)
{
    if (width <= 0) {
        return {};
    }
    const int total = have.size();
    QVector<PieceFill> columns;
    columns.reserve(width);
    for (int column = 0; column < width; ++column) {
        if (total == 0) {
            columns.push_back(PieceFill::Empty);
            continue;
        }
        const int start = column * total / width;
        const int end = qMax(start + 1, (column + 1) * total / width);
        bool any = false;
        bool all = true;
        for (int index = start; index < end && index < total; ++index) {
            any = any || have[index];
            all = all && have[index];
        }
        if (!any) {
            columns.push_back(PieceFill::Empty);
        } else if (all) {
            columns.push_back(PieceFill::Have);
        } else {
            columns.push_back(PieceFill::Partial);
        }
    }
    return columns;
}

} // namespace i2p
