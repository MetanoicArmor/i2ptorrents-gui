use std::cell::RefCell;
use std::path::{Path, PathBuf};
use std::rc::Rc;
use std::sync::mpsc::{self, Receiver, Sender};
use std::thread;

use qtrs::prelude::*;
use qtrs::{clipboard, desktopservices, warning, MessageBox, Spacer, SpacerExt};

use crate::config::{AppSettings, MIN_WINDOW_HEIGHT, MIN_WINDOW_WIDTH};
use crate::i18n::{language, set_language, t, t_args};
use crate::models::{format_bytes, format_rate, Torrent, TorrentStatus};
use crate::qt_chrome as chrome;
use crate::rpc::{normalize_rpc_url, rpc_method_unsupported, TransmissionRPC};
use crate::theme::stylesheet;
use crate::{application_icon_path, version, APP_NAME};

const ARROW_CURSOR: i32 = 0;
const WHATS_THIS_CURSOR: i32 = 4;

fn native_shortcut(seq: &str) -> String {
    if cfg!(target_os = "macos") {
        seq.replace("Ctrl+", "⌘")
    } else {
        seq.to_string()
    }
}

fn tip_with_shortcuts(label_key: &str, sequences: &[&str]) -> String {
    let shortcut = sequences
        .iter()
        .map(|seq| native_shortcut(seq))
        .collect::<Vec<_>>()
        .join(" / ");
    t_args(label_key, &[("shortcut", &shortcut)])
}

enum WorkerMsg {
    Torrents(Result<Vec<Torrent>, String>),
    Added(Result<Option<String>, String>),
    Removed(Result<(), String>),
}

struct Gui {
    window: Widget,
    scroll_ptr: usize,
    status_ptr: usize,
    summary_ptr: usize,
    subtitle_ptr: usize,
    section_ptr: usize,
    add_ptr: usize,
    settings_ptr: usize,
    about_ptr: usize,
    search_ptr: usize,
    refresh_ptr: usize,
    filter_ptrs: [usize; 3],
    settings: AppSettings,
    torrents: Vec<Torrent>,
    filter: String,
    busy: bool,
    adding: bool,
    search_cache: String,
    status_mode: String,
    status_detail: String,
    tx: Sender<WorkerMsg>,
    rx: Receiver<WorkerMsg>,
    timer: Timer,
    _poll: Timer,
}

pub fn run() -> i32 {
    let app = Application::new();
    if let Some(icon) = application_icon_path() {
        app.set_icon(&icon.to_string_lossy());
    }
    chrome::apply_app_font();
    let settings = AppSettings::load();
    let (width, height) = settings.window_size();
    let window = Widget::new()
        .title(&format!("{APP_NAME} {}", version()))
        .size(width, height)
        .build();
    window.set_object_name("MainWindow");
    window.set_min_size(MIN_WINDOW_WIDTH as i32, MIN_WINDOW_HEIGHT as i32);

    set_language(&settings.language);
    window.set_style_sheet(&stylesheet(&settings.theme));
    chrome::install_rounded_tooltips();
    chrome::apply_tooltip_palette(&settings.theme);

    let (tx, rx) = mpsc::channel();
    let holder: Rc<RefCell<Option<Rc<RefCell<Gui>>>>> = Rc::new(RefCell::new(None));

    let mut outer = HBoxLayout::with_parent(&window);
    outer.set_contents_margins(0, 0, 0, 0);
    outer.set_spacing(0);

    let (sidebar, filter_ptrs, settings_ptr, about_ptr, subtitle_ptr, section_ptr) =
        make_sidebar(&holder);
    outer.add(sidebar);

    let split = Widget::new().build();
    split.set_object_name("PaneSplit");
    chrome::set_object_name_ptr(split.widget_ptr() as usize, "PaneSplit");
    split.set_fixed_width(1);
    outer.add(split);

    let (surface, scroll_ptr, status_ptr, summary_ptr, add_ptr, search_ptr, refresh_ptr) =
        make_surface(&holder);
    outer.add(surface);

    for (key, kind) in [
        ("Ctrl+T", "add"),
        ("Ctrl+O", "open"),
        ("Ctrl+S", "settings"),
        ("Ctrl+,", "settings"),
    ] {
        let slot = holder.clone();
        chrome::add_shortcut(&window, key, move || dispatch(&slot, kind));
    }

    let interval = (settings.refresh_seconds.max(2) * 1000) as i32;
    let timer_slot = holder.clone();
    let timer = Timer::new(interval)
        .on_timeout(move || dispatch(&timer_slot, "refresh"))
        .build();
    let poll_slot = holder.clone();
    let poll = Timer::new(80)
        .on_timeout(move || poll_worker(&poll_slot))
        .build();

    let gui = Rc::new(RefCell::new(Gui {
        window,
        scroll_ptr,
        status_ptr,
        summary_ptr,
        subtitle_ptr,
        section_ptr,
        add_ptr,
        settings_ptr,
        about_ptr,
        search_ptr,
        refresh_ptr,
        filter_ptrs,
        settings,
        torrents: Vec::new(),
        filter: "all".into(),
        busy: false,
        adding: false,
        search_cache: String::new(),
        status_mode: "connecting".into(),
        status_detail: String::new(),
        tx,
        rx,
        timer,
        _poll: poll,
    }));
    *holder.borrow_mut() = Some(gui.clone());

    {
        let mut g = gui.borrow_mut();
        apply_chrome(&mut g);
        render_cards(&mut g);
        g.window.show();
        chrome::apply_window_material(&g.window, &g.settings.theme);
        spawn_refresh(&mut g);
    }

    let code = app.exec();
    if let Ok(mut g) = gui.try_borrow_mut() {
        let width = g.window.width();
        let height = g.window.height();
        g.settings.capture_window_size(width, height);
        let _ = g.settings.save();
    }
    std::mem::forget(outer);
    std::mem::forget(gui);
    std::mem::forget(holder);
    code
}

