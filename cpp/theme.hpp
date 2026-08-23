#pragma once

#include <QString>
#include <utility>

namespace i2p {

QString stylesheet(const QString &theme);
std::pair<QString, QString> tooltipPaletteColors(const QString &theme);

} // namespace i2p
