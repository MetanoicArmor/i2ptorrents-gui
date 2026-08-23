#pragma once

#include <QVector>

namespace i2p {

enum class PieceFill { Empty, Partial, Have };

QVector<PieceFill> pieceColumnFill(const QVector<bool> &have, int width);

} // namespace i2p