fn dispatch(holder: &Rc<RefCell<Option<Rc<RefCell<Gui>>>>>, kind: &str) {
    let Some(gui) = holder.borrow().clone() else {
        return;
    };
    match kind {
        "refresh" => {
            let Ok(mut g) = gui.try_borrow_mut() else {
                return;
            };
            spawn_refresh(&mut g);
        }
        "add" | "open" => open_torrent_file(&gui),
        "settings" => open_settings(&gui),
        "about" => open_about(&gui),
        "filter-all" => {
            if let Ok(mut g) = gui.try_borrow_mut() {
                set_filter(&mut g, "all");
            }
        }
        "filter-downloading" => {
            if let Ok(mut g) = gui.try_borrow_mut() {
                set_filter(&mut g, "downloading");
            }
        }
        "filter-seeding" => {
            if let Ok(mut g) = gui.try_borrow_mut() {
                set_filter(&mut g, "seeding");
            }
        }
        _ => {}
    }
}

fn poll_worker(holder: &Rc<RefCell<Option<Rc<RefCell<Gui>>>>>) {
    let Some(gui) = holder.borrow().clone() else {
        return;
    };
    let Ok(mut g) = gui.try_borrow_mut() else {
        return;
    };
    while let Ok(msg) = g.rx.try_recv() {
        match msg {
            WorkerMsg::Torrents(Ok(rows)) => {
                g.busy = false;
                g.torrents = rows;
                g.status_mode = "online".into();
                g.status_detail.clear();
                apply_chrome(&mut g);
                render_cards(&mut g);
            }
            WorkerMsg::Torrents(Err(err)) => {
                g.busy = false;
                g.status_mode = "offline".into();
                g.status_detail = err.clone();
                apply_chrome(&mut g);
                chrome::set_label_text(g.summary_ptr, &err);
            }
            WorkerMsg::Added(Ok(saved)) => {
                g.adding = false;
                if let Some(path) = saved {
                    MessageBox::new()
                        .window_title(&t("added_title"))
                        .text(&t_args("added_body", &[("path", &path)]))
                        .parent(&g.window)
                        .build()
                        .exec();
                }
                spawn_refresh(&mut g);
            }
            WorkerMsg::Added(Err(err)) => {
                g.adding = false;
                warning(Some(&g.window), &t("add_failed"), &err);
                spawn_refresh(&mut g);
            }
            WorkerMsg::Removed(Ok(())) => spawn_refresh(&mut g),
            WorkerMsg::Removed(Err(err)) => warning(Some(&g.window), &t("remove_title"), &err),
        }
    }
    let query = chrome::line_edit_text(g.search_ptr);
    if query != g.search_cache {
        g.search_cache = query;
        render_cards(&mut g);
    }
}

