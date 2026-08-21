# Cache Firmware

Cache Firmware is a standalone MeshCore Room Server firmware project. Its
first release is intentionally bare-bones: ordinary upstream Room Server
behaviour, packaged and maintained as a separate project so cache-specific
features can be added carefully later.

**Cache Firmware** is the name of the reusable firmware. **Recursive Cache**
is the working name of the first geocache installation; it is not the firmware
or repository name.

The first radio board has not been selected yet. Cache Firmware will initially
store and distribute Room Server posts using the standard MeshCore client
protocol.

## Current scope

- Upstream MeshCore Room Server behaviour.
- Firmware identity `Cache v1.17.1-C1`.
- Default node name `Cache`.
- Upstream MeshCore radio defaults.
- Standard initial administrator password `password`.
- Standard initial room password `hello`.
- Hardware target and image formats to be chosen with the first radio board.

Change both public development passwords during provisioning. Normal
application-image updates preserve stored device configuration. A merged image
is intended for a clean/full installation and uses the compiled defaults.

## Not included yet

Cache Firmware does not yet enforce direct or zero-hop access and does not add
puzzles, hints, finder records, geocache commands, or channel announcements.

## Build

The dedicated Cache build target and GitHub Actions workflow will be added
after the first radio board is selected.

## Configure

Use [config.meshcore.io](https://config.meshcore.io) to set the regional radio
parameters, node identity, room password, and a unique administrator password.

## Upstream

Cache Firmware is based on [MeshCore](https://github.com/meshcore-dev/MeshCore)
and preserves its Git history and MIT licence. Cache-specific changes are
maintained independently in this repository.
