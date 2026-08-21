# Cache Firmware

## Start here

**Looking for firmware to install?** Go to
**[Cache Firmware Releases](https://github.com/dchant/Cache-Firmware/releases)**.
Do not use GitHub's green **Code** button; it downloads source code, not
installable firmware.

- **Download firmware:** [open Releases](https://github.com/dchant/Cache-Firmware/releases)
- **Install or update:** [read START-HERE.md](START-HERE.md)
- **Configure:** [jump to configuration](#configure)
- **Understand the project:** [read CACHE.md](CACHE.md)
- **Review changes:** [read the changelog](CHANGELOG.md)

---

Cache Firmware is a standalone MeshCore Room Server firmware project. Its
first release is intentionally bare-bones: ordinary upstream Room Server
behaviour, packaged and maintained as a separate project so cache-specific
features can be added carefully later.

**Cache Firmware** is the name of the reusable firmware. **Recursive Cache**
is the working name of the first geocache installation; it is not the firmware
or repository name.

The first targets are the Heltec MeshPocket and Heltec WiFi LoRa 32 V3. Cache
Firmware initially stores and distributes Room Server posts using the standard
MeshCore client protocol.

## Current scope

- Upstream MeshCore Room Server behaviour.
- Dedicated MeshPocket and Heltec V3 build targets.
- Firmware identity `Cache v1.17.1-C1`.
- Ordinary upstream Room Server node names on clean installation; set the
  cache's real name during configuration.
- Upstream MeshCore radio defaults.
- Standard initial administrator password `password`.
- Standard initial room password `hello`.
- MeshPocket UF2/ZIP and Heltec V3 application/full-install BIN packages.

The current release is **v1.17.1-C1**.

Change both public development passwords during provisioning. Normal
application-image updates preserve stored device configuration. A merged image
is intended for a clean/full installation and uses the compiled defaults.

## Not included yet

Cache Firmware does not yet enforce direct or zero-hop access and does not add
puzzles, hints, finder records, geocache commands, or channel announcements.

## Build

Install PlatformIO and run:

```sh
FIRMWARE_VERSION=v1.17.1-C1 ./build.sh build-firmware \
  Cache_mesh_pocket_room_server Cache_heltec_v3_room_server
```

GitHub Actions can build either board or both. Version tags build both boards
and publish their firmware packages as a permanent GitHub Release.

## Configure

Use [config.meshcore.io](https://config.meshcore.io) to set the regional radio
parameters, node identity, room password, and a unique administrator password.

## Upstream

Cache Firmware is based on [MeshCore](https://github.com/meshcore-dev/MeshCore)
and preserves its Git history and MIT licence. Cache-specific changes are
maintained independently in this repository.
