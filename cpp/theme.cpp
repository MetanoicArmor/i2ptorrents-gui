#include "theme.hpp"

#include "resources.hpp"

namespace i2p {

namespace {

QString appleFontOverlay()
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    return QStringLiteral(R"(
QWidget {
    font-family: "Inter", "SF Pro Text", "SF Pro Display", sans-serif;
}
)");
#else
    return {};
#endif
}

QString macOverlay(const QString &theme)
{
    if (theme == QStringLiteral("night")) {
        return QStringLiteral(R"(
QMainWindow, QWidget#MainWindow { background: transparent; }
QFrame#Sidebar, QWidget#Sidebar {
    background: transparent;
    border: none;
}
QFrame#Surface, QWidget#Surface {
    background: #1c1c1e;
    border: none;
}
QWidget#PaneSplit { background: rgba(255, 255, 255, 0.10); }
QFrame#TorrentCard, QWidget#TorrentCard { background: transparent; border: none; }
)");
    }
    return QStringLiteral(R"(
QMainWindow, QWidget#MainWindow { background: transparent; }
QFrame#Sidebar, QWidget#Sidebar {
    background: transparent;
    border: none;
}
QFrame#Surface, QWidget#Surface {
    background: #ffffff;
    border: none;
}
QWidget#PaneSplit { background: rgba(0, 0, 0, 0.10); }
QFrame#TorrentCard, QWidget#TorrentCard { background: transparent; border: none; }
)");
}

QString windowsOverlay(const QString &theme)
{
    if (theme == QStringLiteral("night")) {
        return QStringLiteral(R"(
QMainWindow, QWidget#MainWindow { background: transparent; }
QDialog { background: transparent; }
QFrame#Sidebar, QWidget#Sidebar {
    background: transparent;
    border: none;
}
QFrame#Surface, QWidget#Surface {
    background: transparent;
    border: none;
}
QWidget#PaneSplit { background: #3d3d3f; }
QFrame#TorrentCard, QFrame#SummaryCard, QWidget#TorrentCard, QWidget#SummaryCard {
    background: transparent;
    border: none;
    border-radius: 10px;
}
QWidget#FilesTablePane, QWidget#PeersTablePane {
    background: transparent;
    border: none;
    border-radius: 12px;
}
QWidget#FilesTableOverlay { background: transparent; border: none; }
QTableWidget#FilesTable, QTableWidget#PeersTable {
    background: transparent;
    border: none;
    border-radius: 12px;
    gridline-color: transparent;
    outline: 0;
    alternate-background-color: #3d3d40;
}
QTableWidget#FilesTable::viewport {
    background: transparent;
    border: none;
    border-bottom-left-radius: 12px;
    border-bottom-right-radius: 12px;
}
QTableWidget#FilesTable QHeaderView {
    background: transparent;
    border: none;
    border-top-left-radius: 12px;
    border-top-right-radius: 12px;
}
QTableWidget#FilesTable QHeaderView::section {
    background: #252527;
    color: #c7c7cc;
    border: none;
    border-bottom: 1px solid #48484a;
    padding: 8px 8px;
    font-weight: 600;
    font-size: 12px;
}
QTableWidget#FilesTable QHeaderView::section:first {
    border-top-left-radius: 12px;
}
QTableWidget#FilesTable QHeaderView::section:last {
    border-top-right-radius: 12px;
}
QTableWidget#FilesTable QScrollBar:horizontal { height: 0px; max-height: 0px; }
QTableWidget#FilesTable QScrollBar:vertical { width: 0px; max-width: 0px; }
QComboBoxPrivateContainer {
    background: transparent;
    border: none;
    padding: 0px;
}
QComboBox QAbstractItemView {
    background: transparent;
    border: none;
    outline: none;
}
)");
    }
    return QStringLiteral(R"(
QMainWindow, QWidget#MainWindow { background: transparent; }
QDialog { background: transparent; }
QFrame#Sidebar, QWidget#Sidebar {
    background: transparent;
    border: none;
}
QFrame#Surface, QWidget#Surface {
    background: transparent;
    border: none;
}
QWidget#PaneSplit { background: #d8d8dc; }
QFrame#TorrentCard, QFrame#SummaryCard, QWidget#TorrentCard, QWidget#SummaryCard {
    background: transparent;
    border: none;
    border-radius: 10px;
}
QWidget#FilesTablePane, QWidget#PeersTablePane {
    background: transparent;
    border: none;
    border-radius: 12px;
}
QWidget#FilesTableOverlay { background: transparent; border: none; }
QTableWidget#FilesTable, QTableWidget#PeersTable {
    background: transparent;
    border: none;
    border-radius: 12px;
    gridline-color: transparent;
    outline: 0;
    alternate-background-color: #e4e4e9;
}
QTableWidget#FilesTable::viewport {
    background: transparent;
    border: none;
    border-bottom-left-radius: 12px;
    border-bottom-right-radius: 12px;
}
QTableWidget#FilesTable QHeaderView {
    background: transparent;
    border: none;
    border-top-left-radius: 12px;
    border-top-right-radius: 12px;
}
QTableWidget#FilesTable QHeaderView::section {
    background: #ececf1;
    color: #6e6e73;
    border: none;
    border-bottom: 1px solid #d0d0d5;
    padding: 8px 8px;
    font-weight: 600;
    font-size: 12px;
}
QTableWidget#FilesTable QHeaderView::section:first {
    border-top-left-radius: 12px;
}
QTableWidget#FilesTable QHeaderView::section:last {
    border-top-right-radius: 12px;
}
QTableWidget#FilesTable QScrollBar:horizontal { height: 0px; max-height: 0px; }
QTableWidget#FilesTable QScrollBar:vertical { width: 0px; max-width: 0px; }
QDialog QLineEdit, QDialog QSpinBox, QDialog QComboBox {
    background: #ffffff;
    border: none;
}
QDialog QComboBox:hover { background: #fafafc; }
QDialog QFrame#SpinRow, QDialog QFrame#PathRow {
    background: #ffffff;
    border: none;
}
QDialog QFrame#SpinRow[focused="true"] { border: 1px solid #0a84ff; }
QDialog QPushButton {
    background: #ffffff;
    border: none;
}
QDialog QPushButton:hover { background: #fafafc; }
QDialog QPushButton:pressed { background: #ececf1; }
QDialog QPushButton#Primary {
    background: #0a84ff;
    color: #ffffff;
}
QDialog QPushButton#Primary:hover { background: #2d95ff; }
QComboBoxPrivateContainer {
    background: transparent;
    border: none;
    padding: 0px;
}
QComboBox QAbstractItemView {
    background: transparent;
    border: none;
    outline: none;
}
)");
}

