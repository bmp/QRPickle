# Data flow: what QRPickle downloads, when, and where it keeps it

Every piece of data on the screens comes from one of the jobs below. This page says when each job
runs, what stops it, where its data is kept and which screens use it. The table is generated from
`src/services/sources.h` (`scripts/gen_data_flow.py`), and the build fails when the two disagree
(`scripts/check_data_flow.py`), so the numbers here are the ones the firmware uses.

Changing a download, a task, a list or how a screen fetches means updating this page in the same
commit (`CLAUDE.md`, `docs/RELEASING.md` step 1b). Memory budgets and rules: `docs/MEMORY.md`.

## Three rules every download follows

1. **One network session at a time.** Every connect and HTTP(S) request holds `services::NetLock`
   (`net_lock.h`). Two TLS sessions never fit in memory together.
2. **Start only when the network is free; check memory before TLS.** Scheduled downloads don't
   create their task while another download holds the network (`net_lock_busy()`), so two tasks'
   memory peaks don't add up; they try again on the next tick. A TLS download then starts only
   when enough memory is free (`tls_heap_ready()`), else it is postponed and retried.
3. **The main loop never waits for the network.** Weather runs on the main loop and skips its turn
   when the network is busy (the loop watchdog fires at 30 s); everything else runs in a task.

A download that never returns can't keep the board off the network: after 60 s holding `NetLock`
the log warns, after 180 s the board restarts and `/api/status` names the task (`net_lock_watch()`).

## Jobs

<!-- BEGIN generated: sources (scripts/gen_data_flow.py) -->
| Job | Connection | Refresh | Retry | Task | Kept in | Screens |
|---|---|---|---|---|---|---|
| weather | OpenWeather, HTTPS | every 2 h; every 3 min within 45 min of sunrise or sunset | 5 min after a failure; 30 s if the network is busy | main loop | RAM (current, forecast) | Weather, Dashboard |
| solar | hamqsl.com or own URL, HTTPS | 15 min after each 3-hourly update, at most hourly; not in the first minute after boot | 15 min; next tick if the network is busy | `solar_fetch`, 6 KB | RAM, copy kept over restarts | Band conditions, Dashboard |
| feed | GitHub (raw), HTTPS | 4 min after boot, then every 6 h | 15 min; next tick if the network is busy | `space_fetch`, 6 KB | flash file /space.json | Satellites |
| pota | api.pota.app, HTTPS | on an xOTA visit: opened by hand if older than 30 s, by cycling if older than 5 min | next visit; waits while the network is busy | `pota_fetch`, 6 KB | RAM list (30), freed when no screen shows it | xOTA (POTA tab) |
| sota | SOTA cluster, telnet | live while the SOTA tab is open; a cycling visit reconnects if older than 10 min | 30 s; silence 4 h; TCP keepalive | `sota_cluster`, 4 KB | RAM list (30), freed when no screen shows it | xOTA (SOTA tab) |
| dx | DX cluster, telnet | live while the Spots page is open or DX is cycled; read on the main loop | 30 s while the Spots page is open; silence 15 min; TCP keepalive | `dx_dial`, 4 KB | RAM list (50), freed when no screen shows it | Spots |
| hamalert | hamalert.org, telnet | always connected | 30 s; silence 4 h; TCP keepalive | `hamalert_task`, 4 KB | RAM (10 alerts) | HamAlert |
| aprs | APRS-IS, TCP | always connected when switched on | 5 s; silence 3 min; TCP keepalive | `aprs_task`, 4.5 KB | RAM (30 stations, 10 messages) | APRS screens |
| ota_check | GitHub Pages, HTTPS (certificate checked) | 2 min after boot, then every 10 min until one succeeds; a manual check at once | 30 s if the network is busy | `gh_ota_check`, 9 KB | RAM (version, notes) | Cloud OTA |
| passes | none (SGP4 from the feed) | every 30 min, and when the feed or the location changes | next tick if the network is busy | `space_passes`, 6 KB | RAM | Satellites, Dashboard |
| ota_install | GitHub Pages, HTTPS (certificate checked) | on request (Cloud OTA screen or web console) | none (the trial boot rolls back a bad image) | `ota_flash_worker`, 12 KB | the other app slot | Cloud OTA |
<!-- END generated: sources -->

"Silence" is the dead-link limit: a connection that received nothing for that long is closed and
reconnected (`link_watch.h`). TCP keepalive finds a vanished peer after about 90 s on its own.

## Opening a screen by hand and screen cycling

xOTA has one rule for both (`src/ui/xota_fetch.h`); only the age limit differs:

| | Opened by hand | Visited by screen cycling |
|---|---|---|
| POTA | downloads if the list is older than 30 s | downloads if older than 5 min, else shows the held list |
| SOTA | connects to the cluster at once | reconnects if older than 10 min |
| DX (Spots page) | connects; closes when you leave | stays connected the whole time DX is in the cycle |

A visit by hand closes the DX connection first (its buffered spots take memory) unless cycling
keeps DX connected. HamAlert and APRS-IS stay connected throughout: their reconnects used to split
the free memory (`docs/MEMORY.md` rule 5).

## Lists and their lifetime

The POTA, SOTA and DX lists (30, 30 and 50 rows; 3.4-3.8 KB each) are allocated when their screen
first needs them and freed a few seconds after no screen shows them and cycling doesn't come back
to them (`services/list_release.h`, a 5 s timer in `src/ui/screen_cycle.cpp`). The POTA and SOTA
lists are allocated only while no download runs. HamAlert (10 alerts) and APRS (30 stations, 10
messages) keep fixed lists while they run. No list grows: every one has a fixed number of rows.

## Freshness

Each data source records its last success and failures in a row (`services/fresh_registry.h`).
`/api/status` reports them under `fresh`:

- `FRESH`: a success within twice the source's normal interval (the table's refresh);
- `STALE`: older than that;
- `FAILING`: three or more failures in a row and nothing fresh;
- `NONE`: nothing yet.

Band conditions, Satellites, Weather (OpenWeather tab) and xOTA (POTA tab) show the age of their
data on screen and turn it to the warning colour when stale. The test scripts (`tools/cycle_run.sh`)
print the table at the end of a run and fail when a source is `FAILING`.