fn make_sidebar(
    holder: &Rc<RefCell<Option<Rc<RefCell<Gui>>>>>,
) -> (Widget, [usize; 3], usize, usize, usize, usize) {
    let sidebar = Widget::new().build();
    sidebar.set_object_name("Sidebar");
    chrome::set_object_name_ptr(sidebar.widget_ptr() as usize, "Sidebar");
    sidebar.set_fixed_width(220);
    let mut side = VBoxLayout::with_parent(&sidebar);
    side.set_contents_margins(12, if cfg!(target_os = "macos") { 40 } else { 14 }, 12, 12);
    side.set_spacing(2);
    let title = Label::new(APP_NAME).build();
    title.set_object_name("AppTitle");
    side.add(title);
    let subtitle = Label::new(&t("subtitle")).build();
    subtitle.set_object_name("AppSubtitle");
    let subtitle_ptr = subtitle.widget_ptr() as usize;
    side.add(subtitle);
    side.add_spacer(Spacer::vertical(18));
    let section = Label::new(&t("section_torrents")).build();
    section.set_object_name("SectionTitle");
    let section_ptr = section.widget_ptr() as usize;
    side.add(section);
    let mut filter_ptrs = [0usize; 3];
    for (index, (key, label_key)) in [
        ("all", "filter_all"),
        ("downloading", "filter_downloading"),
        ("seeding", "filter_seeding"),
    ]
    .into_iter()
    .enumerate()
    {
        let slot = holder.clone();
        let cmd = format!("filter-{key}");
        let button = PushButton::new(&t(label_key))
            .on_clicked(move || dispatch(&slot, &cmd))
            .build();
        button.set_object_name("Filter");
        chrome::set_checkable(&button, true);
        chrome::set_checked(button.widget_ptr() as usize, key == "all");
        filter_ptrs[index] = button.widget_ptr() as usize;
        side.add(button);
    }
    side.add_spacer(Spacer::vertical_expanding());
    let about_slot = holder.clone();
    let about = PushButton::new(&t("about"))
        .on_clicked(move || dispatch(&about_slot, "about"))
        .build();
    about.set_object_name("AboutButton");
    let about_ptr = about.widget_ptr() as usize;
    side.add(about);
    let settings_slot = holder.clone();
    let settings_btn = PushButton::new(&t("settings"))
        .on_clicked(move || dispatch(&settings_slot, "settings"))
        .build();
    settings_btn.set_object_name("SettingsButton");
    chrome::set_tooltip(
        &settings_btn,
        &tip_with_shortcuts("settings_tip", &["Ctrl+,", "Ctrl+S"]),
    );
    let settings_ptr = settings_btn.widget_ptr() as usize;
    side.add(settings_btn);
    std::mem::forget(side);
    (
        sidebar,
        filter_ptrs,
        settings_ptr,
        about_ptr,
        subtitle_ptr,
        section_ptr,
    )
}

fn make_surface(
    holder: &Rc<RefCell<Option<Rc<RefCell<Gui>>>>>,
) -> (Widget, usize, usize, usize, usize, usize, usize) {
    let surface = Widget::new().build();
    surface.set_object_name("Surface");
    chrome::set_object_name_ptr(surface.widget_ptr() as usize, "Surface");
    let mut body = VBoxLayout::with_parent(&surface);
    body.set_contents_margins(0, 0, 0, 0);
    body.set_spacing(0);

    let head = Widget::new().build();
    let mut head_layout = VBoxLayout::with_parent(&head);
    head_layout.set_contents_margins(18, if cfg!(target_os = "macos") { 14 } else { 16 }, 18, 0);
    head_layout.set_spacing(12);

    let top = Widget::new().build();
    let mut top_layout = HBoxLayout::with_parent(&top);
    let status = Label::new(&t("connecting")).build();
    status.set_object_name("StatusOffline");
    status.set_cursor(WHATS_THIS_CURSOR);
    let status_ptr = status.widget_ptr() as usize;
    top_layout.add(status);
    top_layout.add_spacer(Spacer::horizontal_expanding());
    let refresh_slot = holder.clone();
    let refresh = ToolButton::new()
        .text("↻")
        .on_clicked(move || dispatch(&refresh_slot, "refresh"))
        .build();
    refresh.set_object_name("RefreshButton");
    let refresh_ptr = refresh.widget_ptr() as usize;
    chrome::set_tooltip(&refresh, &t("refresh"));
    top_layout.add(refresh);
    let add_slot = holder.clone();
    let add = PushButton::new(&t("add_torrent"))
        .on_clicked(move || dispatch(&add_slot, "add"))
        .build();
    add.set_object_name("Primary");
    chrome::set_tooltip(&add, &tip_with_shortcuts("add_torrent_tip", &["Ctrl+T", "Ctrl+O"]));
    let add_ptr = add.widget_ptr() as usize;
    top_layout.add(add);
    std::mem::forget(top_layout);
    head_layout.add(top);

    let search = LineEdit::new("").build();
    search.set_object_name("Search");
    let search_ptr = search.widget_ptr() as usize;
    chrome::set_placeholder(&search, &t("search_placeholder"));
    head_layout.add(search);
    let summary = Label::new("").build();
    summary.set_object_name("Secondary");
    let summary_ptr = summary.widget_ptr() as usize;
    head_layout.add(summary);
    std::mem::forget(head_layout);
    body.add(head);

    let overlay = chrome::NativeWidget::overlay_scroll();
    overlay.set_object_name("TorrentScroll");
    let scroll_ptr = overlay.as_ptr() as usize;
    body.add(overlay);
    std::mem::forget(body);
    (
        surface,
        scroll_ptr,
        status_ptr,
        summary_ptr,
        add_ptr,
        search_ptr,
        refresh_ptr,
    )
}