QString linuxOverlay(const QString &theme)
{
    if (theme == QStringLiteral("night")) {
        return QStringLiteral(R"(
QMainWindow, QWidget#MainWindow { background: transparent; }
QDialog { background: transparent; }
QFrame#Sidebar, QWidget#Sidebar {
    background: transparent;
    border: none;
}
QFrame#Surface, QWidget#Surface {
    background: #1c1c1e;
    border: none;
}
QWidget#PaneSplit { background: rgba(255, 255, 255, 0.10); }
QFrame#TorrentCard, QWidget#TorrentCard { background: transparent; border: none; }
QWidget#FilesTablePane, QWidget#PeersTablePane { background: transparent; border: none; border-radius: 0px; }
QWidget#FilesTableOverlay { background: transparent; border: none; }
QTableWidget#FilesTable, QTableWidget#PeersTable { border-radius: 0px; outline: none; }
QTableWidget#FilesTable::viewport, QTableWidget#PeersTable::viewport { border-radius: 0px; background: transparent; }
QTableWidget#FilesTable QHeaderView::section, QTableWidget#PeersTable QHeaderView::section { border-radius: 0px; }
)");
    }
    return QStringLiteral(R"(
QMainWindow, QWidget#MainWindow { background: transparent; }
QDialog { background: transparent; }
QFrame#Sidebar, QWidget#Sidebar {
    background: transparent;
    border: none;
}
QFrame#Surface, QWidget#Surface {
    background: #ffffff;
    border: none;
}
QWidget#PaneSplit { background: rgba(0, 0, 0, 0.10); }
QFrame#TorrentCard, QWidget#TorrentCard { background: transparent; border: none; }
QWidget#FilesTablePane, QWidget#PeersTablePane { background: transparent; border: none; border-radius: 0px; }
QWidget#FilesTableOverlay { background: transparent; border: none; }
QTableWidget#FilesTable, QTableWidget#PeersTable { border-radius: 0px; outline: none; }
QTableWidget#FilesTable::viewport, QTableWidget#PeersTable::viewport { border-radius: 0px; background: transparent; }
QTableWidget#FilesTable QHeaderView::section, QTableWidget#PeersTable QHeaderView::section { border-radius: 0px; }
)");
}

QString platformOverlay(const QString &theme)
{
#if defined(Q_OS_LINUX)
    return linuxOverlay(theme);
#elif defined(Q_OS_WIN)
    return windowsOverlay(theme);
#else
    return macOverlay(theme);
#endif
}

} // namespace

QString stylesheet(const QString &theme)
{
    const QString base = loadTextResource(
        theme == QStringLiteral("night") ? QStringLiteral(":/theme/night.qss")
                                         : QStringLiteral(":/theme/light.qss"));
    return base + platformOverlay(theme) + appleFontOverlay();
}

std::pair<QString, QString> tooltipPaletteColors(const QString &theme)
{
    if (theme == QStringLiteral("night")) {
        return {QStringLiteral("#2c2c2e"), QStringLiteral("#f5f5f7")};
    }
    return {QStringLiteral("#f2f2f7"), QStringLiteral("#1d1d1f")};
}

} // namespace i2p
