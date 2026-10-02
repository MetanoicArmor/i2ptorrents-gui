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
#include <QComboBox>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QThread>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>

namespace i2p {

namespace {

constexpr int ARROW_CURSOR = 0;
constexpr int WHATS_THIS_CURSOR = 4;

PostmanProxy catalogProxy(const AppSettings &settings)
{
    PostmanProxy proxy;
    proxy.socks = normalizeCatalogProxy(settings.catalogProxy) == QLatin1String("socks5");
    proxy.port = settings.httpProxyPort == 0 ? POSTMAN_PROXY_PORT : static_cast<int>(settings.httpProxyPort);
    return proxy;
}

QString torrentFileName(const QString &name)
{
    QString file = name.trimmed();
    if (file.isEmpty()) {
        return QStringLiteral("download.torrent");
    }
    for (QChar &ch : file) {
        switch (ch.unicode()) {
        case '/':
        case '\\':
        case ':':
        case '*':
        case '?':
        case '"':
        case '<':
        case '>':
        case '|':
            ch = QLatin1Char('_');
            break;
        default:
            break;
        }
    }
    if (!file.endsWith(QStringLiteral(".torrent"), Qt::CaseInsensitive)) {
        file += QStringLiteral(".torrent");
    }
    return file;
}

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
    side->addSpacing(14);
    trackerSectionLabel_ = new QLabel(trKey(QStringLiteral("section_tracker")), sidebar_);
    trackerSectionLabel_->setObjectName(QStringLiteral("SectionTitle"));
    side->addWidget(trackerSectionLabel_);
    postmanButton_ = new QPushButton(trKey(QStringLiteral("postman")), sidebar_);
    postmanButton_->setObjectName(QStringLiteral("Filter"));
    setCheckable(postmanButton_, true);
    side->addWidget(postmanButton_);
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

    stack_ = new QStackedWidget(surface_);
    stack_->setObjectName(QStringLiteral("SurfaceStack"));
    stack_->setFrameShape(QFrame::NoFrame);
    body->addWidget(stack_);

    auto *libraryPage = new QWidget(stack_);
    libraryPage->setObjectName(QStringLiteral("LibraryPage"));
    auto *libraryLayout = new QVBoxLayout(libraryPage);
    libraryLayout->setContentsMargins(0, 0, 0, 0);
    libraryLayout->setSpacing(0);

    auto *head = new QWidget(libraryPage);
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
    createButton_ = new QPushButton(trKey(QStringLiteral("create_torrent")), top);
    topLayout->addWidget(createButton_);
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
    libraryLayout->addWidget(head);

    NativeWidget overlay = NativeWidget::overlayScroll();
    scrollHost_ = overlay.widget();
    scrollHost_->setObjectName(QStringLiteral("TorrentScroll"));
    scrollPtr_ = reinterpret_cast<quintptr>(scrollHost_);
    libraryLayout->addWidget(scrollHost_, 1);
    overlay.releaseOwnership();
    stack_->addWidget(libraryPage);
    buildCatalogPage();

    outer->addWidget(surface_, 1);

    connect(filterButtons_[0], &QPushButton::clicked, this, &MainWindow::dispatchFilterAll);
    connect(filterButtons_[1], &QPushButton::clicked, this, &MainWindow::dispatchFilterDownloading);
    connect(filterButtons_[2], &QPushButton::clicked, this, &MainWindow::dispatchFilterSeeding);
    connect(postmanButton_, &QPushButton::clicked, this, &MainWindow::dispatchPostman);
    connect(aboutButton_, &QPushButton::clicked, this, &MainWindow::dispatchAbout);
    connect(settingsButton_, &QPushButton::clicked, this, &MainWindow::dispatchSettings);
    connect(refreshButton_, &QToolButton::clicked, this, &MainWindow::dispatchRefresh);
    connect(createButton_, &QPushButton::clicked, this, &MainWindow::dispatchCreate);
    connect(addButton_, &QPushButton::clicked, this, &MainWindow::dispatchAdd);
    connect(catalogSearch_, &QLineEdit::returnPressed, this, [this]() {
        if (catalogBusy_) {
            return;
        }
        catalogQuery_.search = catalogSearch_->text().trimmed();
        catalogQuery_.start = 0;
        spawnCatalog();
    });
    connect(catalogCategory_, &QComboBox::currentIndexChanged, this, [this](int) {
        if (catalogBusy_) {
            return;
        }
        catalogQuery_.category = catalogCategory_->currentData().toInt();
        catalogQuery_.start = 0;
        spawnCatalog();
    });
    connect(catalogOrder_, &QComboBox::currentIndexChanged, this, [this](int) {
        if (catalogBusy_) {
            return;
        }
        catalogQuery_.orderBy = catalogOrder_->currentData().toInt();
        catalogQuery_.start = 0;
        spawnCatalog();
    });
    connect(catalogRefresh_, &QToolButton::clicked, this, [this]() {
        if (!catalogBusy_) {
            spawnCatalog();
        }
    });
    connect(catalogPrevButton_, &QPushButton::clicked, this, [this]() {
        if (catalogPageData_.prevStart < 0) {
            return;
        }
        catalogQuery_.start = catalogPageData_.prevStart;
        spawnCatalog();
    });
    connect(catalogNextButton_, &QPushButton::clicked, this, [this]() {
        if (catalogPageData_.nextStart < 0) {
            return;
        }
        catalogQuery_.start = catalogPageData_.nextStart;
        spawnCatalog();
    });