fn apply_chrome(g: &mut Gui) {
    g.window.set_title(&format!("{APP_NAME} {}", version()));
    chrome::apply_app_font();
    g.window.set_style_sheet(&stylesheet(&g.settings.theme));
    chrome::apply_window_material(&g.window, &g.settings.theme);
    chrome::apply_tooltip_palette(&g.settings.theme);
    chrome::overlay_apply_theme(g.scroll_ptr, &g.settings.theme);
    chrome::set_label_text(g.subtitle_ptr, &t("subtitle"));
    chrome::set_label_text(g.section_ptr, &t("section_torrents"));
    chrome::set_button_text(g.filter_ptrs[0], &t("filter_all"));
    chrome::set_button_text(g.filter_ptrs[1], &t("filter_downloading"));
    chrome::set_button_text(g.filter_ptrs[2], &t("filter_seeding"));
    set_status(g);
    chrome::set_button_text(g.add_ptr, &t("add_torrent"));
    chrome::set_tooltip_ptr(
        g.add_ptr,
        &tip_with_shortcuts("add_torrent_tip", &["Ctrl+T", "Ctrl+O"]),
    );
    chrome::set_button_text(g.settings_ptr, &t("settings"));
    chrome::set_tooltip_ptr(
        g.settings_ptr,
        &tip_with_shortcuts("settings_tip", &["Ctrl+,", "Ctrl+S"]),
    );
    chrome::set_button_text(g.about_ptr, &t("about"));
    chrome::set_placeholder_ptr(g.search_ptr, &t("search_placeholder"));
    chrome::set_tooltip_ptr(g.refresh_ptr, &t("refresh"));
}

fn set_status(g: &Gui) {
    let (text, name, tip, cursor) = match g.status_mode.as_str() {
        "online" => (t("rpc_online"), "StatusOnline", String::new(), ARROW_CURSOR),
        "updating" => (t("updating"), "StatusOffline", String::new(), ARROW_CURSOR),
        "copying" => (
            t("copying_torrent"),
            "StatusOffline",
            String::new(),
            ARROW_CURSOR,
        ),
        "offline" => {
            let mut tip = t("rpc_setup_tip");
            if !g.status_detail.is_empty() {
                tip = format!("{}\n\n{tip}", g.status_detail);
            }
            (t("rpc_offline"), "StatusOffline", tip, WHATS_THIS_CURSOR)
        }
        _ => (
            t("connecting"),
            "StatusOffline",
            t("rpc_setup_tip"),
            WHATS_THIS_CURSOR,
        ),
    };
    chrome::set_label_text(g.status_ptr, &text);
    chrome::set_object_name_ptr(g.status_ptr, name);
    chrome::set_cursor_ptr(g.status_ptr, cursor);
    chrome::set_tooltip_ptr(g.status_ptr, &tip);
    let down: u64 = g.torrents.iter().map(|t| t.rate_download).sum();
    let up: u64 = g.torrents.iter().map(|t| t.rate_upload).sum();
    let summary = t_args(
        "summary",
        &[
            ("count", &g.torrents.len().to_string()),
            ("down", &format_rate(down)),
            ("up", &format_rate(up)),
        ],
    );
    set_summary(g.summary_ptr, &summary);
}

