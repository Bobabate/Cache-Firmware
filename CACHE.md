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

## Version 1 scope

Version 1 is a branded, buildable MeshCore Room Server baseline. It intentionally
adds no cache-specific interaction rules. This gives the project a small,
testable starting point before access controls or game behaviour are designed.

The first hardware targets are the Heltec MeshPocket and Heltec WiFi LoRa 32
V3. Their display, radio configuration, and power behaviour are inherited from
the corresponding upstream MeshCore Room Server targets.

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
- Treat merged images as clean/full installations.
- Add hardware targets individually and validate each through GitHub Actions.

## Deferred ideas

Direct-only access, zero-hop enforcement, a finder log, cache descriptions,
hints, puzzles, and optional channel announcements are possible future features.
They are not requirements for the baseline firmware.
