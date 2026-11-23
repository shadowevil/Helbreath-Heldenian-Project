# Logging Reference

*Assessed 2026-11-23 (docs standard sect. 1): kept whole, about 3 KB. One small reference read before adding logging.*

Contents: Files - How to Include - Channel Behavior - Server Channels - Client Channels - Initialization / Shutdown.

Shared logging system in `sources/dependencies/shared/log/`. Uses `std::format` and multi-channel file output.

**Client and Server targets only.** The `shared/` library itself must not depend on or use the logging system — it has no knowledge of backends or channels. If diagnostic output is needed in Shared code, use `std::printf` or standard I/O (`<cstdio>`, `<iostream>`).

## Files

Paths under `log/` are in `sources/dependencies/shared/`; `server/` and `client/` here mean `sources/server/` and `sources/client/` (the channel headers sit directly in those folders).

| File | Purpose |
|------|---------|
| `log/log.h` | **Public API** — include this to log. Provides `hb::logger::error`, `warn`, `log`, `debug` |
| `log/log_level.h` | Level constants (`error=0, warn=1, log=2, debug=3`) and `level_name()` |
| `log/log_backend.h` | `log_backend` base class — multi-channel file writer, virtual `write_console()` |
| `log/log_backend.cpp` | Implementation — timestamp formatting, per-channel + main dual-write, mutex-protected |
| `server/server_log_channels.h` | Server channel enum (15 channels: main, events, security, pvp, network, chat, etc.) |
| `server/server_log.cpp` | Server wiring — `server_log_backend` subclass routes to `ServerConsole` |
| `client/client_log_channels.h` | Client channel enum (2 channels: main, network) |
| `client/client_log.cpp` | Client wiring — console output only in `_DEBUG` builds |

## How to Include

**Basic logging (channel 0 / main)** — only need `log.h`:
```cpp
#include "log.h"

hb::logger::error("SQLite open failed: {}", sqlite3_errmsg(db));
hb::logger::warn("Unexpected value: {}", val);
hb::logger::log("Player '{}' connected", name);
hb::logger::debug("Tick took {}ms", elapsed);
```

**Logging to a specific channel** — also include the channels header and `using` declaration:
```cpp
#include "log.h"
#include "server_log_channels.h"   // or "client_log_channels.h"

using hb::log_channel;

hb::logger::warn<log_channel::security>("Swing hack: IP={} player={}", ip, name);
hb::logger::log<log_channel::chat>("[ChatMsg] {}: {}", sender, message);
hb::logger::log<log_channel::events>("Player '{}' crafting '{}'", name, item);
```

## Channel Behavior

- Channel 0 (`main`) is the default when no template argument is given.
- Non-zero channels write to **both** their own log file and the main log file.
- Each channel maps to a separate `.log` file (e.g. `security` → `hackevents.log`, `chat` → `chat.log`).
- Format: `[HH:MM:SS.mmm] [LEVEL] [channel_name] message` (channel name omitted for main).

## Server Channels (`hb::log_channel`)

`main`, `events`, `security`, `pvp`, `network`, `log_events`, `chat`, `commands`, `drops`, `trade`, `shop`, `crafting`, `upgrades`, `bank`, `items_misc`

## Client Channels (`hb::log_channel`)

`main`, `network`

## Initialization / Shutdown

Already wired in both targets — do not call manually:
- **Server**: `hb::logger::initialize("gamelogs/")` in `wmain.cpp`, `shutdown()` on exit.
- **Client**: `hb::logger::initialize("logs")` in `game.cpp`, `shutdown()` on exit.