fn set_summary(ptr: usize, text: &str) {
    chrome::set_label_text(ptr, text);
}

fn set_filter(g: &mut Gui, name: &str) {
    g.filter = name.to_string();
    let keys = ["all", "downloading", "seeding"];
    for (index, key) in keys.iter().enumerate() {
        chrome::set_checked(g.filter_ptrs[index], g.filter == *key);
    }
    render_cards(g);
}

fn search_query(g: &Gui) -> String {
    chrome::line_edit_text(g.search_ptr).trim().to_lowercase()
}

fn visible_torrents(g: &Gui) -> Vec<Torrent> {
    let query = search_query(g);
    let mut rows: Vec<Torrent> = g
        .torrents
        .iter()
        .filter(|item| match g.filter.as_str() {
            "downloading" => item.status == TorrentStatus::Downloading,
            "seeding" => item.status == TorrentStatus::Seeding,
            _ => true,
        })
        .filter(|item| {
            query.is_empty()
                || item.name.to_lowercase().contains(&query)
                || item.hash_string.to_lowercase().contains(&query)
        })
        .cloned()
        .collect();
    rows.sort_by(|a, b| {
        (a.status != TorrentStatus::Downloading)
            .cmp(&(b.status != TorrentStatus::Downloading))
            .then_with(|| a.name.to_lowercase().cmp(&b.name.to_lowercase()))
    });
    rows
}

fn render_cards(g: &mut Gui) {
    set_status(g);
    let cards = Widget::new().build();
    let mut layout = VBoxLayout::with_parent(&cards);
    layout.set_contents_margins(18, 12, 14, 16);
    layout.set_spacing(12);
    let rows = visible_torrents(g);
    let downloads = PathBuf::from(&g.settings.torrents_dir);
    let detailed = g.settings.torrent_view != "simple";
    if rows.is_empty() {
        let empty =
            Label::new(&t_args("empty_list", &[("path", &g.settings.torrents_dir)])).build();
        empty.set_object_name("Secondary");
        layout.add(empty);
    } else {
        for torrent in rows {
            layout.add(make_card(&torrent, &downloads, detailed, g));
        }
    }
    layout.add_spacer(Spacer::vertical_expanding());
    chrome::overlay_set_widget(g.scroll_ptr, &cards);
    std::mem::forget(layout);
    std::mem::forget(cards);
}

