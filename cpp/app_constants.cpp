#include "app_constants.hpp"

#include "resources.hpp"

namespace i2p {

QString appVersion()
{
#ifdef I2P_APP_VERSION
    const QString compiled = QStringLiteral(I2P_APP_VERSION);
    if (!compiled.isEmpty()) {
        return compiled;
    }
#endif

    const QString embedded = loadTextResource(QStringLiteral(":/VERSION")).trimmed();
    if (!embedded.isEmpty()) {
        return embedded;
    }

    const QString diskPath = resourcePath(QStringLiteral("VERSION"));
    if (!diskPath.isEmpty()) {
        const QString version = loadTextResource(diskPath).trimmed();
        if (!version.isEmpty()) {
            return version;
        }
    }

    return QStringLiteral("0.0.0");
}

} // namespace i2p
