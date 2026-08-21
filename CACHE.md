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

The first hardware target is the Heltec MeshPocket. Its e-ink display, Nordic
nRF52840 platform, radio configuration, and power behaviour are inherited from
the upstream MeshCore Room Server target.

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
