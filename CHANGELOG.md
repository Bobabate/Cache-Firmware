# Changelog

## 1.17.1-C6 - 2026-08-21

- Reorder the interactive MeshPocket display with the configured cache name at
  the top, a double-size `Finds: N` count, battery information below the count,
  and the complete firmware version on the bottom line.
- Publish MeshPocket UF2 and ZIP packages only. Heltec V3 remains at
  `v1.17.1-C5`.

## 1.17.1-C5 - 2026-08-21

- Acknowledge accepted entries with their find number and congratulate the
  first finder with a distinct message.
- Simplify first-administrator notifications to `Cache found. Total finds: N.`
- Make `rssi` report the requesting radio's current median RSSI without saving
  it, and move saved calibration details to `rssi status`.
- Allow an authenticated administrator to run `rssi reset` over a direct radio
  connection while retaining USB as the physical recovery path.
- Update repository, release, installation, and website links for the Bobabate
  GitHub username.
- Publish current packages for both MeshPocket and Heltec V3. Interactive cache
  behaviour remains MeshPocket-only; Heltec V3 retains baseline Room Server
  behaviour.

## 1.17.1-C4 - 2026-08-21

- Fix generated visitor replies so `!help` and posting-limit warnings display
  as ordinary messages in companion clients.
- Warn repeat visitors that a recent log entry already exists and add
  `!edit <text>` to replace it without increasing the find count.
- Add `!found` to report the persistent find count shown on the MeshPocket.
- Send a private, best-effort notification to the first saved administrator
  when a new visitor find is accepted.
- Add focused tests for successful, missing, overlength, absent-entry, and
  non-command `!edit` parsing.

## 1.17.1-C3 - 2026-08-21

- Send welcome instructions only on first login and add `!help`.
- Return only unread entries on later logins, three at a time; keep `!latest`
  as the explicit replay command.
- Learn visitor names from direct companion adverts and prefix persistent log
  entries with the learned name or a short public-key fallback.
- Add a persistent one-entry-per-24-hours visitor limit with administrator
  exemption and authenticated `cache limit` controls.
- Add authenticated `cache clear` to clear posts, counters, cooldowns, and sync
  positions without erasing identity, configuration, names, or RSSI settings.
- Show `Cache found N times` on the MeshPocket display.
- Reply to valid zero-hop companion adverts with a zero-hop Cache advert after
  five seconds during testing.

## 1.17.1-C2 - 2026-08-20

- Remove the installation-specific `Recursive Cache` node name from firmware
  defaults. Clean installs now use each board's upstream Room Server name until
  the owner configures the actual cache name; configured names still appear on
  the display.
- Add MeshPocket-only cache interaction: persist up to 100 posts using
  alternating checksummed snapshots, suppress automatic live post fan-out, and
  show a temporary instruction message followed by three posts at a time.
- Add MeshPocket post navigation commands: `!older`, `!newer`, and `!latest`.
- Require direct, zero-hop radio access for the MeshPocket cache interaction.
- Add persistent optional RSSI calibration with `rssi near`, `rssi far`,
  `rssi`, and USB-only `rssi reset` commands.
- Leave the Heltec V3 target on the existing C1 Room Server behaviour.

## 1.17.1-C1 - 2026-08-20

- Start Cache Firmware from upstream MeshCore v1.17.1 Room Server.
- Add a dedicated Heltec MeshPocket Room Server target named Recursive Cache.
- Add a dedicated Heltec WiFi LoRa 32 V3 Room Server target.
- Retain standard Room Server behaviour, radio defaults, and initial passwords.
- Add a manual GitHub Actions test-build workflow producing UF2 and ZIP
  packages for MeshPocket and BIN packages for Heltec V3, with tagged releases.
- Add a persistent MeshPocket screen showing the complete firmware version,
  configured cache name, and battery percentage and voltage.
