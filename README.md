# Cache Firmware

Cache Firmware is a standalone MeshCore Room Server firmware project. Its
first release is intentionally bare-bones: ordinary upstream Room Server
behaviour, packaged and maintained as a separate project so cache-specific
features can be added carefully later.

**Cache Firmware** is the name of the reusable firmware. **Recursive Cache**
is the working name of the first geocache installation; it is not the firmware
or repository name.

The first target is the Heltec MeshPocket. Cache Firmware initially stores and
distributes Room Server posts using the standard MeshCore client protocol.

## Current scope

- Upstream MeshCore Room Server behaviour.
- Dedicated `Cache_mesh_pocket_room_server` build target.
- Firmware identity `Cache v1.17.1-C1`.
- Default node name `Recursive Cache`.
- Upstream MeshCore radio defaults.
- Standard initial administrator password `password`.
- Standard initial room password `hello`.
- MeshPocket UF2 and ZIP firmware packages.

Change both public development passwords during provisioning. Normal
application-image updates preserve stored device configuration. A merged image
is intended for a clean/full installation and uses the compiled defaults.

## Not included yet

Cache Firmware does not yet enforce direct or zero-hop access and does not add
puzzles, hints, finder records, geocache commands, or channel announcements.

## Build

Install PlatformIO and run:

```sh
./build_cache_mesh_pocket.sh
```

The build produces UF2 and ZIP packages for the MeshPocket. GitHub Actions can
run the same build and provide temporary downloadable artifacts.

## Configure

Use [config.meshcore.io](https://config.meshcore.io) to set the regional radio
parameters, node identity, room password, and a unique administrator password.

## Upstream

Cache Firmware is based on [MeshCore](https://github.com/meshcore-dev/MeshCore)
and preserves its Git history and MIT licence. Cache-specific changes are
maintained independently in this repository.
