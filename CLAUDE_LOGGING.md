# Logging Reference

Shared logging system in `Sources/Dependencies/Shared/Log/`. Uses `std::format` and multi-channel file output.

**Client and Server targets only.** The `Shared/` library itself must not depend on or use the logging system — it has no knowledge of backends or channels. If diagnostic output is needed in Shared code, use `std::printf` or standard I/O (`<cstdio>`, `<iostream>`).

## Files

| File | Purpose |
|------|---------|
| `Log/Log.h` | **Public API** — include this to log. Provides `hb::logger::error`, `warn`, `log`, `debug` |
| `Log/LogLevel.h` | Level constants (`error=0, warn=1, log=2, debug=3`) and `level_name()` |
| `Log/LogBackend.h` | `log_backend` base class — multi-channel file writer, virtual `write_console()` |
| `Log/LogBackend.cpp` | Implementation — timestamp formatting, per-channel + main dual-write, mutex-protected |
| `Server/ServerLogChannels.h` | Server channel enum (15 channels: main, events, security, pvp, network, chat, etc.) |
| `Server/ServerLog.cpp` | Server wiring — `server_log_backend` subclass routes to `ServerConsole` |
| `Client/ClientLogChannels.h` | Client channel enum (2 channels: main, network) |
| `Client/ClientLog.cpp` | Client wiring — console output only in `_DEBUG` builds |

## How to Include

**Basic logging (channel 0 / main)** — only need `Log.h`:
```cpp
#include "Log.h"

hb::logger::error("SQLite open failed: {}", sqlite3_errmsg(db));
hb::logger::warn("Unexpected value: {}", val);
hb::logger::log("Player '{}' connected", name);
hb::logger::debug("Tick took {}ms", elapsed);
```

**Logging to a specific channel** — also include the channels header and `using` declaration:
```cpp
#include "Log.h"
#include "ServerLogChannels.h"   // or "ClientLogChannels.h"

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
- **Server**: `hb::logger::initialize("gamelogs/")` in `Wmain.cpp`, `shutdown()` on exit.
- **Client**: `hb::logger::initialize("logs")` in `Game.cpp`, `shutdown()` on exit.