fn make_card(torrent: &Torrent, downloads: &Path, detailed: bool, g: &Gui) -> chrome::NativeWidget {
    let card = chrome::NativeWidget::torrent_card(&g.settings.theme);
    chrome::set_tooltip(&card, &t("files_tooltip"));
    let files_torrent_id = torrent.id;
    let files_name = torrent.name.clone();
    let files_theme = g.settings.theme.clone();
    let files_rpc = g.settings.rpc_url.clone();
    let files_window = g.window.widget_ptr() as usize;
    chrome::on_click(&card, move || {
        let window = files_window;
        let name = files_name.clone();
        let theme = files_theme.clone();
        let rpc = files_rpc.clone();
        chrome::defer(move || {
            show_files(window, &name, files_torrent_id, &theme, rpc.clone());
        });
    });
    let mut root = VBoxLayout::with_parent(&card);
    root.set_contents_margins(14, 11, 14, 11);
    root.set_spacing(6);

    let title_row = Widget::new().build();
    let mut titles = HBoxLayout::with_parent(&title_row);
    titles.set_contents_margins(0, 0, 0, 0);
    titles.set_spacing(8);
    let name = Label::new(&torrent.name).build();
    name.set_object_name("TorrentName");
    titles.add(name);
    let status = Label::new(&torrent.status.label()).build();
    status.set_object_name("StatusText");
    titles.add(status);
    let hash = torrent.hash_string.clone();
    let folder = downloads.to_path_buf();
    let torrent_name = torrent.name.clone();
    let torrent_id = torrent.id;
    let window_ptr = g.window.widget_ptr() as usize;
    let tx = g.tx.clone();
    let rpc_url = g.settings.rpc_url.clone();
    let theme = g.settings.theme.clone();
    let mut more = ToolButton::new().text("•••").build();
    more.set_object_name("MoreButton");
    chrome::set_tooltip(&more, &t("actions"));
    let more_ptr = more.widget_ptr() as usize;
    let more_slot_hash = hash.clone();
    let more_slot_folder = folder.clone();
    let more_slot_name = torrent_name.clone();
    more.connect_clicked(move || {
        show_actions(
            &more_slot_hash,
            &more_slot_folder,
            &more_slot_name,
            torrent_id,
            window_ptr,
            more_ptr,
            &theme,
            tx.clone(),
            rpc_url.clone(),
        );
    });
    titles.add(more);
    std::mem::forget(titles);
    root.add(title_row);

    let progress = ProgressBar::new()
        .range(0, 1000)
        .value((torrent.progress() * 1000.0).round() as i32)
        .format("")
        .build();
    chrome::set_tooltip(&progress, &t("download_progress"));
    root.add(progress);

    if detailed && !torrent.pieces.is_empty() {
        let map = chrome::NativeWidget::piece_map(&torrent.pieces);
        let have_n = torrent.pieces.iter().filter(|bit| **bit).count();
        chrome::set_tooltip(
            &map,
            &t_args(
                "pieces_tooltip",
                &[
                    ("have", &have_n.to_string()),
                    ("total", &torrent.pieces.len().to_string()),
                ],
            ),
        );
        root.add(map);
    }

    let details = Widget::new().build();
    let mut detail_row = HBoxLayout::with_parent(&details);
    detail_row.set_contents_margins(0, 0, 0, 0);
    detail_row.set_spacing(8);
    let percent = Label::new(&t_args(
        "progress_percent",
        &[("percent", &format!("{:.1}", torrent.progress() * 100.0))],
    ))
    .build();
    percent.set_object_name("Secondary");
    detail_row.add(percent);
    detail_row.add_spacer(Spacer::horizontal_expanding());
    let size = Label::new(&t_args(
        "progress_size",
        &[
            ("done", &format_bytes(torrent.completed())),
            ("total", &format_bytes(torrent.total_size)),
        ],
    ))
    .build();
    size.set_object_name("Secondary");
    detail_row.add(size);
    detail_row.add_spacer(Spacer::horizontal_expanding());
    let rates = Label::new(&format!(
        "↓ {}   ↑ {}",
        format_rate(torrent.rate_download),
        format_rate(torrent.rate_upload)
    ))
    .build();
    rates.set_object_name("Secondary");
    detail_row.add(rates);
    detail_row.add_spacer(Spacer::horizontal_expanding());
    let peers = Label::new(&t_args(
        "peers",
        &[
            ("down", &torrent.peers_sending_to_us.to_string()),
            ("up", &torrent.peers_getting_from_us.to_string()),
        ],
    ))
    .build();
    peers.set_object_name("Secondary");
    detail_row.add(peers);
    std::mem::forget(detail_row);
    root.add(details);

    if detailed {
        let mut meta = Vec::new();
        if !torrent.hash_string.is_empty() {
            meta.push(torrent.short_hash());
        }
        if torrent.piece_count > 0 {
            meta.push(t_args(
                "pieces_meta",
                &[
                    ("count", &torrent.piece_count.to_string()),
                    ("size", &crate::models::format_bytes(torrent.piece_size)),
                ],
            ));
        }
        if !meta.is_empty() {
            let info = Label::new(&meta.join("  ·  ")).build();
            info.set_object_name("Secondary");
            root.add(info);
        }
    }
    std::mem::forget(root);
    card
}

