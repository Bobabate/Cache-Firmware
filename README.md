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
location-based radio caches. Visitors connect directly over the mesh, read the
recent finder log, and leave one log entry every 24 hours.

The current release is **v1.17.1-C5**.

## Supported hardware

- **Heltec MeshPocket:** complete interactive cache behaviour.
- **Heltec WiFi LoRa 32 V3:** Cache-branded Room Server baseline. Interactive
  finder-log behaviour is not enabled on this board.

Release packages contain MeshPocket UF2/ZIP files and Heltec V3
application/merged BIN files.

## How the cache works

- Access must arrive directly by radio; routed and repeater connections are
  rejected.
- The newest three log entries are shown when requested, with commands for
  paging through older entries.
- Up to 100 entries are retained in flash across reboots and power loss.
- Visitors may leave one entry every 24 hours and may edit their recent entry
  without increasing the find count.
- An optional calibrated RSSI boundary can require visitors to be physically
  closer to the cache.
- The first saved administrator receives a best-effort private message when a
  new find is accepted.

## Visitor commands

Send these as ordinary messages in the direct conversation with the cache:

- `!help` — show visitor instructions and available commands.
- `!found` — show the current number of stored finds.
- `!older` — show the next three older log entries.
- `!newer` — move three entries toward the newest page.
- `!latest` — return to the newest three entries.
- `!edit <new log entry>` — replace your recent entry without adding another
  find.

Any other ordinary message is treated as a new log entry. Entries may contain
at most 151 characters.

## Administrator commands

Send these through the authenticated Room Server CLI:

- `cache clear` — clear log entries, the find count, posting cooldowns, and
  visitor synchronization positions. It preserves identity, radio settings,
  passwords, learned visitor names, and RSSI calibration.
- `cache limit` — show the current posting interval.
- `cache limit <hours>` — set the posting interval from 1 to 168 hours.
- `cache limit off` — disable the posting interval.
- `rssi` — report the current median RSSI from the requesting radio without
  changing calibration.
- `rssi status` — show the saved near, far, and access-limit values.
- `rssi near` — save a reading taken beside the cache.
- `rssi far` — save a reading at the farthest location where access should be
  allowed and activate the RSSI boundary.
- `rssi reset` — clear RSSI calibration and disable RSSI filtering. This works
  over USB or from a directly connected authenticated administrator.

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
