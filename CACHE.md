# Cache Firmware design notes

## Naming

- **Cache Firmware** is the reusable firmware project.
- **Recursive Cache** is the working name of the first geocache installation
  used in design conversations and, later, device configuration.
- Future installations can use different cache names without requiring a new
  firmware project.

## Version 1 scope

Version 1 is a branded, buildable MeshCore Room Server baseline. It intentionally
adds no cache-specific interaction rules. This gives the project a small,
testable starting point before access controls or game behaviour are designed.

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
