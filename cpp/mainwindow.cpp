#include "mainwindow.hpp"

#include "app_constants.hpp"
#include "chrome.hpp"
#include "config.hpp"
#include "i18n.hpp"
#include "rpc.hpp"
#include "theme.hpp"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>

namespace i2p {

namespace {

constexpr int ARROW_CURSOR = 0;
constexpr int WHATS_THIS_CURSOR = 4;

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , settings_(AppSettings::load())
{
    setObjectName(QStringLiteral("MainWindow"));
    const auto [width, height] = settings_.windowSize();
    resize(width, height);
    setMinimumSize(MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT);

    setLanguage(settings_.language);
    setStyleSheet(stylesheet(settings_.theme));
    installRoundedTooltips();
    applyTooltipPalette(settings_.theme);

    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    sidebar_ = new QWidget(this);
    sidebar_->setObjectName(QStringLiteral("Sidebar"));
    sidebar_->setFixedWidth(220);
    auto *side = new QVBoxLayout(sidebar_);
    side->setContentsMargins(12, 0, 12, 12);
#if defined(Q_OS_MACOS)
    side->setContentsMargins(12, 40, 12, 12);
#else
    side->setContentsMargins(12, 14, 12, 12);
#endif
    side->setSpacing(2);

    auto *title = new QLabel(APP_NAME, sidebar_);
    title->setObjectName(QStringLiteral("AppTitle"));
    side->addWidget(title);
    subtitleLabel_ = new QLabel(trKey(QStringLiteral("subtitle")), sidebar_);
    subtitleLabel_->setObjectName(QStringLiteral("AppSubtitle"));
    side->addWidget(subtitleLabel_);
    side->addSpacing(18);
    sectionLabel_ = new QLabel(trKey(QStringLiteral("section_torrents")), sidebar_);
    sectionLabel_->setObjectName(QStringLiteral("SectionTitle"));
    side->addWidget(sectionLabel_);

    const struct {
        const char *key;
        const char *label;
        QPushButton **slot;
    } filters[] = {
        {"all", "filter_all", &filterButtons_[0]},
        {"downloading", "filter_downloading", &filterButtons_[1]},
        {"seeding", "filter_seeding", &filterButtons_[2]},
    };
    for (const auto &filter : filters) {
        auto *button = new QPushButton(trKey(QString::fromUtf8(filter.label)), sidebar_);
        button->setObjectName(QStringLiteral("Filter"));
        setCheckable(button, true);
        setChecked(reinterpret_cast<quintptr>(button),
                   QString::fromUtf8(filter.key) == QStringLiteral("all"));
        *filter.slot = button;
        side->addWidget(button);
    }
    side->addStretch();
    aboutButton_ = new QPushButton(trKey(QStringLiteral("about")), sidebar_);
    aboutButton_->setObjectName(QStringLiteral("AboutButton"));
    side->addWidget(aboutButton_);
    settingsButton_ = new QPushButton(trKey(QStringLiteral("settings")), sidebar_);
    settingsButton_->setObjectName(QStringLiteral("SettingsButton"));
    side->addWidget(settingsButton_);
    outer->addWidget(sidebar_);

    auto *split = new QWidget(this);
    split->setObjectName(QStringLiteral("PaneSplit"));
    split->setFixedWidth(1);
    outer->addWidget(split);

    surface_ = new QWidget(this);
    surface_->setObjectName(QStringLiteral("Surface"));
    auto *body = new QVBoxLayout(surface_);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    auto *head = new QWidget(surface_);
    auto *headLayout = new QVBoxLayout(head);
    headLayout->setContentsMargins(18, 0, 18, 0);
#if defined(Q_OS_MACOS)
    headLayout->setContentsMargins(18, 14, 18, 0);
#else
    headLayout->setContentsMargins(18, 16, 18, 0);
#endif
    headLayout->setSpacing(12);

    auto *top = new QWidget(head);
    auto *topLayout = new QHBoxLayout(top);
    topLayout->setContentsMargins(0, 0, 0, 0);
    statusLabel_ = new QLabel(trKey(QStringLiteral("connecting")), top);
    statusLabel_->setObjectName(QStringLiteral("StatusOffline"));
    statusLabel_->setCursor(Qt::WhatsThisCursor);
    topLayout->addWidget(statusLabel_);
    topLayout->addStretch();
    refreshButton_ = new QToolButton(top);
    refreshButton_->setObjectName(QStringLiteral("RefreshButton"));
    refreshButton_->setText(QStringLiteral("↻"));
    topLayout->addWidget(refreshButton_);
    addButton_ = new QPushButton(trKey(QStringLiteral("add_torrent")), top);
    addButton_->setObjectName(QStringLiteral("Primary"));
    topLayout->addWidget(addButton_);
    headLayout->addWidget(top);

    searchEdit_ = new QLineEdit(head);
    searchEdit_->setObjectName(QStringLiteral("Search"));
    headLayout->addWidget(searchEdit_);
    summaryLabel_ = new QLabel(head);
    summaryLabel_->setObjectName(QStringLiteral("Secondary"));
    headLayout->addWidget(summaryLabel_);
    body->addWidget(head);

    NativeWidget overlay = NativeWidget::overlayScroll();
    scrollHost_ = overlay.widget();
    scrollHost_->setObjectName(QStringLiteral("TorrentScroll"));
    scrollPtr_ = reinterpret_cast<quintptr>(scrollHost_);
    body->addWidget(scrollHost_, 1);
    overlay.releaseOwnership(); // child of surface_

    outer->addWidget(surface_, 1);

    connect(filterButtons_[0], &QPushButton::clicked, this, &MainWindow::dispatchFilterAll);
    connect(filterButtons_[1], &QPushButton::clicked, this, &MainWindow::dispatchFilterDownloading);
    connect(filterButtons_[2], &QPushButton::clicked, this, &MainWindow::dispatchFilterSeeding);
    connect(aboutButton_, &QPushButton::clicked, this, &MainWindow::dispatchAbout);
    connect(settingsButton_, &QPushButton::clicked, this, &MainWindow::dispatchSettings);
    connect(refreshButton_, &QToolButton::clicked, this, &MainWindow::dispatchRefresh);
    connect(addButton_, &QPushButton::clicked, this, &MainWindow::dispatchAdd);

    addShortcut(this, QStringLiteral("Ctrl+T"), [this] { dispatchAdd(); });
    addShortcut(this, QStringLiteral("Ctrl+O"), [this] { dispatchOpen(); });
    addShortcut(this, QStringLiteral("Ctrl+S"), [this] { dispatchSettings(); });
    addShortcut(this, QStringLiteral("Ctrl+,"), [this] { dispatchSettings(); });

    connect(&refreshTimer_, &QTimer::timeout, this, &MainWindow::dispatchRefresh);
    connect(&pollTimer_, &QTimer::timeout, this, &MainWindow::pollWorker);
    refreshTimer_.start(static_cast<int>(std::max(settings_.refreshSeconds, quint32(2)) * 1000));
    pollTimer_.start(80);

    setWindowTitle(QStringLiteral("%1 %2").arg(APP_NAME, appVersion()));
    applyChrome();
    show();
    applyWindowMaterial(this, settings_.theme);
    spawnRefresh();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    settings_.captureWindowSize(width(), height());
    settings_.save();
    QWidget::closeEvent(event);
}

QString MainWindow::nativeShortcut(const QString &sequence) const
{
#if defined(Q_OS_MACOS)
    return QString(sequence).replace(QStringLiteral("Ctrl+"), QStringLiteral("⌘"));
#else
    return sequence;
#endif
}

QString MainWindow::tipWithShortcuts(const QString &labelKey, const QStringList &sequences) const
{
    QStringList shortcuts;
    for (const QString &sequence : sequences) {
        shortcuts << nativeShortcut(sequence);
    }
    return trArgs(labelKey, {{QStringLiteral("shortcut"), shortcuts.join(QStringLiteral(" / "))}});
}

void MainWindow::applyChrome()
{
    setWindowTitle(QStringLiteral("%1 %2").arg(APP_NAME, appVersion()));
    applyAppFont();
    setStyleSheet(stylesheet(settings_.theme));
    applyWindowMaterial(this, settings_.theme);
    applyTooltipPalette(settings_.theme);
    overlayApplyTheme(scrollPtr_, settings_.theme);
    setLabelText(reinterpret_cast<quintptr>(subtitleLabel_), trKey(QStringLiteral("subtitle")));
    setLabelText(reinterpret_cast<quintptr>(sectionLabel_), trKey(QStringLiteral("section_torrents")));
    setButtonText(reinterpret_cast<quintptr>(filterButtons_[0]), trKey(QStringLiteral("filter_all")));
    setButtonText(reinterpret_cast<quintptr>(filterButtons_[1]), trKey(QStringLiteral("filter_downloading")));
    setButtonText(reinterpret_cast<quintptr>(filterButtons_[2]), trKey(QStringLiteral("filter_seeding")));
    setStatus();
    setButtonText(reinterpret_cast<quintptr>(addButton_), trKey(QStringLiteral("add_torrent")));
    addButton_->setToolTip(tipWithShortcuts(QStringLiteral("add_torrent_tip"),
                                            {QStringLiteral("Ctrl+T"), QStringLiteral("Ctrl+O")}));
    setButtonText(reinterpret_cast<quintptr>(settingsButton_), trKey(QStringLiteral("settings")));
    settingsButton_->setToolTip(tipWithShortcuts(QStringLiteral("settings_tip"),
                                                 {QStringLiteral("Ctrl+,"), QStringLiteral("Ctrl+S")}));
    setButtonText(reinterpret_cast<quintptr>(aboutButton_), trKey(QStringLiteral("about")));
    setPlaceholderPtr(reinterpret_cast<quintptr>(searchEdit_), trKey(QStringLiteral("search_placeholder")));
    refreshButton_->setToolTip(trKey(QStringLiteral("refresh")));
}

void MainWindow::setStatus()
{
    QString text;
    QString objectName;
    QString tip;
    int cursor = ARROW_CURSOR;

    if (statusMode_ == QStringLiteral("online")) {
        text = trKey(QStringLiteral("rpc_online"));
        objectName = QStringLiteral("StatusOnline");
    } else if (statusMode_ == QStringLiteral("updating")) {
        text = trKey(QStringLiteral("updating"));
        objectName = QStringLiteral("StatusOffline");
    } else if (statusMode_ == QStringLiteral("copying")) {
        text = trKey(QStringLiteral("copying_torrent"));
        objectName = QStringLiteral("StatusOffline");
    } else if (statusMode_ == QStringLiteral("offline")) {
        text = trKey(QStringLiteral("rpc_offline"));
        objectName = QStringLiteral("StatusOffline");
        tip = trKey(QStringLiteral("rpc_setup_tip"));
        if (!statusDetail_.isEmpty()) {
            tip = statusDetail_ + QStringLiteral("\n\n") + tip;
        }
        cursor = WHATS_THIS_CURSOR;
    } else {
        text = trKey(QStringLiteral("connecting"));
        objectName = QStringLiteral("StatusOffline");
        tip = trKey(QStringLiteral("rpc_setup_tip"));
        cursor = WHATS_THIS_CURSOR;
    }

    setLabelText(reinterpret_cast<quintptr>(statusLabel_), text);
    setObjectNamePtr(reinterpret_cast<quintptr>(statusLabel_), objectName);
    setCursorPtr(reinterpret_cast<quintptr>(statusLabel_), cursor);
    statusLabel_->setToolTip(tip);

    quint64 down = 0;
    quint64 up = 0;
    for (const Torrent &torrent : torrents_) {
        down += torrent.rateDownload;
        up += torrent.rateUpload;
    }
    setLabelText(reinterpret_cast<quintptr>(summaryLabel_),
                 trArgs(QStringLiteral("summary"),
                        {{QStringLiteral("count"), QString::number(torrents_.size())},
                         {QStringLiteral("down"), formatRate(down)},
                         {QStringLiteral("up"), formatRate(up)}}));
}

void MainWindow::setFilter(const QString &name)
{
    filter_ = name;
    const QStringList keys = {QStringLiteral("all"), QStringLiteral("downloading"), QStringLiteral("seeding")};
    for (int index = 0; index < 3; ++index) {
        setChecked(reinterpret_cast<quintptr>(filterButtons_[index]), keys[index] == filter_);
    }
    renderCards();
}

QVector<Torrent> MainWindow::visibleTorrents() const
{
    const QString query = lineEditText(reinterpret_cast<quintptr>(searchEdit_)).trimmed().toLower();
    QVector<Torrent> rows;
    for (const Torrent &item : torrents_) {
        if (filter_ == QStringLiteral("downloading") && item.status != TorrentStatus::Downloading) {
            continue;
        }
        if (filter_ == QStringLiteral("seeding") && item.status != TorrentStatus::Seeding) {
            continue;
        }
        if (!query.isEmpty() && !item.name.toLower().contains(query) &&
            !item.hashString.toLower().contains(query)) {
            continue;
        }
        rows.push_back(item);
    }
    std::sort(rows.begin(), rows.end(), [](const Torrent &a, const Torrent &b) {
        const bool aDown = a.status == TorrentStatus::Downloading;
        const bool bDown = b.status == TorrentStatus::Downloading;
        if (aDown != bDown) {
            return aDown > bDown;
        }
        return a.name.toLower() < b.name.toLower();
    });
    return rows;
}

void MainWindow::renderCards()
{
    setStatus();
    auto *cards = new QWidget;
    auto *layout = new QVBoxLayout(cards);
    layout->setContentsMargins(18, 12, 14, 16);
    layout->setSpacing(12);

    const QVector<Torrent> rows = visibleTorrents();
    const bool detailed = settings_.torrentView != QStringLiteral("simple");
    if (rows.isEmpty()) {
        auto *empty = new QLabel(trArgs(QStringLiteral("empty_list"),
                                        {{QStringLiteral("path"), settings_.torrentsDir}}),
                                 cards);
        empty->setObjectName(QStringLiteral("Secondary"));
        layout->addWidget(empty);
    } else {
        for (const Torrent &torrent : rows) {
            layout->addWidget(makeCard(torrent));
        }
    }
    layout->addStretch();
    overlaySetWidget(scrollPtr_, cards);
}

QWidget *MainWindow::makeCard(const Torrent &torrent)
{
    NativeWidget card = NativeWidget::torrentCard(settings_.theme);
    QWidget *cardWidget = card.widget();
    cardWidget->setToolTip(trKey(QStringLiteral("files_tooltip")));

    const qint64 torrentId = torrent.id;
    const QString torrentName = torrent.name;
    const QString theme = settings_.theme;
    const QString rpcUrl = settings_.rpcUrl;
    onClick(cardWidget, [this, torrentId, torrentName, theme, rpcUrl]() {
        defer([this, torrentId, torrentName, theme, rpcUrl]() {
            showFiles(torrentId, torrentName);
        });
    });

    auto *root = new QVBoxLayout(cardWidget);
    root->setContentsMargins(14, 11, 14, 11);
    root->setSpacing(6);

    auto *titleRow = new QWidget(cardWidget);
    auto *titles = new QHBoxLayout(titleRow);
    titles->setContentsMargins(0, 0, 0, 0);
    titles->setSpacing(8);
    auto *name = new QLabel(torrent.name, titleRow);
    name->setObjectName(QStringLiteral("TorrentName"));
    titles->addWidget(name, 1);
    auto *status = new QLabel(torrent.statusLabel(), titleRow);
    status->setObjectName(QStringLiteral("StatusText"));
    titles->addWidget(status);
    auto *more = new QToolButton(titleRow);
    more->setObjectName(QStringLiteral("MoreButton"));
    more->setText(QStringLiteral("•••"));
    more->setToolTip(trKey(QStringLiteral("actions")));
    titles->addWidget(more);
    root->addWidget(titleRow);

    auto *progress = new QProgressBar(cardWidget);
    progress->setRange(0, 1000);
    progress->setValue(static_cast<int>(std::round(torrent.progress() * 1000.0)));
    progress->setFormat(QString());
    progress->setToolTip(trKey(QStringLiteral("download_progress")));
    root->addWidget(progress);

    const bool detailed = settings_.torrentView != QStringLiteral("simple");
    if (detailed && !torrent.pieces.isEmpty()) {
        NativeWidget map = NativeWidget::pieceMap(torrent.pieces);
        const int haveN = std::count(torrent.pieces.begin(), torrent.pieces.end(), true);
        map.setTooltip(trArgs(QStringLiteral("pieces_tooltip"),
                              {{QStringLiteral("have"), QString::number(haveN)},
                               {QStringLiteral("total"), QString::number(torrent.pieces.size())}}));
        root->addWidget(map.widget());
        map.releaseOwnership();
    }

    auto *details = new QWidget(cardWidget);
    auto *detailRow = new QHBoxLayout(details);
    detailRow->setContentsMargins(0, 0, 0, 0);
    detailRow->setSpacing(8);
    auto *percent = new QLabel(trArgs(QStringLiteral("progress_percent"),
                                      {{QStringLiteral("percent"),
                                        QString::number(torrent.progress() * 100.0, 'f', 1)}}),
                               details);
    percent->setObjectName(QStringLiteral("Secondary"));
    detailRow->addWidget(percent);
    detailRow->addStretch();
    auto *size = new QLabel(trArgs(QStringLiteral("progress_size"),
                                   {{QStringLiteral("done"), formatBytes(torrent.completed())},
                                    {QStringLiteral("total"), formatBytes(torrent.totalSize)}}),
                            details);
    size->setObjectName(QStringLiteral("Secondary"));
    detailRow->addWidget(size);
    detailRow->addStretch();
    auto *rates = new QLabel(QStringLiteral("↓ %1   ↑ %2")
                                 .arg(formatRate(torrent.rateDownload), formatRate(torrent.rateUpload)),
                             details);
    rates->setObjectName(QStringLiteral("Secondary"));
    detailRow->addWidget(rates);
    detailRow->addStretch();
    auto *peers = new QLabel(trArgs(QStringLiteral("peers"),
                                    {{QStringLiteral("down"), QString::number(torrent.peersSendingToUs)},
                                     {QStringLiteral("up"), QString::number(torrent.peersGettingFromUs)}}),
                             details);
    peers->setObjectName(QStringLiteral("PeersLink"));
    peers->setToolTip(trKey(QStringLiteral("peers_tooltip")));
    setCursorPtr(reinterpret_cast<quintptr>(peers), static_cast<int>(Qt::PointingHandCursor));
    onClick(peers, [this, torrentId, torrentName]() {
        defer([this, torrentId, torrentName]() { showPeers(torrentId, torrentName); });
    });
    detailRow->addWidget(peers);
    root->addWidget(details);

    if (detailed) {
        QStringList meta;
        if (!torrent.hashString.isEmpty()) {
            meta << torrent.shortHash();
        }
        if (torrent.pieceCount > 0) {
            meta << trArgs(QStringLiteral("pieces_meta"),
                           {{QStringLiteral("count"), QString::number(torrent.pieceCount)},
                            {QStringLiteral("size"), formatBytes(torrent.pieceSize)}});
        }
        if (!meta.isEmpty()) {
            auto *info = new QLabel(meta.join(QStringLiteral("  ·  ")), cardWidget);
            info->setObjectName(QStringLiteral("Secondary"));
            root->addWidget(info);
        }
    }

    connect(more, &QToolButton::clicked, this, [this, torrent, more]() {
        showActions(torrent, reinterpret_cast<quintptr>(more));
    });

    card.releaseOwnership();
    return cardWidget;
}

void MainWindow::showActions(const Torrent &torrent, quintptr morePtr)
{
    showPopupBelow(this, morePtr, settings_.theme, [this, torrent](void *popup) {
        popupAddAction(popup, trKey(QStringLiteral("files_show")), true, [this, torrent]() {
            defer([this, torrent]() { showFiles(torrent.id, torrent.name); });
        });
        popupAddAction(popup, trKey(QStringLiteral("peers_show")), true, [this, torrent]() {
            defer([this, torrent]() { showPeers(torrent.id, torrent.name); });
        });
        popupAddAction(popup,
                         trKey(QStringLiteral("copy_hash")),
                         !torrent.hashString.isEmpty(),
                         [hash = torrent.hashString]() {
                             QApplication::clipboard()->setText(hash.toLower());
                         });
        popupAddAction(popup, trKey(QStringLiteral("open_folder")), true, [this, torrent]() {
            openFolder(settings_.torrentsDir, torrent.name);
        });
        popupSeparator(popup);
        popupAddAction(popup, trKey(QStringLiteral("remove_from_list")), true, [this, torrent]() {
            confirmRemoveTorrent(torrent.id, torrent.name);
        });
    });
}

void MainWindow::showFiles(qint64 torrentId, const QString &name)
{
    QString error;
    std::optional<QString> endpoint = normalizeRpcUrl(settings_.rpcUrl, &error);
    QVector<TorrentFile> files;
    if (endpoint.has_value()) {
        RpcClient client(*endpoint);
        files = client.getTorrentFiles(torrentId, &error);
    }
    const QString stylesheetText = stylesheet(settings_.theme);
    filesExec(this,
              stylesheetText,
              name,
              files,
              [this, torrentId, rpcUrl = settings_.rpcUrl](int index, int wanted, int priority) {
                  QString err;
                  const std::optional<QString> endpoint = normalizeRpcUrl(rpcUrl, &err);
                  if (!endpoint.has_value()) {
                      return -1;
                  }
                  RpcClient client(*endpoint);
                  if (!client.setFilePriority(torrentId, index, wanted != 0, priority, &err)) {
                      return rpcMethodUnsupported(err) ? 1 : -1;
                  }
                  return 0;
              });
}

void MainWindow::showPeers(qint64 torrentId, const QString &name)
{
    QString error;
    std::optional<QString> endpoint = normalizeRpcUrl(settings_.rpcUrl, &error);
    QVector<Peer> peers;
    if (endpoint.has_value()) {
        RpcClient client(*endpoint);
        peers = client.getTorrentPeers(torrentId, &error);
    }
    const QString title = trArgs(QStringLiteral("peers_title"), {{QStringLiteral("name"), name}});
    if (!error.isEmpty() && peers.isEmpty()) {
        QMessageBox::warning(this, title, error);
        return;
    }
    peersExec(this, stylesheet(settings_.theme), title, peers);
}

void MainWindow::confirmRemoveTorrent(qint64 torrentId, const QString &name)
{
    const std::optional<bool> deleteData = confirmRemove(this,
                                                         trKey(QStringLiteral("remove_title")),
                                                         trArgs(QStringLiteral("remove_text"),
                                                                {{QStringLiteral("name"), name}}),
                                                         trKey(QStringLiteral("remove_data")),
                                                         trKey(QStringLiteral("yes")),
                                                         trKey(QStringLiteral("cancel")));
    if (!deleteData.has_value()) {
        return;
    }
    const QString rpcUrl = settings_.rpcUrl;
    QThread *thread = QThread::create([this, torrentId, rpcUrl, deleteData = *deleteData]() {
        QString error;
        std::optional<QString> endpoint = normalizeRpcUrl(rpcUrl, &error);
        if (!endpoint.has_value()) {
            QMetaObject::invokeMethod(this, [this, error]() { onRemovedReady(error); }, Qt::QueuedConnection);
            return;
        }
        RpcClient client(*endpoint);
        if (!client.removeTorrent(torrentId, deleteData, &error)) {
            QMetaObject::invokeMethod(this, [this, error]() { onRemovedReady(error); }, Qt::QueuedConnection);
            return;
        }
        QMetaObject::invokeMethod(this, [this]() { onRemovedReady({}); }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void MainWindow::openFolder(const QString &root, const QString &name)
{
    QFileInfo target(root + QLatin1Char('/') + name);
    if (!target.exists()) {
        const QFileInfo part(root + QLatin1Char('/') + name + QStringLiteral(".part"));
        target = part.exists() ? part : QFileInfo(root);
    }
    QUrl url = QUrl::fromLocalFile(target.isFile() ? target.absolutePath() : target.absoluteFilePath());
    QDesktopServices::openUrl(url);
}

void MainWindow::spawnRefresh()
{
    if (busy_ || adding_) {
        return;
    }
    busy_ = true;
    statusMode_ = QStringLiteral("updating");
    setStatus();

    const QString endpoint = settings_.rpcUrl;
    const bool detailed = settings_.torrentView != QStringLiteral("simple");
    QThread *thread = QThread::create([this, endpoint, detailed]() {
        QString error;
        std::optional<QString> normalized = normalizeRpcUrl(endpoint, &error);
        QVector<Torrent> rows;
        if (normalized.has_value()) {
            RpcClient client(*normalized);
            rows = client.getTorrents(detailed, &error);
        }
        QMetaObject::invokeMethod(this,
                                  [this, rows, error]() { onTorrentsReady(rows, error); },
                                  Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void MainWindow::openTorrentFile()
{
    if (adding_) {
        return;
    }
    const QString filter =
        QStringLiteral("%1 (*.torrent);;%2 (*)").arg(trKey(QStringLiteral("torrent_files")),
                                                     trKey(QStringLiteral("all_files")));
    const std::optional<QString> path =
        openFile(this, trKey(QStringLiteral("add_title")), filter);
    if (!path.has_value()) {
        return;
    }
    startAdd(*path);
}

void MainWindow::startAdd(const QString &source)
{
    const QString trimmed = source.trimmed();
    if (trimmed.isEmpty() || !QFileInfo(trimmed).isFile()) {
        QMessageBox::warning(this,
                             trKey(QStringLiteral("file_not_found")),
                             trKey(QStringLiteral("file_not_found_text")));
        return;
    }
    const bool rpcOnline = statusMode_ == QStringLiteral("online");
    adding_ = true;
    statusMode_ = QStringLiteral("copying");
    setStatus();

    AppSettings settings = settings_;
    QThread *thread = QThread::create([this, trimmed, settings, rpcOnline]() {
        QString error;
        std::optional<QString> saved;
        QFile file(trimmed);
        if (!file.open(QIODevice::ReadOnly)) {
            error = file.errorString();
        } else {
            const QByteArray content = file.readAll();
            if (content.isEmpty()) {
                error = trKey(QStringLiteral("rpc_empty_file"));
            } else if (rpcOnline) {
                std::optional<QString> endpoint = normalizeRpcUrl(settings.rpcUrl, &error);
                if (endpoint.has_value()) {
                    RpcClient client(*endpoint);
                    if (client.addTorrentBytes(content, &error).isEmpty() && !error.isEmpty()) {
                        // keep error
                    }
                }
            } else {
                QString dirError;
                const QString destRoot = settings.torrentsPath(&dirError);
                if (destRoot.isEmpty()) {
                    error = dirError;
                } else {
                    const QString filename = QFileInfo(trimmed).fileName().isEmpty()
                                                 ? QStringLiteral("download.torrent")
                                                 : QFileInfo(trimmed).fileName();
                    const QString dest = destRoot + QLatin1Char('/') + filename;
                    if (QFile destFile(dest); destFile.open(QIODevice::WriteOnly)) {
                        destFile.write(content);
                        saved = dest;
                    } else {
                        error = destFile.errorString();
                    }
                }
            }
        }
        QMetaObject::invokeMethod(this,
                                  [this, saved, error]() { onAddedReady(saved, error); },
                                  Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void MainWindow::openAboutDialog()
{
    defer([this]() { aboutExec(this, stylesheet(settings_.theme)); });
}

void MainWindow::openSettingsDialog()
{
    defer([this]() {
        const QString previous = language();
        const AppSettings current = settings_;
        const std::optional<SettingsResult> result =
            settingsExec(this, stylesheet(current.theme), current);
        if (!result.has_value()) {
            setLanguage(previous);
            return;
        }
        QString error;
        if (!normalizeRpcUrl(result->rpcUrl, &error)) {
            QMessageBox::warning(this, trKey(QStringLiteral("invalid_address")), error);
            setLanguage(previous);
            return;
        }
        if (result->torrentsDir.trimmed().isEmpty()) {
            QMessageBox::warning(this,
                                 trKey(QStringLiteral("invalid_directory")),
                                 trKey(QStringLiteral("need_torrents_dir")));
            setLanguage(previous);
            return;
        }

        settings_.rpcUrl = result->rpcUrl;
        settings_.torrentsDir = result->torrentsDir;
        settings_.refreshSeconds = result->refreshSeconds;
        settings_.language = result->language;
        settings_.theme = result->theme;
        settings_.torrentView = result->torrentView.isEmpty() ? QStringLiteral("detailed") : result->torrentView;
        if (settings_.theme != QStringLiteral("night")) {
            settings_.theme = QStringLiteral("light");
        }
        if (settings_.language != QStringLiteral("ru")) {
            settings_.language = QStringLiteral("en");
        }
        setLanguage(settings_.language);
        QString dirError;
        if (settings_.torrentsPath(&dirError).isEmpty()) {
            QMessageBox::warning(this,
                                 trKey(QStringLiteral("torrents_directory")),
                                 trArgs(QStringLiteral("torrents_dir_error"),
                                        {{QStringLiteral("error"), dirError}}));
        }
        settings_.captureWindowSize(width(), height());
        settings_.save();
        refreshTimer_.start(static_cast<int>(std::max(settings_.refreshSeconds, quint32(2)) * 1000));
        applyChrome();
        renderCards();
        spawnRefresh();
    });
}

void MainWindow::dispatchRefresh()
{
    spawnRefresh();
}

void MainWindow::dispatchAdd()
{
    openTorrentFile();
}

void MainWindow::dispatchOpen()
{
    openTorrentFile();
}

void MainWindow::dispatchSettings()
{
    openSettingsDialog();
}

void MainWindow::dispatchAbout()
{
    openAboutDialog();
}

void MainWindow::dispatchFilterAll()
{
    setFilter(QStringLiteral("all"));
}

void MainWindow::dispatchFilterDownloading()
{
    setFilter(QStringLiteral("downloading"));
}

void MainWindow::dispatchFilterSeeding()
{
    setFilter(QStringLiteral("seeding"));
}

void MainWindow::pollWorker()
{
    const QString query = lineEditText(reinterpret_cast<quintptr>(searchEdit_));
    if (query != searchCache_) {
        searchCache_ = query;
        renderCards();
    }
}

void MainWindow::onTorrentsReady(QVector<Torrent> torrents, QString error)
{
    busy_ = false;
    if (error.isEmpty()) {
        torrents_ = std::move(torrents);
        statusMode_ = QStringLiteral("online");
        statusDetail_.clear();
    } else {
        statusMode_ = QStringLiteral("offline");
        statusDetail_ = error;
        setLabelText(reinterpret_cast<quintptr>(summaryLabel_), error);
    }
    applyChrome();
    renderCards();
}

void MainWindow::onAddedReady(std::optional<QString> savedPath, QString error)
{
    adding_ = false;
    if (error.isEmpty()) {
        if (savedPath.has_value()) {
            QMessageBox::information(this,
                                     trKey(QStringLiteral("added_title")),
                                     trArgs(QStringLiteral("added_body"),
                                            {{QStringLiteral("path"), *savedPath}}));
        }
        spawnRefresh();
        return;
    }
    QMessageBox::warning(this, trKey(QStringLiteral("add_failed")), error);
    spawnRefresh();
}

void MainWindow::onRemovedReady(QString error)
{
    if (!error.isEmpty()) {
        QMessageBox::warning(this, trKey(QStringLiteral("remove_title")), error);
        return;
    }
    spawnRefresh();
}

} // namespace i2p