    addShortcut(this, QStringLiteral("Ctrl+T"), [this] { dispatchAdd(); });
    addShortcut(this, QStringLiteral("Ctrl+O"), [this] { dispatchOpen(); });
    addShortcut(this, QStringLiteral("Ctrl+Shift+T"), [this] { dispatchCreate(); });
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
    if (catalogScrollPtr_ != 0) {
        overlayApplyTheme(catalogScrollPtr_, settings_.theme);
    }
    setLabelText(reinterpret_cast<quintptr>(subtitleLabel_), trKey(QStringLiteral("subtitle")));
    setLabelText(reinterpret_cast<quintptr>(sectionLabel_), trKey(QStringLiteral("section_torrents")));
    if (trackerSectionLabel_ != nullptr) {
        setLabelText(reinterpret_cast<quintptr>(trackerSectionLabel_), trKey(QStringLiteral("section_tracker")));
    }
    if (postmanButton_ != nullptr) {
        setButtonText(reinterpret_cast<quintptr>(postmanButton_), trKey(QStringLiteral("postman")));
    }
    fillCatalogCombos();
    setButtonText(reinterpret_cast<quintptr>(filterButtons_[0]), trKey(QStringLiteral("filter_all")));
    setButtonText(reinterpret_cast<quintptr>(filterButtons_[1]), trKey(QStringLiteral("filter_downloading")));
    setButtonText(reinterpret_cast<quintptr>(filterButtons_[2]), trKey(QStringLiteral("filter_seeding")));
    setStatus();
    setButtonText(reinterpret_cast<quintptr>(createButton_), trKey(QStringLiteral("create_torrent")));
    createButton_->setToolTip(tipWithShortcuts(QStringLiteral("create_torrent_tip"),
                                               {QStringLiteral("Ctrl+Shift+T")}));
    setButtonText(reinterpret_cast<quintptr>(addButton_), trKey(QStringLiteral("add_torrent")));
    addButton_->setToolTip(tipWithShortcuts(QStringLiteral("add_torrent_tip"),
                                            {QStringLiteral("Ctrl+T"), QStringLiteral("Ctrl+O")}));
    setButtonText(reinterpret_cast<quintptr>(settingsButton_), trKey(QStringLiteral("settings")));
    settingsButton_->setToolTip(tipWithShortcuts(QStringLiteral("settings_tip"),
                                                 {QStringLiteral("Ctrl+,"), QStringLiteral("Ctrl+S")}));
    setButtonText(reinterpret_cast<quintptr>(aboutButton_), trKey(QStringLiteral("about")));
    setPlaceholderPtr(reinterpret_cast<quintptr>(searchEdit_), trKey(QStringLiteral("search_placeholder")));
    if (catalogSearch_ != nullptr) {
        setPlaceholderPtr(reinterpret_cast<quintptr>(catalogSearch_),
                          trKey(QStringLiteral("postman_search_placeholder")));
    }
    if (catalogRefresh_ != nullptr) {
        catalogRefresh_->setToolTip(trKey(QStringLiteral("refresh")));
    }
    if (catalogPrevButton_ != nullptr) {
        setButtonText(reinterpret_cast<quintptr>(catalogPrevButton_), trKey(QStringLiteral("postman_prev")));
        setButtonText(reinterpret_cast<quintptr>(catalogNextButton_), trKey(QStringLiteral("postman_next")));
    }
    refreshButton_->setToolTip(trKey(QStringLiteral("refresh")));
    if (catalogMode_) {
        renderCatalog();
    }
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
    catalogMode_ = false;
    if (stack_ != nullptr) {
        stack_->setCurrentIndex(0);
    }
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
    const bool stopped = torrent.status == TorrentStatus::Stopped;
    auto *startBtn = new QToolButton(titleRow);
    startBtn->setObjectName(QStringLiteral("StartStopButton"));
    startBtn->setText(trKey(QStringLiteral("torrent_start")));
    startBtn->setToolTip(trKey(QStringLiteral("torrent_start")));
    startBtn->setEnabled(stopped);
    titles->addWidget(startBtn);
    auto *stopBtn = new QToolButton(titleRow);
    stopBtn->setObjectName(QStringLiteral("StartStopButton"));
    stopBtn->setText(trKey(QStringLiteral("torrent_stop")));
    stopBtn->setToolTip(trKey(QStringLiteral("torrent_stop")));
    stopBtn->setEnabled(!stopped);
    titles->addWidget(stopBtn);
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
    if (torrent.status == TorrentStatus::Downloading && torrent.eta >= 0) {
        const QString etaText = formatEta(torrent.eta);
        if (!etaText.isEmpty()) {
            detailRow->addStretch();
            auto *eta = new QLabel(trArgs(QStringLiteral("eta"), {{QStringLiteral("value"), etaText}}), details);
            eta->setObjectName(QStringLiteral("Secondary"));
            detailRow->addWidget(eta);
        }
    }
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
            meta << trArgs(
                QStringLiteral("pieces_meta"),
                {{QStringLiteral("count"), QString::number(torrent.pieceCount)},
                 {QStringLiteral("pieces"),
                  pluralForm(torrent.pieceCount,
                             trKey(QStringLiteral("pieces_one")),
                             trKey(QStringLiteral("pieces_few")),
                             trKey(QStringLiteral("pieces_many")))},
                 {QStringLiteral("size"), formatBytes(torrent.pieceSize)}});
        }
        if (!meta.isEmpty()) {
            auto *info = new QLabel(meta.join(QStringLiteral("  ·  ")), cardWidget);
            info->setObjectName(QStringLiteral("Secondary"));
            root->addWidget(info);
        }
    }

    connect(startBtn, &QToolButton::clicked, this, [this, torrentId]() { startStopTorrent(torrentId, true); });
    connect(stopBtn, &QToolButton::clicked, this, [this, torrentId]() { startStopTorrent(torrentId, false); });
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
        popupAddAction(popup, trKey(QStringLiteral("trackers_show")), true, [this, torrent]() {
            defer([this, torrent]() { showTrackers(torrent.id, torrent.name); });
        });
        const bool stopped = torrent.status == TorrentStatus::Stopped;
        popupAddAction(popup, trKey(QStringLiteral("torrent_start")), stopped, [this, torrent]() {
            startStopTorrent(torrent.id, true);
        });
        popupAddAction(popup, trKey(QStringLiteral("torrent_stop")), !stopped, [this, torrent]() {
            startStopTorrent(torrent.id, false);
        });
        popupAddAction(popup,
                         trKey(QStringLiteral("copy_hash")),
                         !torrent.hashString.isEmpty(),
                         [hash = torrent.hashString]() {
                             QApplication::clipboard()->setText(hash.toLower());
                         });
        popupAddAction(popup,
                         trKey(QStringLiteral("copy_magnet")),
                         !torrent.magnetUri().isEmpty(),
                         [magnet = torrent.magnetUri()]() {
                             QApplication::clipboard()->setText(magnet);
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

void MainWindow::showTrackers(qint64 torrentId, const QString &name)
{
    QString error;
    std::optional<QString> endpoint = normalizeRpcUrl(settings_.rpcUrl, &error);
    QVector<Tracker> trackers;
    if (endpoint.has_value()) {
        RpcClient client(*endpoint);
        trackers = client.getTorrentTrackers(torrentId, &error);
    }
    const QString title = trArgs(QStringLiteral("trackers_title"), {{QStringLiteral("name"), name}});
    if (!error.isEmpty() && trackers.isEmpty()) {
        QMessageBox::warning(this, title, error);
        return;
    }
    trackersExec(this, stylesheet(settings_.theme), title, trackers);
}

void MainWindow::startStopTorrent(qint64 torrentId, bool start)
{
    const QString rpcUrl = settings_.rpcUrl;
    QThread *thread = QThread::create([this, torrentId, rpcUrl, start]() {
        QString error;
        std::optional<QString> endpoint = normalizeRpcUrl(rpcUrl, &error);
        if (!endpoint.has_value()) {
            QMetaObject::invokeMethod(this, [this, error, start]() { onStartStopReady(error, start); },
                                      Qt::QueuedConnection);
            return;
        }
        RpcClient client(*endpoint);
        const bool ok = start ? client.startTorrent(torrentId, &error) : client.stopTorrent(torrentId, &error);
        if (!ok) {
            QMetaObject::invokeMethod(this, [this, error, start]() { onStartStopReady(error, start); },
                                      Qt::QueuedConnection);
            return;
        }
        QMetaObject::invokeMethod(this, [this, start]() { onStartStopReady({}, start); }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
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

void MainWindow::openMagnetDialog()
{
    if (adding_) {
        return;
    }
    if (statusMode_ != QStringLiteral("online")) {
        QMessageBox::warning(this,
                             trKey(QStringLiteral("add_magnet_title")),
                             trKey(QStringLiteral("add_magnet_needs_rpc")));
        return;
    }
    QString initial;
    if (QClipboard *clipboard = QApplication::clipboard()) {
        const QString text = clipboard->text().trimmed();
        if (normalizeMagnetLink(text)) {
            initial = text;
        }
    }
    const std::optional<QString> magnet =
        magnetPrompt(this, stylesheet(settings_.theme), initial);
    if (!magnet.has_value()) {
        return;
    }
    startAddMagnet(*magnet);
}

void MainWindow::openCreateTorrentDialog()
{
    if (adding_) {
        return;
    }
    defer([this]() {
        const bool rpcOnline = statusMode_ == QStringLiteral("online");
        const std::optional<CreateTorrentResult> result =
            createTorrentExec(this, stylesheet(settings_.theme), rpcOnline);
        if (!result.has_value()) {
            return;
        }
        if (result->addAfter) {
            startAdd(result->torrentPath);
        }
    });
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
    QFile file(trimmed);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, trKey(QStringLiteral("add_failed")), file.errorString());
        return;
    }
    const QByteArray content = file.readAll();
    startAddMetainfo(content, QFileInfo(trimmed).fileName());
}

void MainWindow::startAddMagnet(const QString &magnet)
{
    QString parseError;
    const std::optional<QString> normalized = normalizeMagnetLink(magnet, &parseError);
    if (!normalized.has_value()) {
        QMessageBox::warning(this, trKey(QStringLiteral("add_magnet_title")), parseError);
        return;
    }
    if (adding_) {
        return;
    }
    adding_ = true;
    statusMode_ = QStringLiteral("copying");
    setStatus();
    if (!pendingCatalogName_.isEmpty()) {
        showAddingNotice(pendingCatalogName_);
    }

    const QString rpcUrl = settings_.rpcUrl;
    const QString payload = *normalized;
    QThread *thread = QThread::create([this, rpcUrl, payload]() {
        QString error;
        std::optional<QString> endpoint = normalizeRpcUrl(rpcUrl, &error);
        if (endpoint.has_value()) {
            RpcClient client(*endpoint);
            if (client.addTorrentMagnet(payload, &error).isEmpty() && !error.isEmpty()) {
                // keep error
            }
        }
        QMetaObject::invokeMethod(this,
                                  [this, error]() { onAddedReady(std::nullopt, error, true); },
                                  Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void MainWindow::startAddMetainfo(const QByteArray &content, const QString &preferredName)
{
    if (adding_) {
        return;
    }
    if (content.isEmpty()) {
        QMessageBox::warning(this,
                             trKey(QStringLiteral("add_failed")),
                             trKey(QStringLiteral("rpc_empty_file")));
        return;
    }
    const bool rpcOnline = statusMode_ == QStringLiteral("online");
    adding_ = true;
    statusMode_ = QStringLiteral("copying");
    setStatus();

    AppSettings settings = settings_;
    const QString filename = preferredName.trimmed().isEmpty()
                                 ? QStringLiteral("download.torrent")
                                 : preferredName.trimmed();
    QThread *thread = QThread::create([this, content, settings, rpcOnline, filename]() {
        QString error;
        std::optional<QString> saved;
        if (rpcOnline) {
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
                const QString dest = destRoot + QLatin1Char('/') + filename;
                if (QFile destFile(dest); destFile.open(QIODevice::WriteOnly)) {
                    destFile.write(content);
                    saved = dest;
                } else {
                    error = destFile.errorString();
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

        const bool proxyChanged = settings_.httpProxyPort != result->httpProxyPort ||
                                  normalizeCatalogProxy(settings_.catalogProxy) !=
                                      normalizeCatalogProxy(result->catalogProxy);
        settings_.rpcUrl = result->rpcUrl;
        settings_.torrentsDir = result->torrentsDir;
        settings_.refreshSeconds = result->refreshSeconds;
        settings_.httpProxyPort = result->httpProxyPort;
        settings_.catalogProxy = normalizeCatalogProxy(result->catalogProxy);
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
        if (proxyChanged && catalogMode_) {
            catalogLoaded_ = false;
            spawnCatalog();
        }
        spawnRefresh();
    });
}

void MainWindow::dispatchRefresh()
{
    spawnRefresh();
}

void MainWindow::dispatchAdd()
{
    showPopupBelow(this, reinterpret_cast<quintptr>(addButton_), settings_.theme, [this](void *popup) {
        popupAddAction(popup, trKey(QStringLiteral("add_torrent_file")), true, [this]() {
            defer([this]() { openTorrentFile(); });
        });
        popupAddAction(popup, trKey(QStringLiteral("add_magnet")), true, [this]() {
            defer([this]() { openMagnetDialog(); });
        });
    });
}

void MainWindow::dispatchCreate()
{
    openCreateTorrentDialog();
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

void MainWindow::dispatchPostman()
{
    showCatalog();
}

void MainWindow::pollWorker()
{
    const QString query = lineEditText(reinterpret_cast<quintptr>(searchEdit_));
    if (query != searchCache_) {
        searchCache_ = query;
        if (!catalogMode_) {
            renderCards();
        }
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
        if (!catalogMode_) {
            setLabelText(reinterpret_cast<quintptr>(summaryLabel_), error);
        }
    }
    if (catalogMode_) {
        // Periodic RPC refresh must not rebuild the catalog scroll area (resets scroll position).
        setStatus();
    } else {
        applyChrome();
        renderCards();
    }
}

void MainWindow::onAddedReady(std::optional<QString> savedPath, QString error, bool magnet)
{
    adding_ = false;
    if (error.isEmpty()) {
        pendingCatalogId_ = 0;
        pendingCatalogName_.clear();
        if (savedPath.has_value()) {
            QMessageBox::information(this,
                                     trKey(QStringLiteral("added_title")),
                                     trArgs(QStringLiteral("added_body"),
                                            {{QStringLiteral("path"), *savedPath}}));
        }
        if (catalogMode_) {
            renderCatalog();
        }
        spawnRefresh();
        return;
    }
    if (magnet && rpcMagnetUnsupported(error) && pendingCatalogId_ > 0) {
        const qint64 id = pendingCatalogId_;
        const QString name = pendingCatalogName_;
        pendingCatalogId_ = 0;
        pendingCatalogName_.clear();
        startAddCatalogFile(id, name, true);
        return;
    }
    pendingCatalogId_ = 0;
    pendingCatalogName_.clear();
    const QString text =
        magnet && rpcMagnetUnsupported(error) ? trKey(QStringLiteral("add_magnet_unsupported")) : error;
    QMessageBox::warning(this, trKey(QStringLiteral("add_failed")), text);
    if (catalogMode_) {
        renderCatalog();
    }
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

void MainWindow::onStartStopReady(QString error, bool start)
{
    if (!error.isEmpty()) {
        const QString title = trKey(start ? QStringLiteral("torrent_start") : QStringLiteral("torrent_stop"));
        const QString text =
            rpcMethodUnsupported(error) ? trKey(QStringLiteral("torrent_start_unsupported")) : error;
        QMessageBox::warning(this, title, text);
        return;
    }
    spawnRefresh();
}

void MainWindow::buildCatalogPage()
{
    auto *page = new QWidget(stack_);
    page->setObjectName(QStringLiteral("CatalogPage"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *head = new QWidget(page);
    auto *headLayout = new QVBoxLayout(head);
#if defined(Q_OS_MACOS)
    headLayout->setContentsMargins(18, 14, 18, 0);
#else
    headLayout->setContentsMargins(18, 16, 18, 0);
#endif
    headLayout->setSpacing(12);

    auto *controls = new QWidget(head);
    auto *row = new QHBoxLayout(controls);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);
    catalogSearch_ = new QLineEdit(controls);
    catalogSearch_->setObjectName(QStringLiteral("Search"));
    catalogSearch_->setClearButtonEnabled(true);
    row->addWidget(catalogSearch_, 1);
    catalogCategory_ = new QComboBox(controls);
    catalogCategory_->setMinimumWidth(140);
    catalogCategory_->setMaximumWidth(200);
    row->addWidget(catalogCategory_);
    catalogOrder_ = new QComboBox(controls);
    catalogOrder_->setMinimumWidth(130);
    catalogOrder_->setMaximumWidth(180);
    row->addWidget(catalogOrder_);
    catalogRefresh_ = new QToolButton(controls);
    catalogRefresh_->setObjectName(QStringLiteral("RefreshButton"));
    catalogRefresh_->setText(QStringLiteral("↻"));
    row->addWidget(catalogRefresh_);
    headLayout->addWidget(controls);

    catalogStatusLabel_ = new QLabel(head);
    catalogStatusLabel_->setObjectName(QStringLiteral("Secondary"));
    catalogStatusLabel_->setWordWrap(true);
    headLayout->addWidget(catalogStatusLabel_);
    layout->addWidget(head);

    NativeWidget overlay = NativeWidget::overlayScroll();
    QWidget *scroll = overlay.widget();
    scroll->setObjectName(QStringLiteral("TorrentScroll"));
    catalogScrollPtr_ = reinterpret_cast<quintptr>(scroll);
    layout->addWidget(scroll, 1);
    overlay.releaseOwnership();

    auto *pagerHost = new QWidget(page);
    auto *pager = new QHBoxLayout(pagerHost);
    pager->setContentsMargins(18, 8, 18, 14);
    catalogPrevButton_ = new QPushButton(trKey(QStringLiteral("postman_prev")), pagerHost);
    catalogNextButton_ = new QPushButton(trKey(QStringLiteral("postman_next")), pagerHost);
    catalogPageLabel_ = new QLabel(pagerHost);
    catalogPageLabel_->setObjectName(QStringLiteral("Secondary"));
    pager->addWidget(catalogPrevButton_);
    pager->addStretch();
    pager->addWidget(catalogPageLabel_);
    pager->addStretch();
    pager->addWidget(catalogNextButton_);
    layout->addWidget(pagerHost);

    stack_->addWidget(page);
    fillCatalogCombos();
}

void MainWindow::fillCatalogCombos()
{
    if (catalogCategory_ == nullptr || catalogOrder_ == nullptr) {
        return;
    }
    const int category = catalogCategory_->count() == 0 ? catalogQuery_.category
                                                        : catalogCategory_->currentData().toInt();
    const int order =
        catalogOrder_->count() == 0 ? catalogQuery_.orderBy : catalogOrder_->currentData().toInt();
    const QSignalBlocker blockCategory(catalogCategory_);
    const QSignalBlocker blockOrder(catalogOrder_);
    catalogCategory_->clear();
    for (const PostmanCategory &item : postmanCategories()) {
        const QString label = item.labelKey != nullptr ? trKey(QString::fromUtf8(item.labelKey))
                                                       : QString::fromUtf8(item.label);
        catalogCategory_->addItem(label, item.id);
    }
    catalogOrder_->clear();
    for (const PostmanSort &item : postmanSorts()) {
        catalogOrder_->addItem(trKey(QString::fromUtf8(item.labelKey)), item.id);
    }
    const int categoryIndex = catalogCategory_->findData(category);
    catalogCategory_->setCurrentIndex(categoryIndex < 0 ? 0 : categoryIndex);
    const int orderIndex = catalogOrder_->findData(order);
    catalogOrder_->setCurrentIndex(orderIndex < 0 ? 0 : orderIndex);
}

void MainWindow::showCatalog()
{
    catalogMode_ = true;
    if (stack_ != nullptr) {
        stack_->setCurrentIndex(1);
    }
    if (postmanButton_ != nullptr) {
        setChecked(reinterpret_cast<quintptr>(postmanButton_), true);
    }
    renderCatalog();
    if (!catalogLoaded_) {
        spawnCatalog();
    }
}

void MainWindow::spawnCatalog()
{
    if (catalogBusy_) {
        return;
    }
    catalogBusy_ = true;
    catalogError_.clear();
    renderCatalog();
    const PostmanQuery query = catalogQuery_;
    const PostmanProxy proxy = catalogProxy(settings_);
    const QString lang = language();
    QThread *thread = QThread::create([this, query, proxy, lang]() {
        setLanguage(lang);
        QString error;
        const PostmanPage page = catalog_.fetch(query, proxy, &error);
        QMetaObject::invokeMethod(
            this, [this, page, error]() { onCatalogReady(page, error); }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void MainWindow::onCatalogReady(PostmanPage page, QString error)
{
    catalogBusy_ = false;
    catalogLoaded_ = true;
    catalogError_ = error;
    if (error.isEmpty()) {
        catalogPageData_ = std::move(page);
    } else if (catalogPageData_.rows.isEmpty()) {
        catalogPageData_.start = catalogQuery_.start;
        catalogPageData_.nextStart = -1;
        catalogPageData_.prevStart = -1;
    }
    renderCatalog();
}

void MainWindow::renderCatalog()
{
    if (catalogScrollPtr_ == 0) {
        return;
    }
    const bool busy = catalogBusy_;
    catalogRefresh_->setEnabled(!busy);
    catalogCategory_->setEnabled(!busy);
    catalogOrder_->setEnabled(!busy);
    catalogPrevButton_->setEnabled(!busy && catalogPageData_.prevStart >= 0);
    catalogNextButton_->setEnabled(!busy && catalogPageData_.nextStart >= 0);

    QString status;
    if (busy) {
        status = trKey(QStringLiteral("postman_loading"));
    } else if (!catalogError_.isEmpty()) {
        status = catalogError_;
    } else {
        status = trArgs(QStringLiteral("postman_summary"),
                        {{QStringLiteral("count"), QString::number(catalogPageData_.rows.size())}});
    }
    if (!adding_) {
        const bool inHeader = !catalogPageData_.rows.isEmpty();
        catalogStatusLabel_->setVisible(inHeader);
        if (inHeader) {
            setObjectNamePtr(reinterpret_cast<quintptr>(catalogStatusLabel_), QStringLiteral("Secondary"));
            setLabelText(reinterpret_cast<quintptr>(catalogStatusLabel_), status);
        }
    }

    QString range = QStringLiteral("—");
    if (!catalogPageData_.rows.isEmpty()) {
        range = trArgs(QStringLiteral("postman_range"),
                       {{QStringLiteral("from"), QString::number(catalogPageData_.start + 1)},
                        {QStringLiteral("to"),
                         QString::number(catalogPageData_.start + catalogPageData_.rows.size())}});
    }
    setLabelText(reinterpret_cast<quintptr>(catalogPageLabel_), range);

    auto *cards = new QWidget;
    auto *layout = new QVBoxLayout(cards);
    layout->setContentsMargins(18, 12, 14, 16);
    layout->setSpacing(12);
    if (catalogPageData_.rows.isEmpty()) {
        const QString text = !catalogError_.isEmpty() ? catalogError_
                             : busy                   ? trKey(QStringLiteral("postman_loading"))
                                                      : trKey(QStringLiteral("postman_empty"));
        auto *empty = new QLabel(text, cards);
        empty->setObjectName(QStringLiteral("Secondary"));
        empty->setWordWrap(true);
        layout->addWidget(empty);
    } else {
        for (const PostmanTorrent &item : catalogPageData_.rows) {
            layout->addWidget(makeCatalogCard(item));
        }
    }
    layout->addStretch();
    overlaySetWidget(catalogScrollPtr_, cards);
}

QWidget *MainWindow::makeCatalogCard(const PostmanTorrent &item)
{
    NativeWidget card = NativeWidget::torrentCard(settings_.theme);
    QWidget *cardWidget = card.widget();
    auto *root = new QVBoxLayout(cardWidget);
    root->setContentsMargins(14, 11, 14, 11);
    root->setSpacing(6);

    auto *name = new QLabel(item.name, cardWidget);
    name->setObjectName(QStringLiteral("TorrentName"));
    name->setWordWrap(true);
    root->addWidget(name);

    QStringList meta;
    if (!item.category.isEmpty()) {
        meta << item.category;
    }
    if (!item.sizeText.isEmpty()) {
        meta << item.sizeText;
    }
    if (item.seeders >= 0) {
        meta << trArgs(QStringLiteral("postman_swarm"),
                       {{QStringLiteral("seeds"), QString::number(item.seeders)},
                        {QStringLiteral("leech"), QString::number(item.leechers)}});
    }
    if (!item.added.isEmpty()) {
        meta << item.added;
    }
    auto *details = new QLabel(meta.join(QStringLiteral("  ·  ")), cardWidget);
    details->setObjectName(QStringLiteral("Secondary"));
    details->setWordWrap(true);
    root->addWidget(details);

    auto *actions = new QWidget(cardWidget);
    auto *actionRow = new QHBoxLayout(actions);
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(8);
    actionRow->addStretch();
    auto *open = new QPushButton(trKey(QStringLiteral("postman_open")), actions);
    open->setEnabled(item.id > 0 || !item.magnet.isEmpty());
    actionRow->addWidget(open);
    auto *add = new QPushButton(trKey(QStringLiteral("postman_add")), actions);
    add->setObjectName(QStringLiteral("Primary"));
    actionRow->addWidget(add);
    root->addWidget(actions);
    connect(open, &QPushButton::clicked, this, [item]() {
        if (item.id > 0) {
            QUrl url(QString::fromLatin1(POSTMAN_ORIGIN) + QStringLiteral("index.php"));
            url.setQuery(QStringLiteral("view=TorrentDetail&id=%1").arg(item.id));
            QDesktopServices::openUrl(url);
            return;
        }
        QDesktopServices::openUrl(QUrl(item.magnet));
    });
    connect(add, &QPushButton::clicked, this, [this, item]() { addCatalogItem(item); });
    card.releaseOwnership();
    return cardWidget;
}

void MainWindow::addCatalogItem(const PostmanTorrent &item)
{
    if (adding_) {
        return;
    }
    const bool rpcUp = statusMode_ == QStringLiteral("online") || statusMode_ == QStringLiteral("updating");
    QString parseError;
    const bool magnetOk = normalizeMagnetLink(item.magnet, &parseError).has_value();
    if (!magnetOk || !rpcUp) {
        if (item.id > 0) {
            startAddCatalogFile(item.id, item.name, false);
            return;
        }
        QMessageBox::warning(this,
                             trKey(QStringLiteral("add_magnet_title")),
                             parseError.isEmpty() ? trKey(QStringLiteral("add_magnet_needs_rpc")) : parseError);
        return;
    }
    pendingCatalogId_ = item.id;
    pendingCatalogName_ = item.name;
    startAddMagnet(item.magnet);
}

void MainWindow::showAddingNotice(const QString &name)
{
    if (catalogStatusLabel_ == nullptr) {
        return;
    }
    catalogStatusLabel_->setVisible(true);
    const QString title = name.trimmed().isEmpty() ? trKey(QStringLiteral("postman_add")) : name.trimmed();
    setObjectNamePtr(reinterpret_cast<quintptr>(catalogStatusLabel_), QStringLiteral("CatalogNotice"));
    setLabelText(reinterpret_cast<quintptr>(catalogStatusLabel_),
                 trArgs(QStringLiteral("postman_adding"), {{QStringLiteral("name"), title}}));
}

void MainWindow::startAddCatalogFile(qint64 id, const QString &name, bool rpcOnline)
{
    if (adding_ || id <= 0) {
        return;
    }
    adding_ = true;
    statusMode_ = QStringLiteral("copying");
    setStatus();
    showAddingNotice(name);
    const QString lang = language();
    const PostmanProxy proxy = catalogProxy(settings_);
    QThread *thread = QThread::create([this, id, name, rpcOnline, lang, proxy]() {
        setLanguage(lang);
        QString error;
        const QByteArray bytes = catalog_.downloadTorrent(id, proxy, &error);
        QMetaObject::invokeMethod(
            this,
            [this, bytes, error, name, rpcOnline]() {
                adding_ = false;
                if (bytes.isEmpty() || !error.isEmpty()) {
                    QMessageBox::warning(this,
                                         trKey(QStringLiteral("add_failed")),
                                         error.isEmpty() ? trKey(QStringLiteral("postman_download_failed")) : error);
                    if (catalogMode_) {
                        renderCatalog();
                    }
                    return;
                }
                statusMode_ = rpcOnline ? QStringLiteral("online") : QStringLiteral("offline");
                startAddMetainfo(bytes, torrentFileName(name));
            },
            Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

} // namespace i2p
