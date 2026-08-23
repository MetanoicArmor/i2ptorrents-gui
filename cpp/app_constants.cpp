#include "app_constants.hpp"

#include "resources.hpp"

namespace i2p {

QString appVersion()
{
    const QString raw = loadTextResource(":/VERSION").trimmed();
    return raw.isEmpty() ? QStringLiteral("0.1.0") : raw;
}

} // namespace i2p
