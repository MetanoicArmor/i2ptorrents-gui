#pragma once

#include "config.hpp"
#include "models.hpp"

#include <QTimer>
#include <QWidget>
#include <QVector>

class QPushButton;
class QLabel;
class QToolButton;
class QLineEdit;
class QVBoxLayout;

namespace i2p {

class MainWindow final : public QWidget {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void dispatchRefresh();
    void dispatchAdd();
    void dispatchOpen();
    void dispatchSettings();
    void dispatchAbout();
    void dispatchFilterAll();
    void dispatchFilterDownloading();
    void dispatchFilterSeeding();
    void pollWorker();
    void onTorrentsReady(QVector<Torrent> torrents, QString error);
    void onAddedReady(std::optional<QString> savedPath, QString error);
    void onRemovedReady(QString error);

private:
    void applyChrome();
    void setStatus();
    void setFilter(const QString &name);
    void renderCards();
    QWidget *makeCard(const Torrent &torrent);
    void spawnRefresh();
    void openTorrentFile();
    void startAdd(const QString &source);
    void showActions(const Torrent &torrent, quintptr morePtr);
    void showFiles(qint64 torrentId, const QString &name);
    void confirmRemoveTorrent(qint64 torrentId, const QString &name);
    void openFolder(const QString &root, const QString &name);
    void openSettingsDialog();
    void openAboutDialog();
    QString tipWithShortcuts(const QString &labelKey, const QStringList &sequences) const;
    QString nativeShortcut(const QString &sequence) const;
    QVector<Torrent> visibleTorrents() const;

    AppSettings settings_;
    QVector<Torrent> torrents_;
    QString filter_ = QStringLiteral("all");
    bool busy_ = false;
    bool adding_ = false;
    QString searchCache_;
    QString statusMode_ = QStringLiteral("connecting");
    QString statusDetail_;

    QWidget *sidebar_ = nullptr;
    QWidget *surface_ = nullptr;
    QWidget *scrollHost_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QLabel *summaryLabel_ = nullptr;
    QLabel *subtitleLabel_ = nullptr;
    QLabel *sectionLabel_ = nullptr;
    QPushButton *addButton_ = nullptr;
    QPushButton *settingsButton_ = nullptr;
    QPushButton *aboutButton_ = nullptr;
    QToolButton *refreshButton_ = nullptr;
    QLineEdit *searchEdit_ = nullptr;
    QPushButton *filterButtons_[3] = {};
    quintptr scrollPtr_ = 0;

    QTimer refreshTimer_;
    QTimer pollTimer_;
};

} // namespace i2p