# Cache Firmware design notes

## Naming

- **Cache Firmware** is the reusable firmware project.
- Installations can use different cache names without requiring a new
  firmware project.

## C1 baseline

C1 is a branded, buildable MeshCore Room Server baseline. It intentionally adds
no cache-specific interaction rules.

The first hardware targets are the Heltec MeshPocket and Heltec WiFi LoRa 32
V3. Their display, radio configuration, and power behaviour are inherited from
the corresponding upstream MeshCore Room Server targets.

## Next MeshPocket version

The next version is intentionally limited to the MeshPocket. Its cache
interaction is:

- Store up to 100 posts in local flash and restore them after reboot or power
  loss. Alternating, checksummed snapshots protect the previous valid copy if a
  write is interrupted.
- Do not automatically send a new post to connected or previously known
  clients. Posts are returned only as part of a visitor's requested page.
- Send a temporary instruction message first, followed by the newest three
  posts. The instruction is not stored as a post.
- Use `!older`, `!newer`, and `!latest` to move through posts three at a time.
- Accept cache access only over a direct radio path. Routed and repeater paths
  are rejected independently of all other settings.
- Optionally require a calibrated minimum RSSI at login. Until calibration is
  complete, direct access remains available so the owner can configure it.
- Calibrate with `rssi near` beside the cache and `rssi far` at the desired
  boundary. `rssi` reports the saved values, and USB-only `rssi reset` restores
  an uncalibrated state.

The RSSI limit is set three dB below the measured far point to tolerate modest
signal variation. Real-world access will still vary with antenna orientation,
obstructions, weather, and the visitor's radio.

## Required cache display

Cache Firmware keeps a simple, persistent screen showing:

- the complete Cache Firmware name and version;
- the configured cache name; and
- the battery level as both a percentage and voltage.

These are permanent Cache Firmware requirements and must not be removed. The
display remains visible and updates the battery reading once per minute. This
Cache-specific screen does not change the upstream Room Server display.

## Compatibility policy

- Stay close to upstream MeshCore Room Server.
- Keep cache-specific changes isolated and easy to review.
- Preserve existing settings during application-image updates.
- Do not compile an installation-specific cache name into reusable firmware.
  Use the upstream board default until the owner configures a cache name.
- Continue showing the configured cache name on the device display.
- Treat merged images as clean/full installations.
- Add hardware targets individually and validate each through GitHub Actions.

## Deferred ideas

A structured finder log, cache descriptions, hints, puzzles, and optional
channel announcements remain possible future features.

## Todo

- [x] Fix generated `!help` and posting-limit replies by initializing their
  outgoing message type as `TXT_TYPE_PLAIN`, and add coverage confirming the
  companion displays both responses.
- [x] When a visitor who has already logged the cache during the current
  posting interval sends another ordinary message, do not add it as a new log
  entry. Reply with a warning that today's entry already exists and explain
  that `!edit <new log entry>` replaces it. Replacement text must obey the
  151-character limit, update the existing entry rather than create another,
  and leave the `Cache found N times` count unchanged. Add `!edit` to the
  `!help` response and test successful, missing-text, overlength, and no-entry
  cases.
- [x] Add `!found` to report how many times the cache has been found since its
  first stored find. Use the same persistent visitor-log count shown by
  `Cache found N times`, do not count temporary or system messages, and reset
  it with `cache clear`. Reply with `This cache has been found N times since
  its first find.` and include `!found` in the `!help` response.
- [x] When a new visitor log entry is accepted, send a private, best-effort
  notification to the first saved administrator: `Cache found by <name>. Total
  finds: N.` Do not notify for `!edit`, rejected duplicate entries, paging or
  help commands, or system messages. Notify only the first administrator to
  avoid duplicate mesh traffic; delivery does not need to be queued while the
  administrator is unreachable.

## C3 MeshPocket implementation

- Add an authenticated CLI command, `cache clear`, that clears all
  persistent posts, post counters, and visitor synchronization positions for
  testing. It must preserve the device identity, node name, radio settings,
  passwords, and RSSI calibration.
- Add a persistent author-name registry keyed by the visitor's MeshCore public
  key. Learn names automatically from valid direct companion adverts and use a
  short public-key fallback until a name is known. Log entries include the
  learned name instead of appearing only as `Unknown <key>` in the client.
  Registered names survive reboot and `cache clear`.
- Send the temporary welcome/instruction message only on a visitor's first
  login so repeated connections do not clutter the visitor's local MeshCore
  conversation. Add `!help` to request the instructions again on demand. The
  welcome remains temporary and must never consume a persistent post slot. It
  must clearly tell visitors that they may leave one log entry every 24 hours
  and that a log entry can contain at most 151 characters. Visitor-facing
  messages should say `log entry`, not `post`.
- Add a configurable posting limit, enabled by default at one log entry per
  visitor public key every 24 hours. Administrators are exempt, and help and
  paging commands do not count. Persist each visitor's last-entry time across
  reboot. A rejected entry should report the remaining wait time.
  Provide authenticated CLI commands to view, change, or disable the interval.
- On Cache Firmware boards with a display, show `Cache found N times` using
  the current number of persistent visitor posts. Do not count temporary
  welcome, instruction, or system messages. Reset the displayed count to zero
  when `cache clear` clears the post store.
- For testing, when Cache Firmware receives a valid zero-hop companion advert,
  send its own zero-hop advert in reply after a five-second delay. Do not add a
  per-node cooldown yet. Ignore self-adverts and restrict replies to companion
  adverts so Cache/Room/Repeater devices cannot create an advert-response loop.