fn show_actions(
    hash: &str,
    folder: &Path,
    torrent_name: &str,
    torrent_id: i64,
    window_ptr: usize,
    more_ptr: usize,
    theme: &str,
    tx: Sender<WorkerMsg>,
    rpc_url: String,
) {
    let hash = hash.to_string();
    let folder = folder.to_path_buf();
    let torrent_name = torrent_name.to_string();
    chrome::show_popup_below(&RawParent(window_ptr), more_ptr, theme, |popup| {
        let name_copy = torrent_name.clone();
        let theme_copy = theme.to_string();
        let rpc_copy = rpc_url.clone();
        chrome::popup_add_action(popup, &t("files_show"), true, move || {
            let name = name_copy.clone();
            let theme = theme_copy.clone();
            let rpc = rpc_copy.clone();
            chrome::defer(move || {
                show_files(window_ptr, &name, torrent_id, &theme, rpc.clone());
            });
        });
        let hash_copy = hash.clone();
        chrome::popup_add_action(popup, &t("copy_hash"), !hash.is_empty(), move || {
            clipboard::set_text(&hash_copy.to_lowercase());
        });
        let folder_copy = folder.clone();
        let name_copy = torrent_name.clone();
        chrome::popup_add_action(popup, &t("open_folder"), true, move || {
            open_folder(&folder_copy, &name_copy);
        });
        chrome::popup_separator(popup);
        let name_copy = torrent_name.clone();
        chrome::popup_add_action(popup, &t("remove_from_list"), true, move || {
            confirm_remove(
                window_ptr,
                torrent_id,
                &name_copy,
                tx.clone(),
                rpc_url.clone(),
            );
        });
    });
}

fn show_files(window_ptr: usize, name: &str, torrent_id: i64, theme: &str, rpc_url: String) {
    let files = TransmissionRPC::new(&rpc_url)
        .ok()
        .and_then(|rpc| rpc.get_torrent_files(torrent_id).ok())
        .unwrap_or_default();
    chrome::files_exec(
        window_ptr,
        &stylesheet(theme),
        name,
        &files,
        move |index, wanted, priority| {
            let result = TransmissionRPC::new(&rpc_url).and_then(|rpc| {
                rpc.set_file_priority(torrent_id, i64::from(index), wanted != 0, i64::from(priority))
                    .map_err(|err| err.0)
            });
            match result {
                Ok(()) => 0,
                Err(err) if rpc_method_unsupported(&err) => 1,
                Err(_) => -1,
            }
        },
    );
}

struct RawParent(usize);

impl qtrs::AsWidget for RawParent {
    fn widget_ptr(&self) -> *mut qtrs::ffi::QWidget {
        self.0 as *mut qtrs::ffi::QWidget
    }
    fn set_has_parent(&mut self) {}
}

fn confirm_remove(
    window_ptr: usize,
    torrent_id: i64,
    name: &str,
    tx: Sender<WorkerMsg>,
    rpc_url: String,
) {
    let accepted = chrome::confirm_remove(
        &RawParent(window_ptr),
        &t("remove_title"),
        &t_args("remove_text", &[("name", name)]),
        &t("remove_data"),
        &t("yes"),
        &t("cancel"),
    );
    let Some(delete_data) = accepted else {
        return;
    };
    thread::spawn(move || {
        let outcome = TransmissionRPC::new(&rpc_url)
            .map_err(|e| e)
            .and_then(|rpc| rpc.remove_torrent(torrent_id, delete_data).map_err(|e| e.0));
        let _ = tx.send(WorkerMsg::Removed(outcome));
    });
}

fn open_folder(root: &Path, name: &str) {
    let mut target = root.join(name);
    if !target.exists() {
        let part = PathBuf::from(format!("{}.part", target.display()));
        target = if part.exists() {
            part
        } else {
            root.to_path_buf()
        };
    }
    if target.is_file() {
        if let Some(parent) = target.parent() {
            target = parent.to_path_buf();
        }
    }
    let url = format!("file://{}", target.display());
    let _ = desktopservices::open_url(&url);
}

fn spawn_refresh(g: &mut Gui) {
    if g.busy || g.adding {
        return;
    }
    g.busy = true;
    g.status_mode = "updating".into();
    set_status(g);
    let tx = g.tx.clone();
    let endpoint = g.settings.rpc_url.clone();
    let detailed = g.settings.torrent_view != "simple";
    thread::spawn(move || {
        let result = TransmissionRPC::new(&endpoint)
            .map_err(|e| e)
            .and_then(|rpc| rpc.get_torrents(detailed).map_err(|e| e.0));
        let _ = tx.send(WorkerMsg::Torrents(result));
    });
}

fn open_torrent_file(gui: &Rc<RefCell<Gui>>) {
    let parent = match gui.try_borrow() {
        Ok(g) => {
            if g.adding {
                return;
            }
            g.window.widget_ptr() as usize
        }
        Err(_) => {
            let gui = gui.clone();
            chrome::defer(move || open_torrent_file(&gui));
            return;
        }
    };
    let filter = format!("{} (*.torrent);;{} (*)", t("torrent_files"), t("all_files"));
    let Some(path) = chrome::open_file(parent, &t("add_title"), &filter) else {
        return;
    };
    let mut g = gui.borrow_mut();
    start_add(&mut g, &path);
}

