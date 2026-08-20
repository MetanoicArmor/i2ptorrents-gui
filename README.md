# I2P Torrents GUI

Кроссплатформенный настольный клиент для встроенного torrent-клиента i2pd. Интерфейс выполнен в визуальном стиле [I2PChat-ng](https://github.com/MetanoicArmor/I2PChat-ng): светлая и ночная темы, карточки, компактная боковая панель и нативное поведение на Linux, Windows и macOS.

## Возможности

- список торрентов, прогресс, скорости, пиры и статусы;
- добавление `.torrent` через Transmission RPC;
- удаление торрента отдельно или вместе с загруженными данными;
- поиск и фильтры загрузок/раздач;
- автоматическое обновление и диагностика соединения;
- fallback без RPC: копирование `.torrent` в `torrentsdir`;
- сохранение адреса RPC, каталога и темы.

## Настройка i2pd

Добавьте в `tunnels.conf`:

```ini
[MyTorrents]
type=torrents
trackers=http://tracker2.postman.i2p/announce.php
torrentsdir=/home/i2pd/torrents
rpcport=9191
rpcpath=mytorrents
```

Трекер менять не следует. RPC требует сборку i2pd с Boost 1.81 или новее. В GUI укажите привычный адрес:

```text
http://127.0.0.1:9191/mytorrents
```

Приложение само преобразует его в фактический endpoint `/mytorrents/rpc/`.
Поскольку RPC i2pd не поддерживает авторизацию и TLS, GUI намеренно разрешает
подключение только к loopback-адресам (`localhost`, `127.0.0.0/8`, `::1`).

## Запуск из исходников

Требуется Python 3.10+.

```bash
python -m venv .venv
source .venv/bin/activate       # Windows: .venv\Scripts\activate
pip install -e .
i2ptorrents-gui
```

## Сборка

```bash
pip install -e . pyinstaller
pyinstaller --noconfirm --windowed --name I2PTorrents run_gui.py
```

## Ограничения i2pd RPC

Текущая реализация i2pd предоставляет только `torrent-add`, `torrent-get` и `torrent-remove`. Пауза, возобновление, изменение трекеров и лимитов скорости пока не поддерживаются на стороне i2pd.

## Лицензия

GNU AGPL v3 или более поздняя версия.