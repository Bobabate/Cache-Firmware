# Cache Firmware

## Start here

**Looking for firmware to install?** Go to
**[Cache Firmware Releases](https://github.com/Bobabate/Cache-Firmware/releases)**.
Do not use GitHub's green **Code** button; it downloads source code, not
installable firmware.

- **Download firmware:** [open Releases](https://github.com/Bobabate/Cache-Firmware/releases)
- **Install or update:** [read START-HERE.md](START-HERE.md)
- **Configure:** [jump to configuration](#configure)
- **Understand the project:** [read CACHE.md](CACHE.md)
- **Review changes:** [read the changelog](CHANGELOG.md)

---

Cache Firmware is a standalone MeshCore Room Server firmware project for
location-based radio caches. It began with ordinary upstream Room Server
behaviour and is adding the cache interaction carefully, one board at a time.

The current cache interaction is available on the Heltec MeshPocket only. The
Heltec WiFi LoRa 32 V3 remains at the original C1 Room Server behaviour.

## Current scope

- Upstream MeshCore Room Server behaviour.
- Dedicated MeshPocket and Heltec V3 build targets.
- Firmware identity `Cache v1.17.1-C5`.
- Ordinary upstream Room Server node names on clean installation; set the
  cache's real name during configuration.
- Upstream MeshCore radio defaults.
- Standard initial administrator password `password`.
- Standard initial room password `hello`.
- MeshPocket UF2/ZIP packages and Heltec V3 application/merged BIN packages.

The current MeshPocket release adds:

- up to 100 posts preserved across ordinary reboots and power loss;
- no automatic delivery of newly added posts to other clients;
- a short instruction message when a visitor connects;
- the newest three posts, with `!older`, `!newer`, and `!latest` navigation;
- `!help`, `!found`, and `!edit <text>` visitor commands;
- a clear duplicate-entry warning instead of silently treating a second entry
  as a new find;
- a private best-effort notification to the first saved administrator when a
  new visitor find is accepted;
- direct-radio-only access with no routed or repeater connections; and
- an optional calibrated RSSI limit for controlling the usable distance.

The current release is **v1.17.1-C5** for MeshPocket and Heltec V3. Cache
interaction remains MeshPocket-only; the Heltec V3 package retains the Cache
Room Server baseline behaviour.

Change both public development passwords during provisioning. Normal
application-image updates preserve stored device configuration. A merged image
is intended for a clean/full installation and uses the compiled defaults.

## RSSI calibration

After connecting directly as administrator, send `rssi` to inspect the current
median direct-radio reading without changing calibration. Stand beside the
cache and send `rssi near`, then move to the farthest location that should work
and send `rssi far`. The cache saves the resulting limit across reboots. Send
`rssi status` to view the saved readings. `rssi reset` is available over USB or
from a directly connected administrator.

The RSSI gate remains inactive until both calibration points are recorded.
Direct-radio-only access is always enforced and is independent of RSSI.

## Not included yet

Cache Firmware does not yet add puzzles, hints, structured finder records, or
channel announcements.

## Build

Install PlatformIO and run:

```sh
FIRMWARE_VERSION=v1.17.1-C5 ./build.sh build-firmware \
  Cache_mesh_pocket_room_server

FIRMWARE_VERSION=v1.17.1-C5 ./build.sh build-firmware \
  Cache_heltec_v3_room_server
```

GitHub Actions can test either board or both. A version tag builds and publishes
both board packages.

## Configure

Use [config.meshcore.io](https://config.meshcore.io) to set the regional radio
parameters, node identity, room password, and a unique administrator password.

## Upstream

Cache Firmware is based on [MeshCore](https://github.com/meshcore-dev/MeshCore)
and preserves its Git history and MIT licence. Cache-specific changes are
maintained independently in this repository.
