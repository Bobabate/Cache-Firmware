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
