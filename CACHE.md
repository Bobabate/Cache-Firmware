# Cache Firmware design notes

## Naming

- **Cache Firmware** is the reusable firmware project.
- **Recursive Cache** is the working name of the first geocache installation
  used in design conversations and, later, device configuration.
- Future installations can use different cache names without requiring a new
  firmware project.

## Recursive Cache installation

Recursive Cache will be located near Toronto's “Recursive History” plaque and
the nearby bench. The plaque provides the theme and the bench provides a
natural place for a visitor to connect to the radio.

The initial experience remains ordinary Room Server interaction: visitors can
read the shared room and leave a post. Each post becomes another entry in the
location's continuing history, extending the plaque's recursive idea without
requiring a puzzle or separate game system.

The exact radio placement, power, enclosure, antenna, and permission to install
equipment remain open design decisions.

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