fn start_add(g: &mut Gui, source: &str) {
    let source = source.trim().to_string();
    if source.is_empty() || !Path::new(&source).is_file() {
        warning(
            Some(&g.window),
            &t("file_not_found"),
            &t("file_not_found_text"),
        );
        return;
    }
    let rpc_online = g.status_mode == "online";
    g.adding = true;
    g.status_mode = "copying".into();
    set_status(g);
    let tx = g.tx.clone();
    let settings = g.settings.clone();
    thread::spawn(move || {
        let result = import_source(&source, &settings, rpc_online);
        let _ = tx.send(WorkerMsg::Added(result));
    });
}

fn import_source(
    source: &str,
    settings: &AppSettings,
    rpc_online: bool,
) -> Result<Option<String>, String> {
    let content = std::fs::read(source).map_err(|e| e.to_string())?;
    if content.is_empty() {
        return Err(t("rpc_empty_file"));
    }
    if rpc_online {
        let rpc = TransmissionRPC::new(&settings.rpc_url)?;
        rpc.add_torrent_bytes(&content).map_err(|e| e.0)?;
        return Ok(None);
    }
    let filename = Path::new(source)
        .file_name()
        .map(|n| n.to_string_lossy().into_owned())
        .unwrap_or_else(|| "download.torrent".into());
    let mut settings = settings.clone();
    let dest = settings
        .torrents_path()
        .map_err(|e| e.to_string())?
        .join(&filename);
    std::fs::write(&dest, &content).map_err(|e| e.to_string())?;
    Ok(Some(dest.display().to_string()))
}

fn open_about(gui: &Rc<RefCell<Gui>>) {
    let (parent, theme) = match gui.try_borrow() {
        Ok(g) => (g.window.widget_ptr() as usize, g.settings.theme.clone()),
        Err(_) => {
            let gui = gui.clone();
            chrome::defer(move || open_about(&gui));
            return;
        }
    };
    chrome::about_exec(parent, &stylesheet(&theme));
}

fn open_settings(gui: &Rc<RefCell<Gui>>) {
    let previous = language();
    let (current, parent) = match gui.try_borrow() {
        Ok(g) => (g.settings.clone(), g.window.widget_ptr() as usize),
        Err(_) => {
            let gui = gui.clone();
            chrome::defer(move || open_settings(&gui));
            return;
        }
    };
    let Some(result) = chrome::settings_exec(parent, &stylesheet(&current.theme), &current) else {
        set_language(&previous);
        return;
    };
    if let Err(err) = normalize_rpc_url(&result.rpc_url) {
        let g = gui.borrow();
        warning(Some(&g.window), &t("invalid_address"), &err);
        set_language(&previous);
        return;
    }
    if result.torrents_dir.trim().is_empty() {
        let g = gui.borrow();
        warning(
            Some(&g.window),
            &t("invalid_directory"),
            &t("need_torrents_dir"),
        );
        set_language(&previous);
        return;
    }
    let mut g = gui.borrow_mut();
    g.settings.rpc_url = result.rpc_url;
    g.settings.torrents_dir = result.torrents_dir;
    g.settings.refresh_seconds = result.refresh_seconds;
    g.settings.language = result.language;
    g.settings.theme = result.theme;
    g.settings.torrent_view = result.torrent_view;
    if g.settings.torrent_view.is_empty() {
        g.settings.torrent_view = "detailed".into();
    }
    if g.settings.theme != "night" {
        g.settings.theme = "light".into();
    }
    if g.settings.language != "ru" {
        g.settings.language = "en".into();
    }
    set_language(&g.settings.language);
    if let Err(err) = g.settings.torrents_path() {
        warning(
            Some(&g.window),
            &t("torrents_directory"),
            &t_args("torrents_dir_error", &[("error", &err.to_string())]),
        );
    }
    let width = g.window.width();
    let height = g.window.height();
    g.settings.capture_window_size(width, height);
    let _ = g.settings.save();
    g.timer
        .start((g.settings.refresh_seconds.max(2) * 1000) as i32);
    apply_chrome(&mut g);
    render_cards(&mut g);
    spawn_refresh(&mut g);
}
