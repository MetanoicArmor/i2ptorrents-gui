#pragma once

#include "config.hpp"
#include "models.hpp"
#include "postman_catalog.hpp"

#include <QTimer>
#include <QWidget>
#include <QVector>

class QPushButton;
class QLabel;
class QToolButton;
class QLineEdit;
class QComboBox;
class QStackedWidget;
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
    void dispatchCreate();
    void dispatchOpen();
    void dispatchSettings();
    void dispatchAbout();
    void dispatchFilterAll();
    void dispatchFilterDownloading();
    void dispatchFilterSeeding();
    void dispatchPostman();
    void pollWorker();
    void onTorrentsReady(QVector<Torrent> torrents, QString error);
    void onAddedReady(std::optional<QString> savedPath, QString error, bool magnet = false);
    void onRemovedReady(QString error);
    void onStartStopReady(QString error, bool start);

private:
    void applyChrome();
    void setStatus();
    void setFilter(const QString &name);
    void showCatalog();
    void buildCatalogPage();
    void fillCatalogCombos();
    void spawnCatalog();
    void renderCatalog();
    QWidget *makeCatalogCard(const PostmanTorrent &item);
    void onCatalogReady(PostmanPage page, QString error);
    void addCatalogItem(const PostmanTorrent &item);
    void showAddingNotice(const QString &name);
    void startAddCatalogFile(qint64 id, const QString &name, bool rpcOnline);
    void renderCards();
    QWidget *makeCard(const Torrent &torrent);
    void spawnRefresh();
    void openTorrentFile();
    void openMagnetDialog();
    void openCreateTorrentDialog();
    void startAdd(const QString &source);
    void startAddMagnet(const QString &magnet);
    void startAddMetainfo(const QByteArray &content, const QString &preferredName);
    void showActions(const Torrent &torrent, quintptr morePtr);
    void showFiles(qint64 torrentId, const QString &name);
    void showPeers(qint64 torrentId, const QString &name);
    void showTrackers(qint64 torrentId, const QString &name);
    void confirmRemoveTorrent(qint64 torrentId, const QString &name);
    void startStopTorrent(qint64 torrentId, bool start);
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
    bool catalogMode_ = false;
    bool catalogBusy_ = false;
    bool catalogLoaded_ = false;
    qint64 pendingCatalogId_ = 0;
    QString pendingCatalogName_;
    PostmanCatalog catalog_;
    PostmanQuery catalogQuery_;
    PostmanPage catalogPageData_;
    QString catalogError_;

    QWidget *sidebar_ = nullptr;
    QWidget *surface_ = nullptr;
    QStackedWidget *stack_ = nullptr;
    QWidget *scrollHost_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QLabel *summaryLabel_ = nullptr;
    QLabel *subtitleLabel_ = nullptr;
    QLabel *sectionLabel_ = nullptr;
    QLabel *trackerSectionLabel_ = nullptr;
    QPushButton *addButton_ = nullptr;
    QPushButton *createButton_ = nullptr;
    QPushButton *settingsButton_ = nullptr;
    QPushButton *aboutButton_ = nullptr;
    QToolButton *refreshButton_ = nullptr;
    QLineEdit *searchEdit_ = nullptr;
    QPushButton *postmanButton_ = nullptr;
    QPushButton *filterButtons_[3] = {};
    quintptr scrollPtr_ = 0;
    QLineEdit *catalogSearch_ = nullptr;
    QComboBox *catalogCategory_ = nullptr;
    QComboBox *catalogOrder_ = nullptr;
    QToolButton *catalogRefresh_ = nullptr;
    QLabel *catalogStatusLabel_ = nullptr;
    QLabel *catalogPageLabel_ = nullptr;
    QPushButton *catalogPrevButton_ = nullptr;
    QPushButton *catalogNextButton_ = nullptr;
    quintptr catalogScrollPtr_ = 0;

    QTimer refreshTimer_;
    QTimer pollTimer_;
};

} // namespace i2p